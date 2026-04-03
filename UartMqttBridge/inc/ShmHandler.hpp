#include "captureAndShare_types.hpp"



class ShmHandler{
    public:
        ShmHandler();
        int setupCameraGainExpoInterval(int gain, int expo, int interval);
        int setupCameraRoi();
        int setupCameraDataType(ImageDataType img_data_type);
        void readMiscInfo(Misc_Info& misc_info);
        void readCameraInfo(SHM_cameraInfo& cam_info);
        void readMotorCtrlReq(SHM_MotorControl& motor_ctrl_req);
    private:
        SHM_cameraControls m_shm_camera_controls;
        SHM_cameraControls* m_shm_camera_controls_ptr;
        Misc_Info* m_shm_misc_info_ptr;
        Misc_Info m_shm_misc_info;
        SHM_cameraInfo* m_shm_camera_info_ptr;
        SHM_cameraInfo m_shm_camera_info;
        SHM_MotorControl* m_shm_motor_control_ptr;
        SHM_MotorControl m_shm_motor_control_info;
        SHM_DetectedStarsInfo* m_shm_detected_stars_ptr;
        SHM_DetectedStarsInfo m_shm_detected_stars;
};