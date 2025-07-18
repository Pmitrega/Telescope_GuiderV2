#include "ShmHandler.hpp"
#include <sys/stat.h>
#include <iostream>
#include <fcntl.h>           /* For O_* constants */
#include <sys/mman.h>

bool fileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}


ShmHandler::ShmHandler(){
    if(fileExists(CAMERA_CONTROLS_SHM_FULL_PATH)){
        m_shm_camera_controls_fd = shm_open(CAMERA_CONTROLS_SHM, O_RDWR, 0666);
        if(m_shm_camera_controls_fd == -1){
            std::cerr <<"Failed to open camera control SHM"<<std::endl;
        }
        else{
            m_shm_camera_controls_ptr = static_cast<SHM_cameraControls*>(mmap(nullptr, sizeof(SHM_cameraControls),
                                                                PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_camera_controls_fd, 0));
        }
    }
    else{
        std::cerr <<"Camera control path does not exists";
    }
}

int ShmHandler::setupCameraGainExpoInterval(int gain, int expo, int interval){
    m_shm_camera_controls_ptr->gain = gain;
    m_shm_camera_controls_ptr->exposure = expo;
    m_shm_camera_controls_ptr->interval = interval;
    m_shm_camera_controls_ptr->dataType = UNKNOWN_DATA_TYPE;
    m_shm_camera_controls_ptr->roi_x_end = -1;
    m_shm_camera_controls_ptr->roi_y_end = -1;
    m_shm_camera_controls_ptr->roi_x_start = -1;
    m_shm_camera_controls_ptr->roi_y_start = -1;
    m_shm_camera_controls_ptr->updated = true;
    return 0;
}

int ShmHandler::setupCameraDataType(ImageDataType img_data_type){
    m_shm_camera_controls_ptr->dataType = img_data_type;
    m_shm_camera_controls_ptr->updated = true;
    return 0;
}
int ShmHandler::setupCameraRoi(){

    return 0;
}