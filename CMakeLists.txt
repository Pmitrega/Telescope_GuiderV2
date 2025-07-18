cmake_minimum_required(VERSION 3.18)
project(TelescopeGuider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Specify the cross compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Tell CMake to statically link by default
set(CMAKE_EXE_LINKER_FLAGS "")

# === Capture and Share ===

set(CAPTURE_AND_SHARE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/CaptureAndShare
)

set(CAPTURE_AND_SHARE_LIB_INC_DIR
    ${CAPTURE_AND_SHARE_DIR}/libs/inc
)

set(CAPTURE_AND_SHARE_SOURCES
    ${CAPTURE_AND_SHARE_DIR}/main.cpp
    ${CAPTURE_AND_SHARE_DIR}/src/cameraControl.cpp
    ${CAPTURE_AND_SHARE_DIR}/src/senderReader.cpp
    ${CAPTURE_AND_SHARE_DIR}/src/worker.cpp
)

set(CAPTURE_AND_SHARE_INCLUDES
    ${CAPTURE_AND_SHARE_DIR}/inc
    ${CAPTURE_AND_SHARE_DIR}/inc_shr
)


add_executable(CaptureAndShare ${CAPTURE_AND_SHARE_SOURCES})
target_include_directories(CaptureAndShare
                             PRIVATE
                            ${CAPTURE_AND_SHARE_INCLUDES}
                            ${CAPTURE_AND_SHARE_LIB_INC_DIR})

# === Path to your shared libraries
set(LIB_DIR ${CAPTURE_AND_SHARE_DIR}/libs/src)

# Tell linker where to find the .so files at build time
link_directories(${LIB_DIR})
find_package(Threads REQUIRED)

target_link_libraries(CaptureAndShare
    PRIVATE
    "${LIB_DIR}/libSVBCameraSDK.so"
    "${LIB_DIR}/libusb-1.0.so"
    rt  # add this line
    Threads::Threads
)


# === \Uart Mqtt bridge ===

set(UART_MQTT_BRIDGE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/UartMqttBridge
)

set(UART_MQTT_BRIDGE_SOURCES
    ${UART_MQTT_BRIDGE_DIR}/main.cpp
    ${UART_MQTT_BRIDGE_DIR}/src/MQTTHandler.cpp
    ${UART_MQTT_BRIDGE_DIR}/src/ShmHandler.cpp
)

set(UART_MQTT_BRIDGE_INCLUDES
    ${UART_MQTT_BRIDGE_DIR}/inc
    ${CAPTURE_AND_SHARE_DIR}/inc_shr
)


add_executable(UartMqttBridge ${UART_MQTT_BRIDGE_SOURCES})

target_include_directories(UartMqttBridge
                             PRIVATE
                            ${UART_MQTT_BRIDGE_INCLUDES})


target_link_libraries(UartMqttBridge
    PRIVATE
    paho-mqtt3c
    pthread
    rt
)
