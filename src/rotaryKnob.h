#pragma once
#include <Arduino.h>

  class rotaryKnob {
    public:
      rotaryKnob(byte _Apin, byte _Bpin, byte _Cpin, bool _bufferTurns); // declare constructor
      void invertDirection(); // declare function to swap A/B pins
      void update();
      int  getTurnFromBuffer(); // positive = counterclockwise
      int  getClick();
      int  getValueInTurnBuffer();
      byte getApin();
      byte getBpin();
      byte getCpin();
      int  getKnobState();
    private:
      int  turnBuffer;
      int  clickBuffer;
      byte Apin;
      byte Bpin;
      byte Cpin;
      byte state;
      byte press;
      bool clicked;
      bool bufferTurns;
  };