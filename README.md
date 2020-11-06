# Polaris-Arduino
Arduino application for Polaris IoT platform

## Notes
To install Polaris board support package in Arduino IDE see: https://github.com/fortebit/fortebit-openiot-arduino-boards

When you compile the software, make sure you enable floating point support in the "C Runtime Library" settings and you select the "Modem Model" that matches your Polaris board variant (2G, 3G, NB-IoT).

Before compiling, copy the `tracker.h.example` file to `tracker.h` and customize it to your liking.
