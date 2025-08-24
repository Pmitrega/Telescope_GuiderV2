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
            double mean = cv::mean(img)[0];
            auto stop =  std::chrono::high_resolution_clock::now();
            std::cout << "New image recieved ... Mean brightness: "<< mean <<  " Exec time: " << std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() << std::endl;
            last_ID = img_info->ID;
        }

    }
    

    return 0;
}