#include <iostream>
#include <cstring>
#include "senderReader.hpp"
#include "cameraControl.hpp"
#include <thread>
#include <chrono>


int main(){
    cameraControl cam_ctrl;
    cam_ctrl.scanForCameras();
    if(cam_ctrl.openFirstAvaible() == -1){
        std::cout << "Failed to open" << std::endl;
        return -1;
    }
    auto cam_info = cam_ctrl.getCurrentCameraInfo();
    cameraSetup cam_setup;
    cam_setup.img_data_type =  cam_info.img_data_types[0];
    cam_setup.exposure_us = 100'000;
    cam_setup.interval_ms = 100;
    cam_setup.gain = 50;
    cam_ctrl.setupCamera(cam_setup);
    std::cout << cam_info.y_res << " " << cam_info.x_res << std::endl;
    ImageInfo im_info = {0, cam_info.x_res, cam_info.y_res, cam_setup.img_data_type, NONE};
    cam_ctrl.startVideo();
    senderReader senderReader(im_info);
    while(true){
        const uint8_t* cam_buffer = cam_ctrl.getImageBuffer();
        if(cam_buffer != nullptr){
            senderReader.setImageData(cam_buffer);
            senderReader.sendImageData();
            std::cout<<"Sending..."<<std::endl;
        }

        if(senderReader.newSetupRequested(&cam_setup) == true){
            cam_ctrl.stopVideo();
            ImageInfo im_info = {0,  cam_info.x_res, cam_info.y_res, cam_setup.img_data_type, NONE};
            senderReader.modifyBufferSize(im_info);
            cam_ctrl.setupCamera(cam_setup);
            cam_ctrl.startVideo();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
    }
    cam_ctrl.stopVideo();

    return 0;
}