import sys
import mmap
import struct
import os

if len(sys.argv) < 4 or len(sys.argv) > 5:
    print("Usage: python3 set_camera_controls.py <exposure> <gain> <interval> [data_type]")
    sys.exit(1)

exposure = int(sys.argv[1])
gain = int(sys.argv[2])
interval = int(sys.argv[3])

# Optional fourth argument
data_type_map = {
    "RGB24": 0,
    "RAW16": 1,
    "RAW8": 2,
    "Y8": 3,
    "Y16": 4,
    "UNKNOWN": 5
}
data_type = data_type_map.get(sys.argv[4].upper(), 5) if len(sys.argv) == 5 else 5

# Path to existing shared memory file
SHM_PATH = "/dev/shm/camera_setup"
SHM_SIZE = 32  # total size of SHM_cameraControls

# FIXED STRUCT FORMAT: use padding after bool (1 byte + 3 bytes pad + 7 ints)
STRUCT_FORMAT = "=B3x7i"  # B = unsigned char (bool), 3x = padding, 7i = 7 ints

try:
    with open(SHM_PATH, "r+b") as f:
        shm = mmap.mmap(f.fileno(), SHM_SIZE, access=mmap.ACCESS_WRITE)

        # Pack and write data: updated=True, then exposure, gain, interval, and ROIs
        packed = struct.pack(STRUCT_FORMAT,
                             True,
                             gain,
                             exposure,
                             interval,
                             data_type,
                            -1, -1, -1)  # ROI values

        shm.seek(0)
        shm.write(packed)
        shm.flush()
        shm.close()

    print("✅ Wrote camera settings to shared memory.")

except FileNotFoundError:
    print("❌ Shared memory file not found:", SHM_PATH)
except Exception as e:
    print("❌ Error:", e)