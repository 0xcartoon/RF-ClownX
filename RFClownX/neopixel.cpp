/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */
   
#include "setting.h"
#include "config.h"

extern Adafruit_NeoPixel pixels;

void setNeoPixelColour(const std::string& colour) {
  uint32_t colorValue = 0; 

  if (colour == "red") {
    colorValue = pixels.Color(5, 0, 0);
  } else if (colour == "green") {
    colorValue = pixels.Color(0, 5, 0);
  } else if (colour == "blue") {
    colorValue = pixels.Color(0, 0, 5);
  } else if (colour == "yellow") {
    colorValue = pixels.Color(5, 5, 0);
  } else if (colour == "purple") {
    colorValue = pixels.Color(5, 0, 5);
  } else if (colour == "cyan") {
    colorValue = pixels.Color(0, 5, 5);
  } else if (colour == "white") {
    colorValue = pixels.Color(5, 5, 5);
  } else if (colour == "null") {
    colorValue = pixels.Color(0, 0, 0);
  }

  pixels.setPixelColor(0, colorValue);
  pixels.show();
}
