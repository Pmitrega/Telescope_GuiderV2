#include <iostream>
#include <thread>
#include <chrono>
#include <fcntl.h>      // O_RDONLY, O_RDWR
#include <sys/mman.h>   // shm_open, mmap
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close
#include "captureAndShare_types.hpp"
#include <opencv2/opencv.hpp>

int main(){


    const char *shm_name = "/guider_image";

    int fd = shm_open(shm_name, O_RDONLY, 0);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    // 2. Get the size of the shared memory
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }
    size_t size = st.st_size;

    // 3. Map the shared memory into our address space
    void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    

    int last_ID = -1;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto img_info = (ImageInfo*)(addr);
        unsigned char* img_data = reinterpret_cast<unsigned char*>(addr) + sizeof(ImageInfo);
        if(img_info->ID != last_ID){
            cv::Mat img;
            /*Here we are assuming 12 bit bit depth*/
            auto start =  std::chrono::high_resolution_clock::now();
            switch(img_info->data_type) {
                case RAW8: // RAW8
                    if(img_info->bayerPattern ==  NONE || img_info->bayerPattern == UNKNOWN_PATT){
                        img = cv::Mat(img_info->y_size, img_info->x_size, CV_8UC1, img_data);
                        img.convertTo(img, CV_16UC1, 256.0); 
                    }
                    break;
                case RAW16: // RAW16
                    if(img_info->bayerPattern == NONE || img_info->bayerPattern == UNKNOWN_PATT){
                        img = cv::Mat(img_info->y_size, img_info->x_size, CV_16UC1, img_data);
                    }
                    break;
                case RGB24: // RGB24
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
            auto stop =  std::chrono::high_resolution_clock::now();
            if(im_mean[0] < 15000.0 && im_std[0] < 2000.0){
                cv::Mat img_filt;
                cv::GaussianBlur(img, img_filt, cv::Size(15, 15), 6);
                cv::subtract(img, img_filt, img_filt);
                cv::medianBlur(img_filt, img_filt, 3);
                cv::Mat binary_im;
                cv::threshold(img_filt, binary_im, 4000, 65536,cv::THRESH_BINARY);
                cv::Mat binary_8u;
                binary_im.convertTo(binary_8u, CV_8U, 255.0/65535.0);
                cv::Mat labels, stats, centroids;
                int num_objects = cv::connectedComponentsWithStats(
                    binary_8u,
                    labels,
                    stats,
                    centroids,
                    8,                // connectivity: 4 or 8
                    CV_32S            // label type
                );
                    // Loop through each object (skip label 0 = background)
                for (int i = 1; i < num_objects; i++) {
                    int x = stats.at<int>(i, cv::CC_STAT_LEFT);
                    int y = stats.at<int>(i, cv::CC_STAT_TOP);
                    int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
                    int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
                    int area = stats.at<int>(i, cv::CC_STAT_AREA);

                    double cx = centroids.at<double>(i, 0);
                    double cy = centroids.at<double>(i, 1);

                    std::cout << "Object " << i << ": area=" << area 
                            << ", bbox=(" << x << "," << y << "," << w << "," << h << ")"
                            << ", center=(" << cx << "," << cy << ")" << std::endl;
                }

                // cv::imwrite("img.png", binary_im);
                // cv::imwrite("img_org.png", img);
                // cv::imwrite("img_filt.png", img_filt);
            }
            std::cout << "New image recieved ... Mean brightness: "<< im_mean[0] << "std:" << im_std[0] <<  " Exec time: " << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
            last_ID = img_info->ID;
        }

    }
    

    return 0;
}