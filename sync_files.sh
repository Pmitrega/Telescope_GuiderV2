#!/bin/bash

# Windows path to your project folder (adjust accordingly)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_DIR="$SCRIPT_DIR"

# Orange Pi user and IP address
REMOTE_USER="orangepi"
REMOTE_HOST="192.168.1.62"

# Remote directory on Orange Pi
REMOTE_DIR="/home/orangepi"

echo "Starting sync from $LOCAL_DIR to $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR"

rsync -avz --delete \
  --include 'webServer/' \
  --include 'webServer/dist/***' \
  --exclude 'webServer/**' \
  --exclude '.git/**' \
  "$LOCAL_DIR" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR"

if [ $? -eq 0 ]; then
  echo "Sync completed successfully!"
else
  echo "Sync failed."
fi


# # === Copy .so files to /usr/local/lib and run ldconfig ===
# REMOTE_LIB_DIR="/usr/local/lib"
# echo "🔧 Installing shared libraries to $REMOTE_LIB_DIR..."

# ssh -t "$REMOTE_USER@$REMOTE_HOST" "sudo cp $REMOTE_DIR/TelescopeGuiderV2/CaptureAndShare/libs/src/*.so* $REMOTE_LIB_DIR && sudo ldconfig"


# if [ $? -eq 0 ]; then
#   echo "✅ Libraries installed successfully!"
# else
#   echo "❌ Failed to install libraries."
# fi