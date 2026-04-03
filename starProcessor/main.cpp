#include <iostream>
#include <thread>
#include <chrono>
#include <fcntl.h>    // O_RDONLY, O_RDWR
#include <sys/mman.h> // shm_open, mmap
#include <sys/stat.h> // fstat
#include <unistd.h>   // close
#include "captureAndShare_types.hpp"
#include <opencv2/opencv.hpp>
#include <utility>
#include <cmath>
#include <fstream> // For std::ofstream
#include "MQTTHandler.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

constexpr int max_considered_stars = 10;

double Gaussian2D(double x, double y, double sigma, double x0 = 0, double y0 = 0)
{
    return exp(-((x - x0) * (x - x0) + (y - y0) * (y - y0)) / (2 * sigma * sigma));
}

cv::Mat gaussianMask(int size, double sigma, double x_cent, double y_cent)
{
    cv::Mat kernel(size, size, CV_64F);
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            kernel.at<double>(i, j) = Gaussian2D(i, j, sigma, x_cent, y_cent);
        }
    }
    return kernel;
}

double rankStar(cv::Mat slice, double x, double y, double x_size, double y_size)
{
    cv::Mat slice_filt = slice.clone();
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(slice, &minVal, &maxVal, &minLoc, &maxLoc);
    slice_filt = slice_filt - static_cast<u_int16_t>(minVal);
    double edge_distance = std::min(std::min(x_size - x, x), std::min(y_size - y, y));
    double saturation_score = 0;
    double brightness = 0;

    for (int i = 0; i < slice_filt.size[0]; i++)
    {
        for (int j = 0; j < slice_filt.size[1]; j++)
        {
            saturation_score += (slice.at<u_int16_t>(i, j) == UINT16_MAX) ? -2 * static_cast<double>(UINT16_MAX) : 0.0;
            brightness += static_cast<double>(slice_filt.at<u_int16_t>(i, j));
        }
    }
    double score = edge_distance * 1000 + saturation_score + brightness;
    // std::cout << "Total score: " << score << " - ED: " << edge_distance * 1000 << " SS: " << saturation_score << " BR: " << brightness << std::endl;
    return score;
}

Star refineStarCentroid(cv::Mat image, double x_init, double y_init, int iterations = 10, double *score = nullptr)
{
    double x_cent = x_init;
    double y_cent = y_init;
    const int MASK_SIZE = 15;
    const int SIGMA = 5;
    static_assert(MASK_SIZE % 2);

    if ((x_cent > MASK_SIZE) && (x_cent < image.size[0] - MASK_SIZE) && (y_cent > MASK_SIZE) && (y_cent < image.size[1] - MASK_SIZE))
    {
        int x_offset = (int)x_init - (int)(MASK_SIZE / 2);
        int y_offset = (int)y_init - (int)(MASK_SIZE / 2);

        if (score != nullptr)
        {
            cv::Mat sliceee = image(cv::Range(x_offset, x_offset + MASK_SIZE), cv::Range(y_offset, y_offset + MASK_SIZE)).clone();
            *score = rankStar(sliceee, x_init, y_init, image.size[0], image.size[1]);
        }
        for (int i = 0; i < iterations; i++)
        {

            cv::Mat slice = image(cv::Range(x_offset, x_offset + MASK_SIZE), cv::Range(y_offset, y_offset + MASK_SIZE)).clone();
            slice.convertTo(slice, CV_64F);
            cv::Mat ker = gaussianMask(MASK_SIZE, SIGMA, x_cent - (float)x_offset, y_cent - (float)y_offset);
            slice.mul(ker);
            cv::Moments moments = cv::moments(slice);
            x_cent = x_offset + moments.m10 / moments.m00;
            y_cent = y_offset + moments.m01 / moments.m00;
        }
    }
    Star ret_star;
    ret_star.x_center = x_cent;
    ret_star.y_center = y_cent;
    ret_star.brighteness = 1;

    return ret_star;
}

int main()
{

    MQTTHandler mqtt_handler;
    const char *shm_name = "/guider_image";

    int fd = shm_open(shm_name, O_RDONLY, 0);
    if (fd == -1)
    {
        perror("shm_open");
        return 1;
    }

    // 2. Get the size of the shared memory
    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("fstat");
        close(fd);
        return 1;
    }
    size_t size = st.st_size;

    // 3. Map the shared memory into our address space
    void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return 1;
    }
    Star guiding_star = {-1, -1, -1};
    SHM_DetectedStarsInfo stars;
    // const int fb_h = 1080;
    // const int fb_w = 1920;
    // cv::Mat bgrx_image(fb_h, fb_w, CV_8UC4, cv::Scalar(0, 0, 0, 255));
    // std::ofstream fb("/dev/fb0", std::ios::binary);
    int last_ID = -1;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto img_info = (ImageInfo *)(addr);
        unsigned char *img_data = reinterpret_cast<unsigned char *>(addr) + sizeof(ImageInfo);
        if (img_info->ID != last_ID)
        {
            cv::Mat img;
            /*Here we are assuming 12 bit bit depth*/
            auto start = std::chrono::high_resolution_clock::now();
            switch (img_info->data_type)
            {
            case RAW8: // RAW8
                if (img_info->bayerPattern == NONE || img_info->bayerPattern == UNKNOWN_PATT)
                {
                    img = cv::Mat(img_info->y_size, img_info->x_size, CV_8UC1, img_data);
                    img.convertTo(img, CV_16UC1, 256.0);
                }
                break;
            case RAW16: // RAW16
                if (img_info->bayerPattern == NONE || img_info->bayerPattern == UNKNOWN_PATT)
                {
                    img = cv::Mat(img_info->y_size, img_info->x_size, CV_16UC1, img_data);
                }
                break;
            case RGB24:                                                               // RGB24
                img = cv::Mat(img_info->y_size, img_info->x_size, CV_8UC3, img_data); // BGR in OpenCV
                cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
                img.convertTo(img, CV_16UC1, 256.0);
                break;
            case Y8:
                img = cv::Mat(img_info->y_size, img_info->x_size, CV_8UC1, img_data);
                img.convertTo(img, CV_16UC1, 256.0);
                break;
            }
            cv::Scalar im_mean, im_std;
            cv::meanStdDev(img, im_mean, im_std);
            auto stop = std::chrono::high_resolution_clock::now();
            if (im_mean[0] < 15000.0 && im_std[0] < 2500.0)
            {
                cv::Mat img_filt;
                cv::GaussianBlur(img, img_filt, cv::Size(15, 15), 6);
                cv::subtract(img, img_filt, img_filt);
                cv::medianBlur(img_filt, img_filt, 3);
                cv::Mat binary_im;
                cv::threshold(img_filt, binary_im, im_mean[0] * 0.5, 65536, cv::THRESH_BINARY);
                cv::Mat binary_8u;
                binary_im.convertTo(binary_8u, CV_8U, 255.0 / 65535.0);
                cv::Mat labels, stats, centroids;
                int num_objects = cv::connectedComponentsWithStats(
                    binary_8u,
                    labels,
                    stats,
                    centroids,
                    8,     // connectivity: 4 or 8
                    CV_32S // label type
                );

                std::vector<Star> stars_vect;
                stars_vect.reserve(num_objects - 1);
                for (int i = 1; i < num_objects; i++)
                {
                    stars_vect.push_back(Star({centroids.at<double>(i, 0), centroids.at<double>(i, 1), stats.at<int>(i, cv::CC_STAT_AREA)}));
                }
                std::sort(stars_vect.begin(), stars_vect.end(),
                          [](const Star &a, const Star &b)
                          {
                              return a.brighteness > b.brighteness; // descending
                          });
                std::ostringstream oss;
                oss << "{ \"stars\": [";

                // Loop through each object (skip label 0 = background)
                stars.detected_stars = num_objects;
                std::cout << stars_vect.size() << std::endl;
                for (int i = 0; i < stars_vect.size(); i++)
                {
                    // std::cout << "processing " << i << "star" << std::endl;
                    int area = stars_vect[i].brighteness;

                    double cx = stars_vect[i].x_center;
                    double cy = stars_vect[i].y_center;
                    Star ref_star;
                    if (i < max_considered_stars)
                    {
                        double score;
                        ref_star = refineStarCentroid(img, cx, cy, 10, &score);
                        std::cout << "Object " << i << ": area=" << area
                                  << ", center=(" << ref_star.x_center << "," << ref_star.y_center << ")" << " Score: " << score << std::endl;
                        // Format with fixed precision
                        oss << "["
                            << std::fixed << std::setprecision(5) << ref_star.x_center << ","
                            << std::fixed << std::setprecision(5) << ref_star.y_center << ","
                            << area << "]";

                        if (i < (max_considered_stars - 1) && i < (stars_vect.size() - 1))
                        {
                            oss << ",";
                        }
                    }
                }

                oss << "] }";
                std::string mqtt_mess = oss.str();
                mqtt_handler.publish("guider/detected_stars", mqtt_mess);
            }
            std::cout << "New image recieved ... Mean brightness: " << im_mean[0] << "std:" << im_std[0] << " Exec time: " << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
            last_ID = img_info->ID;
        }
    }

    // fb.close();
    return 0;
}