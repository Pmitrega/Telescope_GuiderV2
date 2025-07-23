#include "ShmHandler.hpp"
#include <sys/stat.h>
#include <iostream>
#include <fcntl.h>           /* For O_* constants */
#include <sys/mman.h>

bool fileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

int openShm(const char* path, int size, void** ptr){
    if (path[0] != '/') {
        fprintf(stderr, "Shared memory path must start with '/': %s\n", path);
        return -1;
    }
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/dev/shm%s", path);

    if(fileExists(full_path)){
        int fd = shm_open(path, O_RDWR, 0666);
        if(fd == -1){
            std::cerr <<"Failed to open  SHM: "<< path << std::endl;
        }
        else{
            *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        }
    }
    else{
        std::cerr <<"Path does not exists: " << path;
        return -1;
    }

    return 0;
}


ShmHandler::ShmHandler(){
    openShm(CAMERA_CONTROLS_SHM, sizeof(SHM_cameraControls) ,reinterpret_cast<void**>(&m_shm_camera_controls_ptr));
    openShm(MISC_INFO_SHM, sizeof(Misc_Info), reinterpret_cast<void**> (&m_shm_misc_info_ptr));
    // if(fileExists(CAMERA_CONTROLS_SHM_FULL_PATH)){
    //     m_shm_camera_controls_fd = shm_open(CAMERA_CONTROLS_SHM, O_RDWR, 0666);
    //     if(m_shm_camera_controls_fd == -1){
    //         std::cerr <<"Failed to open camera control SHM"<<std::endl;
    //     }
    //     else{
    //         m_shm_camera_controls_ptr = static_cast<SHM_cameraControls*>(mmap(nullptr, sizeof(SHM_cameraControls),
    //                                                             PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_camera_controls_fd, 0));
    //     }
    // }
    // else{
    //     std::cerr <<"Camera control path does not exists";
    // }
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

void ShmHandler::readMiscInfo(Misc_Info& misc_info){
    misc_info.updated = m_shm_misc_info_ptr->updated;
    misc_info.current_exposure_time = m_shm_misc_info_ptr->current_exposure_time;
    misc_info.final_exposure_time = m_shm_misc_info_ptr->final_exposure_time;
    m_shm_misc_info_ptr->updated = false;
}

int ShmHandler::setupCameraRoi(){

    return 0;
}