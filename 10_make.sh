#!/bin/bash
set -euo pipefail

echo "compiling..."
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 esp_sensor.ino

echo "uploading..."
arduino-cli upload -p /dev/cu.SLAB_USBtoUART --fqbn esp8266:esp8266:nodemcuv2 Simple_DS18B20_Client.ino

# screen /dev/cu.SLAB_USBtoUART 115200
# or
# minicom -D /dev/cu.SLAB_USBtoUART
