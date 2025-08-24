#include "captureAndShare_types.hpp"



class ShmHandler{
    public:
        ShmHandler();
        int setupCameraGainExpoInterval(int gain, int expo, int interval);
        int setupCameraRoi();
        int setupCameraDataType(ImageDataType img_data_type);
        void readMiscInfo(Misc_Info& misc_info);
        void readCameraInfo(SHM_cameraInfo& cam_info);
    private:
        SHM_cameraControls m_shm_camera_controls;
        SHM_cameraControls* m_shm_camera_controls_ptr;
        Misc_Info* m_shm_misc_info_ptr;
        Misc_Info m_shm_misc_info;
        SHM_cameraInfo* m_shm_camera_info_ptr;
        SHM_cameraInfo m_shm_camera_info;
};