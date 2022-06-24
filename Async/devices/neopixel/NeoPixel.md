# NeoPixel Driver
The *NeoPixel* driver can be used to control a single line
of NeoPixels attached to a given GPIO pin.

To use it in your own programs, add the *neopixel* to your
target list of library dependencies.

The following demonstrate a typical usage:

Add the following include:
```cpp
#include "devices/neopixel/NeoPixel.h"
```
First, instantiate a NeoPixel device:
```cpp
// pioDevice must be pio0 or pio1. stateMachine is the
// state machine index (0 to 3)
NeoPixel np(pin, numLEDs, pioDevice, stateMachine);
```
To change the display, push the colors to the *np* object
and then send the data to the pixel chain. 
For example, for 3 pixels, to have the first one red,
the second green and the last one blue, use:
```cpp
np.clear(); // First clear previous data
np.push_back({128,0,0}); // Half brightness red
np.push_back({0,128,0}); // Green
np.push_back({0,0,128}); // Blue
co_await np.show(); // Asynchronously send the data to the pixel chain
```

Note: The library currently assume you have RGB pixels and they
expect a 800kHz signal.