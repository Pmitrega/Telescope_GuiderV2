#include "captureAndShare_types.hpp"
#include <cstdint>


class senderReader {       // The class
  public:             
    senderReader(ImageInfo img_info);
    ~senderReader();
    int setupBuffer(ImageInfo img_info);
    int sendImageData();
    int setImageData(const uint8_t* src);
    bool newSetupRequested(cameraSetup* cam_controls);
    int modifyBufferSize(ImageInfo img_info);
  private:
    int m_guider_image_shm_fd;
    void* m_guider_image_shm_ptr = nullptr;
    int m_guider_camera_controls_shm_fd;
    SHM_cameraControls* m_guider_camera_controls_shm_ptr = nullptr;
    int m_guider_camera_setup_shm_fd;
    uint8_t* m_image_buffer = nullptr;
    int m_buffer_size = 0;
    ImageInfo m_image_info;

    int m_debug_val = 0;
};