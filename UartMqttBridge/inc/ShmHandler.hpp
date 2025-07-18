#include "captureAndShare_types.hpp"



class ShmHandler{
    public:
        ShmHandler();
        int setupCameraGainExpoInterval(int gain, int expo, int interval);
        int setupCameraRoi();
        int setupCameraDataType(ImageDataType img_data_type);
    private:
        SHM_cameraControls m_shm_camera_controls;
        SHM_cameraControls* m_shm_camera_controls_ptr;
        int m_shm_camera_controls_fd;
};