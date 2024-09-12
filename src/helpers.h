	#pragma once
	#include <Arduino.h>
	#include "constants.h"

// @helpers
  /*
    C++ returns a negative value for 
    negative N % D. This function
    guarantees the mod value is always
    positive.
  */
  int positiveMod(int n, int d) {
    return (((n % d) + d) % d);
  }
  /*
    There may already exist linear interpolation
    functions in the standard library. This one is helpful
    because it will do the weighting division for you.
    It only works on byte values since it's intended
    to blend color values together. A better C++
    coder may be able to allow automatic type casting here.
  */
  byte byteLerp(byte xOne, byte xTwo, float yOne, float yTwo, float y) {
    float weight = (y - yOne) / (yTwo - yOne);
    int temp = xOne + ((xTwo - xOne) * weight);
    if (temp < xOne) {temp = xOne;}
    if (temp > xTwo) {temp = xTwo;}
    return temp;
  }

// @diagnostics
  /*
    This section of the code handles
    optional sending of log messages
    to the Serial port
  */
  void sendToLog(std::string msg) {
    if (DIAGNOSTICS_ON) {
      Serial.println(msg.c_str());
    }
  }