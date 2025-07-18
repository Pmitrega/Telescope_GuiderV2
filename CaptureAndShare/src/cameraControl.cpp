#include <cameraControl.hpp>
#include "SVBCameraSDK.h"
#include <iostream>
#include <algorithm> // for std::find

/*
    BGGR, 
    RGBG,
    GRBG,
    RGGB,
*/

constexpr ImageBayerPattern getBayerFromSVBBayer(SVB_BAYER_PATTERN pattern){
    switch (pattern)
    {
        case SVB_BAYER_RG: return RGGB;
        case SVB_BAYER_BG: return BGGR;
        case SVB_BAYER_GR:  return GRBG;
        case SVB_BAYER_GB:  return GRBG;
        default:    return UNKNOWN_PATT; // or throw/handle invalid enum
    }
}

constexpr ImageDataType getImageTypeFromSVBImageType(SVB_IMG_TYPE type){
    switch (type)
    {
        case SVB_IMG_RAW8: return RAW8;
        case SVB_IMG_RAW16: return RAW16;
        case SVB_IMG_RGB24:  return RGB24;
        case SVB_IMG_Y8: return Y8;
        case SVB_IMG_Y16: return Y16;
        default:    return UNKNOWN_DATA_TYPE; // or throw/handle invalid enum
    }
}

constexpr SVB_IMG_TYPE getSVBImageTypeFromImageDataType(ImageDataType type) {
    switch (type)
    {
        case RAW8:  return SVB_IMG_RAW8;
        case RAW16: return SVB_IMG_RAW16;
        case RGB24: return SVB_IMG_RGB24;
        case Y8:    return SVB_IMG_Y8;
        case Y16:   return SVB_IMG_Y16;
        default:    return SVB_IMG_END; // or handle invalid case appropriately
    }
}

cameraControl::cameraControl(){
    }

int cameraControl::scanForCameras(){
    int ret_val = -1;
    m_scanedCameras.clear();
    /*Scan for SVBONY CAMERAS*/
    ret_val = SVB_ScanForCameras();
    /*Scan for ZWO CAMERAS*/
    return ret_val;
    }

int cameraControl::openFirstAvaible(){
    if(m_scanedCameras.size() > 0){
        auto cam = m_scanedCameras[0];
        std::cout << "Opened Camera: "<< cam << std::endl;
        if(cam.producer == "SVB"){
            int ret = SVBOpenCamera(cam.ID);
            if (ret != SVB_SUCCESS)
            {
                std::cerr << "Open camera failed: \r\n" << cam.cameraName;
                return -1;
            }
            else
            {
                m_current_camera = cam;
                m_camera_opened = true;
            }

        }
        else if(cam.producer == "ZWO"){
            /*TODO*/
        }
        else{
            m_current_camera.producer = "NONE";
            m_current_camera.cameraName = "NONE";
        }
        return 0;
    }
    else{
        return -1;
    }
}


int  cameraControl::setupCamera(cameraSetup cam_setup){
    std::cout << "trying to setup camera" << std::endl;
    std::cout << "Exposure: " <<  cam_setup.exposure_us << std::endl;
    std::cout << "Gain: " << cam_setup.gain <<  std::endl;
    std::cout << "Interval: " << cam_setup.interval_ms << std::endl;
    std::cout << "Image type: " << ImageDataTypeToStr(cam_setup.img_data_type) << std::endl;
    
    if(cam_setup.interval_ms < cam_setup.exposure_us/1000){
        m_ms_per_frame = static_cast<int>(cam_setup.exposure_us/1000);
    }
    else{
        m_ms_per_frame = cam_setup.interval_ms;
    }

    if(m_camera_opened == false){
        std::cerr << "No camera is no opened, can't setup.\r\n" << std::endl;
        return -1;
    }
    if(m_current_camera.producer == "SVB"){
        SVBSetCameraMode(m_current_camera.ID, SVB_MODE_NORMAL);
        SVB_applySetup(cam_setup);
    }
    return 0;
}


int cameraControl::startVideo(){
    m_stop_video_thread = false;
    std::cout <<m_current_camera.producer <<std::endl;
    m_capture_video_thread = std::thread(&cameraControl::captureVideoThreadFunc, this);
    return 0;
}
int cameraControl::stopVideo(){
    m_stop_video_thread = true;
    if (m_capture_video_thread.joinable()) {
        m_capture_video_thread.join();
    }
    m_capture_video_thread = std::thread();
    std::cout << "Stopped video thread" << std::endl;
    return 0;
}

const uint8_t* cameraControl::getImageBuffer(){
    if(m_new_img_ready){
        m_new_img_ready = false;
        return m_image_buffer;
    }else{
        return nullptr;
    }
}
void cameraControl::captureVideoThreadFunc(){
    auto ret = SVBStartVideoCapture(m_current_camera.ID);
    m_last_send_time = std::chrono::steady_clock::now();
    if(ret != SVB_SUCCESS){
        std::cerr << "couldn't open camera" << std::endl;
    }
    else{
        std::cout <<m_current_camera.producer <<std::endl;
        while(m_stop_video_thread == false){
            scanForImage();
            using namespace std::chrono;
            bool should_send_frame = duration_cast<milliseconds>(steady_clock::now() - m_last_send_time).count() > m_ms_per_frame;
            if(m_new_img_in_buffer && should_send_frame){
                m_new_img_ready = true;
                m_new_img_in_buffer = false;
                m_last_send_time = steady_clock::now();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    SVBStopVideoCapture(m_current_camera.ID);
}

int cameraControl::scanForImage(){
    if(m_camera_opened == false){
        std::cerr << "No camera is no opened, can't read" << std::endl;
        return -1;
    }
    if(m_current_camera.producer == "SVB"){
        auto ret = SVBGetVideoData(m_current_camera.ID, m_image_buffer, m_image_buffer_size, 100);
        if(ret == SVB_SUCCESS){
            m_new_img_in_buffer = true;
        }
        else if (ret == SVB_ERROR_TIMEOUT)
        {
            // std::cout << "Image timeout" << std::endl;
        }
        else{
            std::cerr << "Could't read buffer" << std::endl;
            return -1;
        }
    }
    else{
        // std::cerr << "Unknown producer" << m_current_camera.producer <<std::endl;
    }
    return 0;
}


// --- Pretty Print ---
std::ostream& operator<<(std::ostream& os, const cameraInfo& cam) {
    os << std::boolalpha; // print bools as true/false
    os << "=== Camera Info ===\n";
    os << "Producer         : " << cam.producer << "\n";
    os << "Camera Name      : " << cam.cameraName << "\n";
    os << "Device ID        : " << cam.ID << "\n";
    os << "Resolution       : " << cam.x_res << " x " << cam.y_res << "\n";
    os << "Bayer Pattern    : " << BayerPatternToStr(cam.bayer_patter) << "\n";
    os << "Mono Camera      : " << cam.mono << "\n";

    os << "Supported Formats: ";
    for (size_t i = 0; i < cam.img_data_types.size(); ++i) {
        os << ImageDataTypeToStr(cam.img_data_types[i])
           << " (" << ImageDataTypeToBytes(cam.img_data_types[i]) << " B)";
        if (i != cam.img_data_types.size() - 1) os << ", ";
    }
    os << "\n";

    os << "Gain Range       : [" << cam.gain_range.first << ", " << cam.gain_range.second << "]\n";
    os << "Exposure (µs)    : [" << cam.exposure_range_us.first << ", " << cam.exposure_range_us.second << "]\n";
    os << "====================\n";
    return os;
}




















/********************SPECIFIC VENDORS HANDLER*********************** */

int cameraControl::SVB_ScanForCameras(){
    int ret_val = -1;
    int cameraNum = SVBGetNumOfConnectedCameras();
	printf("Svbony camera number: %d\r\n", cameraNum);
    if (cameraNum > 0)ret_val = 0;
    
    	for (int i = 0; i < cameraNum; i++)
	{
		SVB_CAMERA_INFO svb_cameraInfo;
        SVB_CAMERA_PROPERTY svb_cameraProp;
		int ret = SVBGetCameraInfo(&svb_cameraInfo, i);
		if (ret == SVB_SUCCESS)
		{
            cameraInfo cam_info = {"SVB", svb_cameraInfo.FriendlyName, static_cast<int>(svb_cameraInfo.CameraID), -1, -1, UNKNOWN_PATT, {}, {-1,-1}, {-1,-1}, true};
            m_scanedCameras.push_back(cam_info);
            ret = SVBOpenCamera(svb_cameraInfo.CameraID);
            if (ret != SVB_SUCCESS)
            {
                printf("open camera failed.\r\n");
                return -1;
            }
            ret = SVBGetCameraProperty(svb_cameraInfo.CameraID, &svb_cameraProp);
            if (ret != SVB_SUCCESS)
            {
                printf("get camera property failed\r\n");
                SVBCloseCamera(svb_cameraInfo.CameraID);
                return -1;
            }
            m_scanedCameras.back().x_res = static_cast<int>(svb_cameraProp.MaxWidth);
            m_scanedCameras.back().y_res = static_cast<int>(svb_cameraProp.MaxHeight);
            m_scanedCameras.back().mono = !static_cast<bool>(svb_cameraProp.IsColorCam);
            m_scanedCameras.back().bayer_patter = getBayerFromSVBBayer(svb_cameraProp.BayerPattern);
            for(int i = 0; i < sizeof(svb_cameraProp.SupportedVideoFormat) / sizeof(svb_cameraProp.SupportedVideoFormat[0]); i++)
            {
                if(svb_cameraProp.SupportedVideoFormat[i] == SVB_IMG_END){
                    break;
                }
                std::cout <<svb_cameraProp.SupportedVideoFormat[i] << std::endl;
                m_scanedCameras.back().img_data_types.push_back(getImageTypeFromSVBImageType(svb_cameraProp.SupportedVideoFormat[i]));
            }
            int controlsNum = 0;
	        ret = SVBGetNumOfControls(svb_cameraInfo.CameraID, &controlsNum);
            SVB_CONTROL_CAPS caps;
            for(int i = 0; i < controlsNum; i++){
                auto ret = SVBGetControlCaps(svb_cameraInfo.CameraID, i, &caps);
                if(caps.ControlType == SVB_GAIN){
                    m_scanedCameras.back().gain_range = std::make_pair<int,int>(caps.MinValue, caps.MaxValue);
                }
                else if(caps.ControlType == SVB_EXPOSURE){
                    m_scanedCameras.back().exposure_range_us = std::make_pair<int,int>(caps.MinValue, caps.MaxValue);
                }
            }
            std::cout << m_scanedCameras.back() <<std::endl;
            SVBCloseCamera(svb_cameraInfo.CameraID);
		}
	}
    return ret_val;
}

int cameraControl::SVB_applySetup(const cameraSetup& cam_setup){
    /*Setup output type*/
    int ret_val = 0;
    bool type_av = std::find(m_current_camera.img_data_types.begin(), m_current_camera.img_data_types.end(), cam_setup.img_data_type)
                            != m_current_camera.img_data_types.end();
    if(type_av){
        auto SVB_type = getSVBImageTypeFromImageDataType(cam_setup.img_data_type);
        auto ret = SVBSetOutputImageType(m_current_camera.ID, SVB_type);
        if(ret != SVB_SUCCESS){
            std::cerr << "failed to set image type!! EC:" << ret <<std::endl;
        }
        delete[] m_image_buffer;
        m_image_buffer_size = m_current_camera.x_res * m_current_camera.x_res * ImageDataTypeToBytes(cam_setup.img_data_type);
        m_image_buffer = new uint8_t[m_image_buffer_size];
    }
    else{
        std::cerr << "Image type not avaiable\r\n" << std::endl;
        ret_val = -1;
    }

    /** FIRST NEED TO CHANGE EXPOSURE TO CHANGE GAIN ----------------- OMGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG */
    bool is_in_range = cam_setup.exposure_us >= m_current_camera.exposure_range_us.first && cam_setup.exposure_us <= m_current_camera.exposure_range_us.second;
    if(is_in_range){
        auto ret =SVBSetControlValue(m_current_camera.ID, SVB_EXPOSURE, cam_setup.exposure_us, SVB_FALSE);
        if(ret != SVB_SUCCESS){
            std::cerr << "failed to set exposure!! EC:" << ret <<std::endl;
        }
    }
    else if(cam_setup.gain == -1){
        /*do nothing*/
    }
    else{
        std::cerr << "Exposure out of range!\r\n" << std::endl;
        ret_val = -1;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    is_in_range = cam_setup.gain >= m_current_camera.gain_range.first && cam_setup.gain <= m_current_camera.gain_range.second;
    if(is_in_range){
        auto ret = SVBSetControlValue(m_current_camera.ID, SVB_GAIN, cam_setup.gain, SVB_FALSE);
        if(ret != SVB_SUCCESS){
            std::cerr << "failed to set gain!! EC:" << ret <<std::endl;
        }
    }
    else if(cam_setup.gain == -1){
        /*do nothing*/
    }
    else{
        std::cerr << "Gain out of range!!\r\n" << std::endl;
        ret_val = -1;
    }



    return ret_val;
}