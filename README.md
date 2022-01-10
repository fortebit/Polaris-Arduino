# Polaris-Arduino
Arduino application for Polaris IoT platform

## Build Requirements
To install Polaris board support package in Arduino IDE see: https://github.com/fortebit/fortebit-openiot-arduino-boards

When you compile the software, make sure you enable floating point support in the "C Runtime Library" settings and you select the "Modem Model" that matches your Polaris board variant (2G, 3G, NB-IoT).

Before compiling, copy the `tracker.h.example` file to `tracker.h` and customize it to your liking.
Use  `tracker.h.legacy` as a base if you need compatibility with the legacy OpenTracker protocol.

### Notes

For further customization you can copy  `addon.ino.example` to `addon.ino` and enable the add-on interface in your `tracker.h` with:

```cpp
#define ADDON_INTERFACE 1
```

The provided examples both connect to the Polaris Cloud service via HTTP requests, either using the new JSON-based protocol or the legacy OpenTracker protocol with sensor data extension.
