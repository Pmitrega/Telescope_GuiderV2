import mmap
import struct
import numpy as np
import time
import paho.mqtt.client as mqtt
import cv2  # OpenCV for JPEG encoding
import os

# POSIX shared memory name
SHM_NAME = "/guider_image"
SHM_PATH = f"/dev/shm{SHM_NAME}"

# C++ ImageInfo = 5 x int32_t = 20 bytes
IMAGE_INFO_FORMAT = "iiiii"
IMAGE_INFO_SIZE = struct.calcsize(IMAGE_INFO_FORMAT)

# MQTT setup
MQTT_BROKER = "localhost"  # Change if remote
MQTT_PORT = 1883
MQTT_TOPIC_PNG = "/guider/image_png"
MQTT_TOPIC_JPG = "/guider/image_jpg"
MQTT_FORMAT_TOPIC = "/guider/format"

selected_format = "jpg"



# Called when a message is received
def on_message(client, userdata, msg):
    global selected_format  # So we can modify the outer variable
    try:
        payload = msg.payload.decode("utf-8").strip().lower()
        print(f"Received: {payload} on topic {msg.topic}")

        # Only handle messages from /guider/format
        if msg.topic == "/guider/format":
            if payload in ["jpg", "png", "bmp", "webp"]:
                selected_format = payload
                print(f"Selected format updated to: {selected_format}")
            else:
                print(f"Ignored unsupported format: {payload}")
        else:
            print(f"Ignored message from unexpected topic: {msg.topic}")
    except Exception as e:
        print(f"Error processing message: {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker.")
        client.subscribe(MQTT_FORMAT_TOPIC)
        print(f"Subscribed to {MQTT_FORMAT_TOPIC}")
    else:
        print(f"Failed to connect, return code {rc}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

def read_image_from_shm(shm):
    try:
        shm.seek(0)
        header = shm.read(IMAGE_INFO_SIZE)
        if len(header) != IMAGE_INFO_SIZE:
            raise ValueError("Incomplete header data")
        ID, x_size, y_size, data_type, bayer = struct.unpack(IMAGE_INFO_FORMAT, header)
        # print(x_size, " ", y_size, " ", data_type)
        bytes_per_pixel = {0:3, 1:2, 2:1, 3:1, 4:2}[data_type]
        buffer_size = x_size * y_size * bytes_per_pixel

        raw_data = shm.read(buffer_size)
        if len(raw_data) != buffer_size:
            raise ValueError("Incomplete image data")

        if data_type == 0:  # RGB24
            image = np.frombuffer(raw_data, dtype=np.uint8).reshape((y_size, x_size, 3))
            # Assuming image is in BGR format
            b,g,r = cv2.split(image)
            # Apply gain correction
            b = cv2.multiply(b, 1.0)  # reduce blue
            g = cv2.multiply(g, 1.00)  # keep green
            r = cv2.multiply(r, 1.00)  # boost red slightly
            # Merge corrected channels
            image = cv2.merge([b, g, r])
        elif data_type == 1:  # RAW16
            image = np.frombuffer(raw_data, dtype=np.uint16).reshape((y_size, x_size))
            # image = (image).astype(np.uint8)  # normalize to 8-bit
            image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
        elif data_type == 2:  # RAW8
            image = np.frombuffer(raw_data, dtype=np.uint8).reshape((y_size, x_size))
            image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
        elif data_type == 3:  # Y8 grayscale 8-bit
            image = np.frombuffer(raw_data, dtype=np.uint8).reshape((y_size, x_size))
            image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
        elif data_type == 4:  # Y16 grayscale 16-bit
            image = np.frombuffer(raw_data, dtype=np.uint16).reshape((y_size, x_size))
            # image = (image / 256).astype(np.uint8)
            image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
        else:
            raise ValueError(f"Unsupported data type: {data_type}")

            # ...rest of image decoding...
    except (ValueError, struct.error, KeyError) as e:
        print(f"Error reading shared memory: {e}")
        return None, None

    return ID, image


with open(SHM_PATH, "rb") as shm_file:
    shm = mmap.mmap(shm_file.fileno(), 0, access=mmap.ACCESS_READ)
    last_id = -1
    try:
        while True:
            ID, image = read_image_from_shm(shm)
            if ID is None or image is None:
                time.sleep(0.1)
                continue
            if ID != last_id:
                start_time = time.perf_counter()
                last_id = ID
                
                # Encode image based on selected_format
                if selected_format == "jpg":
                    if image.dtype == 'uint16':
                        # Normalize to uint8 by scaling from [0, 65535] to [0, 255]
                        image = (image / 256).astype('uint8')  
                    success, encoded = cv2.imencode(".jpg", image, [int(cv2.IMWRITE_JPEG_QUALITY), 90])
                    topic = MQTT_TOPIC_JPG
                elif selected_format == "png":
                    success, encoded = cv2.imencode(".png", image, [int(cv2.IMWRITE_PNG_COMPRESSION), 3])
                    topic = MQTT_TOPIC_PNG
                else:
                    print(f"Unsupported format: {selected_format}")
                    continue

                if success:
                    client.publish(topic, encoded.tobytes(), qos=0)
                    print(f"Published {selected_format.upper()} image ID {ID} to {topic}")
                else:
                    print(f"{selected_format.upper()} encoding failed")
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("Stopping.")
    finally:
        shm.close()
        client.disconnect()
