#include <iostream>
#include <thread>
#include <chrono>
#include <fcntl.h>      // O_RDONLY, O_RDWR
#include <sys/mman.h>   // shm_open, mmap
#include <sys/stat.h>   // fstat
#include <unistd.h>     // close
#include "captureAndShare_types.hpp"
#include <opencv2/opencv.hpp>
#include <utility>
#include <cmath>
#include <fstream>   // For std::ofstream
#include "MQTTHandler.hpp"
#include <iomanip>
#include <sstream>

double Gaussian2D(double x, double y, double sigma, double x0 = 0, double y0 = 0){
    return exp(-((x-x0)*(x-x0) + (y-y0)*(y-y0))/(2*sigma*sigma));
}


std::pair<double, double> refineStarCentroid(cv::Mat image, double x_init, double y_init, int iterations = 10){
    double x_cent = x_init;
    double y_cent = y_init;
    for(int i =0; i< iterations; i++){
        
    }
    std::pair<double, double> ret = {x_cent, y_cent};
    return ret;
}

int main(){

    MQTTHandler mqtt_handler;
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
    
    const int fb_h = 1080;
    const int fb_w = 1920;
    cv::Mat bgrx_image(fb_h, fb_w, CV_8UC4, cv::Scalar(0,0,0,255));
    std::ofstream fb("/dev/fb0", std::ios::binary);
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
            if (!fb) {
                std::cerr << "can't open fb\n";
            }
            else{
                int img_h = img.rows;
                int img_w = img.cols;
                // Create framebuffer buffer (BGRX)


                // // Create ROI (top-left corner) in framebuffer
                cv::Mat roi = bgrx_image(cv::Rect(0, 0, std::min(img_w, fb_w), std::min(img_h, fb_h)));

                // Convert grayscale to 3 channels
                cv::Mat img_8bit;
                cv::Mat img_color;
                img.convertTo(img_8bit, CV_8U, 1/256.0);
                cv::cvtColor(img_8bit, img_color, cv::COLOR_GRAY2BGRA); // Grayscale → BGRA

                // Copy to ROI (OpenCV handles memory efficiently)
                img_color.copyTo(roi(cv::Rect(0, 0, img_color.cols, img_color.rows)));
                    // Write to framebuffer
                fb.seekp(0);  
                fb.write(reinterpret_cast<char*>(bgrx_image.data), bgrx_image.total() * bgrx_image.elemSize());

                std::cout << "Saved image to fb\n" <<std::endl;
            }
            cv::Scalar im_mean, im_std;
            cv::meanStdDev(img, im_mean, im_std);
            auto stop =  std::chrono::high_resolution_clock::now();
            if(im_mean[0] < 15000.0 && im_std[0] < 2500.0){
                cv::Mat img_filt;
                cv::GaussianBlur(img, img_filt, cv::Size(15, 15), 6);
                cv::subtract(img, img_filt, img_filt);
                cv::medianBlur(img_filt, img_filt, 3);
                cv::Mat binary_im;
                cv::threshold(img_filt, binary_im, im_mean[0] * 0.5, 65536,cv::THRESH_BINARY);
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
                std::ostringstream oss;
                oss << "{ \"stars\": [";

                // Loop through each object (skip label 0 = background)
                for (int i = 1; i < num_objects; i++) {
                    int area = stats.at<int>(i, cv::CC_STAT_AREA);

                    double cx = centroids.at<double>(i, 0);
                    double cy = centroids.at<double>(i, 1);

                    std::cout << "Object " << i << ": area=" << area
                            << ", center=(" << cx << "," << cy << ")" << std::endl;

                    // Format with fixed precision
                    oss << "[" 
                        << std::fixed << std::setprecision(5) << cx << ","
                        << std::fixed << std::setprecision(5) << cy << ","
                        << area << "]";

                    if (i < num_objects - 1) {
                        oss << ",";
                    }
                }

                oss << "] }";
                std::string mqtt_mess = oss.str();
                mqtt_handler.publish("guider/detected_stars", mqtt_mess);
            }
            std::cout << "New image recieved ... Mean brightness: "<< im_mean[0] << "std:" << im_std[0] <<  " Exec time: " << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
            last_ID = img_info->ID;
        }


    }
    
    fb.close();
    return 0;
}