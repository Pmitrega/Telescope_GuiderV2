#include <iostream>
#include <cstring>
#include "senderReader.hpp"
#include "cameraControl.hpp"
#include <thread>
#include <chrono>

#define MAIN_LOOP_SLEEP 1
#define SEND_MISC_IT_MAX (50/MAIN_LOOP_SLEEP)




int main(){
    cameraControl cam_ctrl;
    senderReader senderReader;
    cam_ctrl.scanForCameras();
    if(cam_ctrl.openFirstAvaible() == -1){
        std::cout << "Failed to open" << std::endl;
        return -1;
    }
    auto cam_info = cam_ctrl.getCurrentCameraInfo();
    cameraSetup cam_setup;
    cam_setup.img_data_type =  cam_info.img_data_types[0];
    cam_setup.exposure_us = 100'000;
    cam_setup.interval_ms = 500;
    cam_setup.gain = 50;
    cam_ctrl.setupCamera(cam_setup);
    std::cout << cam_info.y_res << " " << cam_info.x_res << std::endl;
    ImageInfo im_info = {0, cam_info.x_res, cam_info.y_res, cam_setup.img_data_type, NONE};
    senderReader.modifyBufferSize(im_info);
    cam_ctrl.startVideo();


    uint64_t send_misc_it = 0;

    while(true){
        const uint8_t* cam_buffer = cam_ctrl.getImageBuffer();
        if(cam_buffer != nullptr){
            senderReader.setImageData(cam_buffer);
            senderReader.sendImageData();
            std::cout<<"Sending..."<<std::endl;
        }

        if(senderReader.newSetupRequested(&cam_setup) == true){
            cam_ctrl.stopVideo();

            cam_ctrl.setupCamera(cam_setup);

            /*gets current setup, with values that was succesfully applied*/
            auto applied_setup = cam_ctrl.getCameraSetup();
            ImageInfo im_info = {0,  applied_setup.ROI_x.second, applied_setup.ROI_y.second, cam_setup.img_data_type, cam_ctrl.getCurrentCameraInfo().bayer_patter};
            std::cout << "xy size" << applied_setup.ROI_x.second <<" " <<applied_setup.ROI_y.second << std::endl;
            senderReader.modifyBufferSize(im_info);
            cam_ctrl.startVideo();
        }
        if(send_misc_it%SEND_MISC_IT_MAX == 0){
            auto expo_status = cam_ctrl.getExpsureStatus();
            Misc_Info misc_info = {true, expo_status.first, expo_status.second};
            senderReader.sendMisc(misc_info);
        }
        send_misc_it +=1;
        std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_LOOP_SLEEP));
        
    }
    cam_ctrl.stopVideo();

    return 0;
}