#include <iostream>
#include <cstring>
#include "senderReader.hpp"
#include "cameraControl.hpp"
#include <thread>
#include <chrono>


int main(){
    cameraControl cam_ctrl;
    cam_ctrl.scanForCameras();
    cam_ctrl.openFirstAvaible();
    cameraSetup cam_setup;
    cam_setup.img_data_type = RGB24;
    cam_setup.exposure_us = 2'000'000;
    cam_setup.interval_ms = 200;
    cam_setup.gain = 200;
    cam_ctrl.setupCamera(cam_setup);
    ImageInfo im_info = {0, 1280, 960, RGB24, NONE};
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
            ImageInfo im_info = {0, 1280, 960, cam_setup.img_data_type, NONE};
            senderReader.modifyBufferSize(im_info);
            cam_ctrl.setupCamera(cam_setup);
            cam_ctrl.startVideo();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
    }
    cam_ctrl.stopVideo();

    return 0;
}