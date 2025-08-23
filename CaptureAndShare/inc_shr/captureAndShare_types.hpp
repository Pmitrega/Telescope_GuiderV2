#ifndef CAPTURE_AND_SHARE_TYPES_H
#define CAPTURE_AND_SHARE_TYPES_H

#include <utility>
#define IMAGE_SHM "/guider_image"
#define IMAGE_SHM_FULL_PATH "/dev/shm" IMAGE_SHM

#define CAMERA_CONTROLS_SHM "/camera_setup"
#define CAMERA_CONTROLS_SHM_FULL_PATH "/dev/shm" CAMERA_CONTROLS_SHM

#define MISC_INFO_SHM "/misc_info"
#define MISC_INFO_SHM_FULL_PATH "/dev/shm" MISC_INFO_SHM

enum ImageDataType
{
    RGB24 = 0,
    RAW16,
    RAW8,
    Y8,
    Y16,
    UNKNOWN_DATA_TYPE
};



enum ImageBayerPattern
{
    NONE = 0,
    BGGR, 
    RGBG,
    GRBG,
    RGGB,
    UNKNOWN_PATT
};


struct ImageInfo
{
    int ID;
    int x_size;
    int y_size;
    ImageDataType data_type;
    ImageBayerPattern bayerPattern;
};


struct SHM_cameraControls
{
    bool updated;
    int gain;
    int exposure;
    int interval;
    ImageDataType dataType;
    int roi_x_start;
    int roi_x_end;
    int roi_y_start;
    int roi_y_end;
};

enum videoSource{
    CAMERA_0 = 0,
    CAMERA_1,
    CAMERA_2,
    CAMERA_3,
    CAMERA_4,
    CAMERA_5,
    CAMERA_6,
    CAMERA_7,
    CAMERA_8,
    CAMERA_9,
    TEST_STREAM = 255
};

struct cameraSetup{
    std::pair<int,int> ROI_x = std::make_pair<int,int>(-1,-1);
    std::pair<int,int> ROI_y = std::make_pair<int,int>(-1,-1);
    ImageDataType img_data_type = UNKNOWN_DATA_TYPE;
    int gain = -1;
    int exposure_us = -1;
    int interval_ms = -1;
    float temperature;
};

struct SHM_cameraInfo
{
    int x_size;
    int y_size;
    int gain_min;
    int gain_max;
    int exposure_min;
    int exposure_max;
};


struct Misc_Info
{
    bool updated = false;
    int current_exposure_time = -1;
    int final_exposure_time = -1;
};


constexpr int ImageDataTypeToBytes(ImageDataType type)
{
    switch (type)
    {
        case RGB24: return 3;
        case RAW16: return 2;
        case RAW8:  return 1;
        case Y8:  return 1;
        case Y16:  return 2;
        default:    return 0; // or throw/handle invalid enum
    }
}

// Optional: readable strings
constexpr char* BayerPatternToStr(ImageBayerPattern pattern) {
    switch (pattern) {
        case BGGR: return (char*)"BGGR";
        case RGBG: return (char*)"RGBG";
        case GRBG: return (char*)"GRBG";
        case RGGB: return (char*)"RGGB";
        case NONE: return (char*)"None";
        default:   return (char*)"Unknown";
    }
}


constexpr char* ImageDataTypeToStr(ImageDataType type) {
    switch (type) {
        case RGB24: return (char*)"RGB24";
        case RAW16: return (char*)"RAW16";
        case RAW8:  return (char*)"RAW8";
        case Y8:    return (char*)"Y8";
        case Y16:   return (char*)"Y16";
        default:    return (char*)"UNKNOWN";
    }
}

#endif