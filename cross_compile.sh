#!/bin/bash


SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROJECT_DIR="$SCRIPT_DIR"
CONTAINER_NAME="Guider_build_container"
IMAGE_NAME="debian:11"

# Command to run inside the container
BUILD_CMD="mkdir -p /project/build && cd /project/build && cmake .. && cmake --build ."

# Check if the container already exists
if [ "$(docker ps -aq -f name=$CONTAINER_NAME)" ]; then
  echo "✅ Container exists. Running build..."
  docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
  docker start "$CONTAINER_NAME" > /dev/null
  docker exec "$CONTAINER_NAME" bash -c "$BUILD_CMD"
else
  echo "🚀 Creating new container and running build..."
  docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
  docker run -it --platform linux/arm64 \
    --name "$CONTAINER_NAME" \
    -v "$PROJECT_DIR:/project" \
    "$IMAGE_NAME" /bin/bash -c "
      apt update && \
      apt install -y cmake build-essential git libpaho-mqtt-dev libopencv-dev && \
      $BUILD_CMD
    "
fi