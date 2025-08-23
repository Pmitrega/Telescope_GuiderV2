#include <iostream>
#include <cstring>
#include "senderReader.hpp"
#include "cameraControl.hpp"
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define MAIN_LOOP_SLEEP 1
#define NO_CAMERA_SLEEP_SEC 2
#define SEND_MISC_IT_MAX (25 / MAIN_LOOP_SLEEP)

#define USE_DUMMY false





int main()
{
    cameraControl cam_ctrl;
    senderReader senderReader;
    cameraSetup cam_setup;
    uint64_t send_misc_it = 0;
    cameraInfo last_opened_camera;
    cameraSetup last_camera_setup;
    if(USE_DUMMY){
        ImageInfo im_info = {0, 1280, 960, RAW16, NONE};
        senderReader.modifyBufferSize(im_info);
        uint8_t* buffer;
        std::string base_dir = "/home/orangepi/Telescope_GuiderV2/CaptureAndShare/dummy_images";
        int it = 0;
        while (true){
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            std::string img_path = base_dir + "/image0" + std::to_string(it%10) + ".raw";
            int fd = open(img_path.c_str(), O_RDONLY);
            if(fd == -1){
                std::cout <<"Failed to open file: " << img_path << std::endl;
                return -1;
            }
            void *map = mmap(NULL, 1280*960*2, PROT_READ, MAP_PRIVATE, fd, 0);
            buffer = (uint8_t*)map;
            senderReader.setImageData(buffer);
            senderReader.sendImageData();
            std::cout << "Sending image ..." << std::endl;
            it++;
        }
    }
    while (true)
    {
        /*Camera disconnection handling ...*/
        if (cam_ctrl.cameraOpened() == false)
        {
            cam_ctrl.stopVideo();
            cam_ctrl.scanForCameras();
            if (cam_ctrl.openFirstAvaible() == -1)
            {
                std::this_thread::sleep_for(std::chrono::seconds(NO_CAMERA_SLEEP_SEC));
                continue;
            }
            auto cam_info = cam_ctrl.getCurrentCameraInfo();

            if (cam_info.cameraName == last_opened_camera.cameraName)
            {
                std::cout << "Found previously connected camera:" << cam_info.cameraName << std::endl;
                cam_setup.img_data_type = last_camera_setup.img_data_type;
                cam_setup.exposure_us = last_camera_setup.exposure_us;
                cam_setup.gain = last_camera_setup.gain;
                cam_setup.interval_ms = last_camera_setup.interval_ms;
            }
            else
            {
                cam_setup.img_data_type = cam_info.img_data_types[0];
                cam_setup.exposure_us = 500;
                cam_setup.interval_ms = 500;
                cam_setup.gain = static_cast<int>(cam_info.gain_range.second / 2);
            }
            cam_ctrl.setupCamera(cam_setup);
            auto applied_setup = cam_ctrl.getCameraSetup();
            ImageInfo im_info = {0, applied_setup.ROI_x.second, applied_setup.ROI_y.second,
                                 cam_setup.img_data_type, cam_info.bayer_patter};
            senderReader.modifyBufferSize(im_info);
            cam_ctrl.startVideo();

            last_camera_setup = cam_setup;
            last_opened_camera = cam_info;
            continue;
        }

        const uint8_t *cam_buffer = cam_ctrl.getImageBuffer();
        if (cam_buffer != nullptr)
        {
            senderReader.setImageData(cam_buffer);
            senderReader.sendImageData();
            std::cout << "Sending..." << std::endl;
        }

        if (senderReader.newSetupRequested(&cam_setup) == true)
        {
            cam_ctrl.stopVideo();

            cam_ctrl.setupCamera(cam_setup);
            auto applied_setup = cam_ctrl.getCameraSetup();
            auto cam_info = cam_ctrl.getCurrentCameraInfo();
            ImageInfo im_info = {0, applied_setup.ROI_x.second, applied_setup.ROI_y.second,
                                 cam_setup.img_data_type, cam_info.bayer_patter};
            senderReader.modifyBufferSize(im_info);

            last_camera_setup = cam_setup;
            /*gets current setup, with values that was succesfully applied*/

            cam_ctrl.startVideo();
        }
        if (send_misc_it % SEND_MISC_IT_MAX == 0)
        {
            auto expo_status = cam_ctrl.getExpsureStatus();
            Misc_Info misc_info = {true, expo_status.first, expo_status.second};
            senderReader.sendMisc(misc_info);
        }
        send_misc_it += 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_LOOP_SLEEP));
    }
    cam_ctrl.stopVideo();

    return 0;
}