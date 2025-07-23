#include "captureAndShare_types.hpp"
#include "senderReader.hpp"
#include <fcntl.h>           /* For O_* constants */
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <iostream>

senderReader::senderReader(){
    setupShm();
    /*setup shared memory*/



}

senderReader::~senderReader(){
    if(m_image_buffer != nullptr){
        delete[] m_image_buffer;
    }
}

bool fileExists(const char* path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}


int setupShmFromPath(const char* path, int size, int* fd, void** ptr){
    if (path[0] != '/') {
        fprintf(stderr, "Shared memory path must start with '/': %s\n", path);
        return -1;
    }
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/dev/shm%s", path);

    if (!fileExists(full_path)) {
        // Shared memory file doesn't exist, create & truncate it
        *fd = shm_open(path, O_RDWR | O_CREAT, 0666);
        if (*fd == -1) {
            perror("shm_open create");
            return -1;
        }
        if (ftruncate(*fd, size) == -1) {
            perror("ftruncate");
            close(*fd);
            return -1;
        }
        printf("Created and truncated shared memory to fixed size\n");
    } else {
        // Exists, open without creating
        *fd = shm_open(path, O_RDWR, 0666);
        if (*fd == -1) {
            perror("shm_open");
            return -1;
        }
        printf("Opened existing shared memory\n");
    }

    *ptr = mmap(nullptr, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);

    if (*ptr == MAP_FAILED) {
        perror("mmap");
        *ptr = nullptr;
        return -1;
    }
    return 0;
}



int senderReader::setupShm() {

    constexpr size_t fixed_x = 4096;
    constexpr size_t fixed_y = 4096;
    constexpr size_t bytes_per_pixel = 16;
    size_t fixed_buffer_size = fixed_x * fixed_y * bytes_per_pixel;

    /*---------------------- IMAGE SHARED MEMORY SETUP -------------- */
    // const char* shm_path = IMAGE_SHM_FULL_PATH;
    setupShmFromPath(IMAGE_SHM, sizeof(ImageInfo) + fixed_buffer_size, &m_guider_image_shm_fd, reinterpret_cast<void**>(&m_guider_image_shm_ptr));
    // if (!fileExists(shm_path)) {
    //     // Shared memory file doesn't exist, create & truncate it
    //     m_guider_image_shm_fd = shm_open(IMAGE_SHM, O_RDWR | O_CREAT, 0666);
    //     if (m_guider_image_shm_fd == -1) {
    //         perror("shm_open create");
    //         return -1;
    //     }
    //     if (ftruncate(m_guider_image_shm_fd, sizeof(ImageInfo) + fixed_buffer_size) == -1) {
    //         perror("ftruncate");
    //         close(m_guider_image_shm_fd);
    //         return -1;
    //     }
    //     printf("Created and truncated shared memory to fixed size\n");
    // } else {
    //     // Exists, open without creating
    //     m_guider_image_shm_fd = shm_open(IMAGE_SHM, O_RDWR, 0666);
    //     if (m_guider_image_shm_fd == -1) {
    //         perror("shm_open");
    //         return -1;
    //     }
    //     printf("Opened existing shared memory\n");
    // }

    // m_guider_image_shm_ptr = mmap(nullptr, sizeof(ImageInfo) + fixed_buffer_size,
    //                              PROT_WRITE, MAP_SHARED, m_guider_image_shm_fd, 0);
    // if (m_guider_image_shm_ptr == MAP_FAILED) {
    //     perror("mmap");
    //     return -1;
    // }

    /*---------------------- CAMERA SETUP SHARED MEMORY SETUP -------------- */
    setupShmFromPath(CAMERA_CONTROLS_SHM, sizeof(SHM_cameraControls), &m_guider_camera_controls_shm_fd, reinterpret_cast<void**>(&m_guider_camera_controls_shm_ptr));
    m_guider_camera_controls_shm_ptr->updated = false;
    m_guider_camera_controls_shm_ptr->exposure = -1;
    m_guider_camera_controls_shm_ptr->gain = -1;
    m_guider_camera_controls_shm_ptr->interval = -1;
    m_guider_camera_controls_shm_ptr->roi_x_end = -1;
    m_guider_camera_controls_shm_ptr->roi_y_end = -1;
    m_guider_camera_controls_shm_ptr->roi_x_start = -1;
    m_guider_camera_controls_shm_ptr->roi_y_start = -1;


    /*---------------------- MISC INFO SHARED MEMORY SETUP -------------- */
    const char* shm_path_misc_info = MISC_INFO_SHM_FULL_PATH;
    setupShmFromPath(MISC_INFO_SHM, sizeof(Misc_Info), &m_misc_info_shm_fd, reinterpret_cast<void**>(&m_misc_info_shm_ptr));
    /*---------------------- EXPOSURE INFO ----------------------- */
    return 0;
}


bool senderReader::newSetupRequested(cameraSetup* cam_controls){
    if(m_guider_camera_controls_shm_ptr->updated == true){
        m_guider_camera_controls_shm_ptr->updated = false;
        cam_controls->exposure_us = m_guider_camera_controls_shm_ptr->exposure;
        cam_controls->gain = m_guider_camera_controls_shm_ptr->gain;
        cam_controls->img_data_type = m_guider_camera_controls_shm_ptr->dataType;
        cam_controls->interval_ms = m_guider_camera_controls_shm_ptr->interval;
        return true;
    }
    else{
        return false;
    }
}


int senderReader::sendImageData(){

    /* TAKE NAMED SEMAPHORE guider/image_sem */
    /* SAVE IMAGE {m_image_info, m_image_buffer} to SHM - guider/image*/
    std::memcpy((void*)m_guider_image_shm_ptr, &m_image_info, sizeof(ImageInfo));
    // int bytes_per_pixel;
    std::memcpy(static_cast<uint8_t*>(m_guider_image_shm_ptr) + sizeof(ImageInfo), m_image_buffer, m_buffer_size);
    /* FREE SEMAPHORE  guider/image_sem */
    return 0;
}

int senderReader::sendMisc(Misc_Info& misc_info){
    m_misc_info_shm_ptr->current_exposure_time = misc_info.current_exposure_time;
    m_misc_info_shm_ptr->final_exposure_time = misc_info.final_exposure_time;
    m_misc_info_shm_ptr->updated = true;
    // std::cout << "sending misc" << m_misc_info_shm_ptr->current_exposure_time << " " << m_misc_info_shm_ptr->final_exposure_time <<std::endl;
    return 0;
}

int senderReader::modifyBufferSize(ImageInfo img_info){
    auto last_type = m_image_info.data_type;

    m_image_info = img_info;
    
    if(img_info.data_type == UNKNOWN_DATA_TYPE){
        m_image_info.data_type = last_type;    
    }
    delete[] m_image_buffer;
    m_buffer_size = img_info.x_size * img_info.y_size * ImageDataTypeToBytes(m_image_info.data_type);
    m_image_buffer = new uint8_t[m_buffer_size];
    return 0;
}

int senderReader::setImageData(const uint8_t* src){
    m_image_info.ID = m_image_info.ID + 1;
    memcpy(m_image_buffer, src, m_buffer_size);
    return 0;
}