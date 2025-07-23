#!/bin/bash
sudo cp captureAndShare.service /etc/systemd/system
sudo cp uartmqttbridge.service /etc/systemd/system
sudo cp imagetransmit.service /etc/systemd/system
sudo systemctl enable captureAndShare.service 
sudo systemctl enable uartmqttbridge.service
sudo systemctl enable imagetransmit.service
sudo systemctl start captureAndShare.service 
sudo systemctl start uartmqttbridge.service
sudo systemctl start imagetransmit.service
sudo systemctl status captureAndShare.service 
sudo systemctl status uartmqttbridge.service
sudo systemctl status imagetransmit.service