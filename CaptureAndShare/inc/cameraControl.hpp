#include <string>
#include <vector>
#include "captureAndShare_types.hpp"
#include <thread>
#include <chrono>

struct cameraInfo{
    std::string producer;
    std::string cameraName;
    int ID;
    int x_res;
    int y_res;
    ImageBayerPattern bayer_patter;
    std::vector<ImageDataType> img_data_types;
    std::pair<int, int> gain_range;
    std::pair<int, int> exposure_range_us;
    bool mono;
};






class cameraControl{
  public:
    cameraControl();
    int scanForCameras();
    int openFirstAvaible();
    int openByID(int ID);
    int setupCamera(cameraSetup cam_setup);
    int startVideo();
    int stopVideo();
    const uint8_t* getImageBuffer();
    cameraInfo getCurrentCameraInfo();
  private:
    /*SVB SPECIFIC*/
    int SVB_ScanForCameras();
    int SVB_applySetup(const cameraSetup& cam_setup);
    /*ZWO SPECIFIC*/
    int ZWO_ScanForCameras();
    int ASI_applySetup(const cameraSetup& cam_setup);
    //-----------
    /*REST*/
    std::thread m_capture_video_thread;
    void captureVideoThreadFunc();
    bool m_stop_video_thread = false;
    int m_ms_per_frame = 200;
    std::chrono::steady_clock::time_point m_last_send_time;
    int scanForImage();
    bool m_new_img_ready = false;
    bool m_new_img_in_buffer = false;
    std::vector<cameraInfo> m_scanedCameras;
    bool m_camera_opened = false;
    cameraInfo m_current_camera;
    uint8_t* m_image_buffer = nullptr;
    int m_image_buffer_size = 0;
};

std::ostream& operator<<(std::ostream& os, const cameraInfo& cam);