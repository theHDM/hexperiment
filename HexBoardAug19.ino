// @readme
  /*
    HexBoard
    Copyright 2022-2023 Jared DeCook and Zach DeCook
    with help from Nicholas Fox
    Licensed under the GNU GPL Version 3.

    Hardware information:
      Generic RP2040 running at 133MHz with 16MB of flash
        https://github.com/earlephilhower/arduino-pico
      Additional board manager URL:
        https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
      Tools > USB Stack > (Adafruit TinyUSB)
      Sketch > Export Compiled Binary

    Compilation instructions:
      Using arduino-cli...
        # Download the board index
        arduino-cli --additional-urls=https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json core update-index
        # Install the core for rp2040
        arduino-cli --additional-urls=https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json core download rp2040:rp2040
        arduino-cli --additional-urls=https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json core install rp2040:rp2040
        # Install libraries
        arduino-cli lib install "MIDI library"
        arduino-cli lib install "Adafruit NeoPixel"
        arduino-cli lib install "U8g2" # dependency for GEM
        arduino-cli lib install "Adafruit GFX Library" # dependency for GEM
        arduino-cli lib install "GEM"
        sed -i 's@#include "config/enable-glcd.h"@//\0@g' ~/Arduino/libraries/GEM/src/config.h # remove dependency from GEM
        # Run Make to build the firmware
        make
    ---------------------------
    New to programming Arduino?
    ---------------------------
    Coding the Hexboard is, basically, done in C++.
    
    When the HexBoard is plugged in, it runs
    void setup() and void setup1(), then
    runs void loop() and void loop1() on an
    infinite loop until the HexBoard powers down.
    There are two cores running independently.
    You can pretend that the compiler tosses
    these two routines inside an int main() for
    each processor.
  
    To #include libraries, the Arduino
    compiler expects them to be installed from
    a centralized repository. You can also bring
    your own .h / .cpp code but it must be saved
    in "/src/____/___.h" to be valid.

    We found this really annoying so to the
    extent possible we have consolidated
    this code into one single .ino sketch file.
    However, the code is sectioned into something
    like a library format for each feature
    of the HexBoard, so that if the code becomes
    too long to manage in a single file in the
    future, it is easier to air-lift parts of
    the code into a library at that point.
  */
// @init
  #include <Arduino.h>            // this is necessary to talk to the Hexboard!
  #include <Wire.h>               // this is necessary to deal with the pins and wires
  #include <GEM_u8g2.h>           // library of code to create menu objects on the B&W display
  #include <numeric>              // need that GCD function, son
  #include <string>               // standard C++ library string classes (use "std::string" to invoke it); these do not cause the memory corruption that Arduino::String does.
  #include <queue>                // standard C++ library construction to store open channels in microtonal mode (use "std::queue" to invoke it)
  #include "src/rotaryKnob.h"
  #include "src/softTimer.h"     // library of code to access the processor's clock functions
  #include "src/constants.h"
  #include "src/helpers.h"
  #include "src/microtonal.h"

// @defaults
  /*
    This section sets default values
    for user-editable options
  */
  byte Hardware_Version = 0;       // 0 = unknown, 1 = v1.1 board. 2 = v1.2 board.
  int transposeSteps = 0;
  byte scaleLock = 0;
  byte perceptual = 1;
  byte paletteBeginsAtKeyCenter = 1;
  byte animationFPS = 32;             // actually frames per 2^20 microseconds. close enough to 30fps

  byte wheelMode = 0;                 // standard vs. fine tune mode
  byte modSticky = 0;
  byte pbSticky = 0;
  byte velSticky = 1;
  int modWheelSpeed = 8;
  int pbWheelSpeed = 1024;
  int velWheelSpeed = 8;

  byte playbackMode = SYNTH_OFF;
  byte currWave = WAVEFORM_HYBRID;
  byte colorMode = RAINBOW_MODE;
  byte animationType = ANIMATE_NONE;
  byte globalBrightness = BRIGHT_MID;

  presetDef current = {
    "Default",      // name
    TUNING_12EDO,   // tuning
    0,              // default to the first layout, wicki hayden
    0,              // default to using no scale (chromatic)
    -9,             // default to the key of C, which in 12EDO is -9 steps from A.
    0               // default to no transposition
  };



// @timing
  /*
    This section of the code handles basic
    timekeeping stuff
  */
  uint64_t runTime = 0;                // Program loop consistent variable for time in microseconds since power on
  uint64_t lapTime = 0;                // Used to keep track of how long each loop takes. Useful for rate-limiting.
  uint64_t loopTime = 0;               // Used to check speed of the loop
  void timeTracker() {
    lapTime = runTime - loopTime;
    loopTime = runTime;                                 // Update previousTime variable to give us a reference point for next loop
    runTime = getTheCurrentTime();   // Store the current time in a uniform variable for this program loop
  }

// @fileSystem
  /*
    This section of the code handles the
    file system. There isn't much being
    done with it yet, per se.
    If so, this section might be relocated
  */
  #include "LittleFS.h"       // code to use flash drive space as a file system -- not implemented yet, as of May 2024
  void setupFileSystem() {
    Serial.begin(115200);     // Set serial to make uploads work without bootsel button
    LittleFSConfig cfg;       // Configure file system defaults
    cfg.setAutoFormat(true);  // Formats file system if it cannot be mounted.
    LittleFS.setConfig(cfg);
    LittleFS.begin();         // Mounts file system.
    if (!LittleFS.begin()) {
      sendToLog("An Error has occurred while mounting LittleFS");
    } else {
      sendToLog("LittleFS mounted OK");
    }
  }

// @gridSystem
  /*
    This section of the code handles the hex grid
       Hexagonal coordinates
         https://www.redblobgames.com/grids/hexagons/
         http://ondras.github.io/rot.js/manual/#hex/indexing
    The HexBoard contains a grid of 140 buttons with
    hexagonal keycaps. The processor has 10 pins connected
    to a multiplexing unit, which hotswaps between the 14 rows
    of ten buttons to allow all 140 inputs to be read in one
    program read cycle.
  */



  /*
    This class defines the hexagon button
    as an object. It stores all real-time
    properties of the button -- its coordinates,
    its current pressed state, the color
    codes to display based on what action is
    taken, what note and frequency is assigned,
    whether the button is a command or not,
    whether the note is in the selected scale,
    whether the button is flagged to be animated,
    and whether the note is currently 
    sounding on MIDI / the synth.
   
    Needless to say, this is an important class.
  */
  class buttonDef {
  public:
    #define BTN_STATE_OFF 0
    #define BTN_STATE_NEWPRESS 1
    #define BTN_STATE_RELEASED 2
    #define BTN_STATE_HELD 3
    byte     btnState = 0;        // binary 00 = off, 01 = just pressed, 10 = just released, 11 = held
    void interpBtnPress(bool isPress) {
      btnState = (((btnState << 1) + isPress) & 3);
    }
    int8_t   coordRow = 0;        // hex coordinates
    int8_t   coordCol = 0;        // hex coordinates
    uint64_t timePressed = 0;     // timecode of last press
    uint32_t LEDcodeAnim = 0;     // calculate it once and store value, to make LED playback snappier 
    uint32_t LEDcodePlay = 0;     // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeRest = 0;     // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeOff = 0;      // calculate it once and store value, to make LED playback snappier
    uint32_t LEDcodeDim = 0;      // calculate it once and store value, to make LED playback snappier
    bool     animate = 0;         // hex is flagged as part of the animation in this frame, helps make animations smoother
    int16_t  stepsFromC = 0;      // number of steps from C4 (semitones in 12EDO; microtones if >12EDO)
    bool     isCmd = 0;           // 0 if it's a MIDI note; 1 if it's a MIDI control cmd
    bool     inScale = 0;         // 0 if it's not in the selected scale; 1 if it is
    byte     note = UNUSED_NOTE;  // MIDI note or control parameter corresponding to this hex
    int16_t  bend = 0;            // in microtonal mode, the pitch bend for this note needed to be tuned correctly
    byte     MIDIch = 0;          // what MIDI channel this note is playing on
    byte     synthCh = 0;         // what synth polyphony ch this is playing on
    float    frequency = 0.0;     // what frequency to ring on the synther
  };
  /*
    This class is like a virtual wheel.
    It takes references / pointers to 
    the state of three command buttons,
    translates presses of those buttons
    into wheel turns, and converts
    these movements into corresponding
    values within a range.
   
    This lets us generalize the
    behavior of a virtual pitch bend
    wheel or mod wheel using the same
    code, only needing to modify the
    range of output and the connected
    buttons to operate it.
  */
  class wheelDef {
  public:
    byte* alternateMode; // two ways to control
    byte* isSticky;      // TRUE if you leave value unchanged when no buttons pressed
    byte* topBtn;        // pointer to the key Status of the button you use as this button
    byte* midBtn;
    byte* botBtn;
    int16_t minValue;
    int16_t maxValue;
    int* stepValue;      // this can be changed via GEM menu
    int16_t defValue;    // snapback value
    int16_t curValue;
    int16_t targetValue;
    uint64_t timeLastChanged;
    void setTargetValue() {
      if (*alternateMode) {
        if (*midBtn >> 1) { // middle button toggles target (0) vs. step (1) mode
          int16_t temp = curValue;
              if (*topBtn == 1)     {temp += *stepValue;} // tap button
              if (*botBtn == 1)     {temp -= *stepValue;} // tap button
              if (temp > maxValue)  {temp  = maxValue;} 
          else if (temp <= minValue) {temp  = minValue;}
          targetValue = temp;
        } else {
          switch (((*topBtn >> 1) << 1) + (*botBtn >> 1)) {
            case 0b10:   targetValue = maxValue;     break;
            case 0b11:   targetValue = defValue;     break;
            case 0b01:   targetValue = minValue;     break;
            default:     targetValue = curValue;     break;
          }
        }
      } else {
        switch (((*topBtn >> 1) << 2) + ((*midBtn >> 1) << 1) + (*botBtn >> 1)) {
          case 0b100:  targetValue = maxValue;                         break;
          case 0b110:  targetValue = (3 * maxValue + minValue) / 4;    break;
          case 0b010:
          case 0b111:
          case 0b101:  targetValue = (maxValue + minValue) / 2;        break;
          case 0b011:  targetValue = (maxValue + 3 * minValue) / 4;    break;
          case 0b001:  targetValue = minValue;                         break;
          case 0b000:  targetValue = (*isSticky ? curValue : defValue); break;
          default: break;
        }
      }
    }
    bool updateValue(uint64_t givenTime) {
      int16_t temp = targetValue - curValue;
      if (temp != 0) {
        if ((givenTime - timeLastChanged) >= CC_MSG_COOLDOWN_MICROSECONDS ) {
          timeLastChanged = givenTime;
          if (abs(temp) < *stepValue) {
            curValue = targetValue;
          } else {
            curValue = curValue + (*stepValue * (temp / abs(temp)));
          }
          return 1;
        } else {
          return 0;
        }
      } else {
        return 0;
      }
    }   
  };
  const byte mPin[] = { 
    MPLEX_1_PIN, MPLEX_2_PIN, MPLEX_4_PIN, MPLEX_8_PIN 
  };
  const byte cPin[] = { 
    COLUMN_PIN_0, COLUMN_PIN_1, COLUMN_PIN_2, COLUMN_PIN_3,
    COLUMN_PIN_4, COLUMN_PIN_5, COLUMN_PIN_6, 
    COLUMN_PIN_7, COLUMN_PIN_8, COLUMN_PIN_9 
  };
  const byte assignCmd[] = { 
    CMDBTN_0, CMDBTN_1, CMDBTN_2, CMDBTN_3, 
    CMDBTN_4, CMDBTN_5, CMDBTN_6
  };

  /*
    define h, which is a collection of all the 
    buttons from 0 to 139. h[i] refers to the 
    button with the LED address = i.
  */
  buttonDef h[BTN_COUNT];
  
  wheelDef modWheel = { &wheelMode, &modSticky,
    &h[assignCmd[4]].btnState, &h[assignCmd[5]].btnState, &h[assignCmd[6]].btnState,
    0, 127, &modWheelSpeed, 0, 0, 0, 0
  };
  wheelDef pbWheel =  { &wheelMode, &pbSticky, 
    &h[assignCmd[4]].btnState, &h[assignCmd[5]].btnState, &h[assignCmd[6]].btnState,
    -8192, 8191, &pbWheelSpeed, 0, 0, 0, 0
  };
  wheelDef velWheel = { &wheelMode, &velSticky, 
    &h[assignCmd[0]].btnState, &h[assignCmd[1]].btnState, &h[assignCmd[2]].btnState,
    0, 127, &velWheelSpeed, 96, 96, 96, 0
  };
  
  bool toggleWheel = 0; // 0 for mod, 1 for pb

  void setupPins() {
    for (byte p = 0; p < sizeof(cPin); p++) { // For each column pin...
      pinMode(cPin[p], INPUT_PULLUP);         // set the pinMode to INPUT_PULLUP (+3.3V / HIGH).
    }
    for (byte p = 0; p < sizeof(mPin); p++) { // For each column pin...
      pinMode(mPin[p], OUTPUT);               // Setting the row multiplexer pins to output.
    }
    sendToLog("Pins mounted");
  }

  void setupGrid() {
    for (byte i = 0; i < BTN_COUNT; i++) {
      h[i].coordRow = (i / 10);
      h[i].coordCol = (2 * (i % 10)) + (h[i].coordRow & 1);
      h[i].isCmd = 0;
      h[i].note = UNUSED_NOTE;
      h[i].btnState = 0;
    }
    for (byte c = 0; c < CMDCOUNT; c++) {
      h[assignCmd[c]].isCmd = 1;
      h[assignCmd[c]].note = CMDB + c;
    }
    // "flag" buttons
    for (byte i = 140; i < BTN_COUNT; i++) {
      h[i].isCmd = 1;
    }
    // On version 1.2, "button" 140 is shorted (always connected)
    h[140].note = HARDWARE_V1_2;
  }

// @LED
  /*
    This section of the code handles sending
    color data to the LED pixels underneath
    the hex buttons.
  */
  #include <Adafruit_NeoPixel.h>  // library of code to interact with the LED array
  #define LED_PIN 22

  Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);  
  int32_t rainbowDegreeTime = 65'536; // microseconds to go through 1/360 of rainbow
  /*
    This is actually a hacked together approximation
    of the color space OKLAB. A true conversion would
    take the hue, saturation, and value bits and
    turn them into linear RGB to feed directly into
    the LED class. This conversion is... not very OK...
    but does the job for now. A proper implementation
    of OKLAB is in the works.
   
    For transforming hues, the okLAB hue degree (0-360) is
    mapped to the RGB hue degree from 0 to 65535, using
    simple linear interpolation I created by hand comparing
    my HexBoard outputs to a Munsell color chip book.
  */
  int16_t transformHue(float h) {
    float D = fmod(h,360);
    if (!perceptual) {
      return 65536 * D / 360;
    } else {
      //                red            yellow             green        cyan         blue
      int hueIn[] =  {    0,    9,   18,  102,  117,  135,  142,  155,  203,  240,  252,  261,  306,  333,  360};
      //              #ff0000          #ffff00           #00ff00      #00ffff     #0000ff     #ff00ff
      int hueOut[] = {    0, 3640, 5861,10922,12743,16384,21845,27306,32768,38229,43690,49152,54613,58254,65535};
      byte B = 0;
      while (D - hueIn[B] > 0) {
        B++;
      }
      float T = (D - hueIn[B - 1]) / (float)(hueIn[B] - hueIn[B - 1]);
      return (hueOut[B - 1] * (1 - T)) + (hueOut[B] * T);
    }
  }
  /*
    Saturation and Brightness are taken as is (already in a 0-255 range).
    The global brightness / 255 attenuates the resulting color for the
    user's brightness selection. Then the resulting RGB (HSV) color is
    "un-gamma'd" to be converted to the LED strip color.
  */
  uint32_t getLEDcode(colorDef c) {
    return strip.gamma32(strip.ColorHSV(transformHue(c.hue),c.sat,c.val * globalBrightness / 255));
  }
  /*
    This function cycles through each button, and based on what color
    palette is active, it calculates the LED color code in the palette, 
    plus its variations for being animated, played, or out-of-scale, and
    stores it for recall during playback and animation. The color
    codes remain in the object until this routine is called again.
  */
  void setLEDcolorCodes() {
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        colorDef setColor;
        byte paletteIndex = positiveMod(h[i].stepsFromC,current.tuning().cycleLength);
        if (paletteBeginsAtKeyCenter) {
          paletteIndex = current.keyDegree(paletteIndex);
        }
        switch (colorMode) {
          case TIERED_COLOR_MODE: // This mode sets the color based on the palettes defined above.
            setColor = palette[current.tuningIndex].getColor(paletteIndex);
            break;
          case RAINBOW_MODE:      // This mode assigns the root note as red, and the rest as saturated spectrum colors across the rainbow.
            setColor = 
              { 360 * ((float)paletteIndex / (float)current.tuning().cycleLength)
              , SAT_VIVID
              , VALUE_NORMAL
              };
            break;
          case ALTERNATE_COLOR_MODE:
            // This mode assigns each note a color based on the interval it forms with the root note.
            // This is an adaptation of an algorithm developed by Nicholas Fox and Kite Giedraitis.
            float cents = current.tuning().stepSize * paletteIndex;
            bool perf = 0;
            float center = 0.0;
                   if                    (cents <   50)  {perf = 1; center =    0.0;}
              else if ((cents >=  50) && (cents <  250)) {          center =  147.1;}
              else if ((cents >= 250) && (cents <  450)) {          center =  351.0;}
              else if ((cents >= 450) && (cents <  600)) {perf = 1; center =  498.0;}
              else if ((cents >= 600) && (cents <= 750)) {perf = 1; center =  702.0;}
              else if ((cents >  750) && (cents <= 950)) {          center =  849.0;}
              else if ((cents >  950) && (cents <=1150)) {          center = 1053.0;}
              else if ((cents > 1150) && (cents < 1250)) {perf = 1; center = 1200.0;}
              else if ((cents >=1250) && (cents < 1450)) {          center = 1347.1;}
              else if ((cents >=1450) && (cents < 1650)) {          center = 1551.0;}
              else if ((cents >=1650) && (cents < 1850)) {perf = 1; center = 1698.0;}
              else if ((cents >=1800) && (cents <=1950)) {perf = 1; center = 1902.0;}
            float offCenter = cents - center;
            int16_t altHue = positiveMod((int)(150 + (perf * ((offCenter > 0) ? -72 : 72)) - round(1.44 * offCenter)), 360);
            float deSaturate = perf * (abs(offCenter) < 20) * (1 - (0.02 * abs(offCenter)));
            setColor = { 
              (float)altHue, 
              (byte)(255 - round(255 * deSaturate)), 
              (byte)(cents ? VALUE_SHADE : VALUE_NORMAL) };
            break;
        }
        h[i].LEDcodeRest   = getLEDcode(setColor);
        h[i].LEDcodePlay = getLEDcode(setColor.tint()); 
        h[i].LEDcodeDim  = getLEDcode(setColor.shade());  
        setColor = {HUE_NONE,SAT_BW,VALUE_BLACK};
        h[i].LEDcodeOff  = getLEDcode(setColor);                // turn off entirely
        h[i].LEDcodeAnim = h[i].LEDcodePlay;
      }
    }
    sendToLog("LED codes re-calculated.");
  }

  void resetVelocityLEDs() {
    colorDef tempColor = 
      { (runTime % (rainbowDegreeTime * 360)) / (float)rainbowDegreeTime
      , SAT_MODERATE
      , byteLerp(0,255,85,127,velWheel.curValue)
      };
    strip.setPixelColor(assignCmd[0], getLEDcode(tempColor));

    tempColor.val = byteLerp(0,255,42,85,velWheel.curValue);
    strip.setPixelColor(assignCmd[1], getLEDcode(tempColor));
    
    tempColor.val = byteLerp(0,255,0,42,velWheel.curValue);
    strip.setPixelColor(assignCmd[2], getLEDcode(tempColor));
  }
  void resetWheelLEDs() {
    // middle button
    byte tempSat = SAT_BW;
    colorDef tempColor = {HUE_NONE, tempSat, (byte)(toggleWheel ? VALUE_SHADE : VALUE_LOW)};
    strip.setPixelColor(assignCmd[3], getLEDcode(tempColor));
    if (toggleWheel) {
      // pb red / green
      tempSat = byteLerp(SAT_BW,SAT_VIVID,0,8192,abs(pbWheel.curValue));
      tempColor = {(float)((pbWheel.curValue > 0) ? HUE_RED : HUE_CYAN), tempSat, VALUE_FULL};
      strip.setPixelColor(assignCmd[5], getLEDcode(tempColor));

      tempColor.val = tempSat * (pbWheel.curValue > 0);
      strip.setPixelColor(assignCmd[4], getLEDcode(tempColor));

      tempColor.val = tempSat * (pbWheel.curValue < 0);
      strip.setPixelColor(assignCmd[6], getLEDcode(tempColor));
    } else {
      // mod blue / yellow
      tempSat = byteLerp(SAT_BW,SAT_VIVID,0,64,abs(modWheel.curValue - 63));
      tempColor = {
        (float)((modWheel.curValue > 63) ? HUE_YELLOW : HUE_INDIGO), 
        tempSat, 
        (byte)(127 + (tempSat / 2))
      };
      strip.setPixelColor(assignCmd[6], getLEDcode(tempColor));

      if (modWheel.curValue <= 63) {
        tempColor.val = 127 - (tempSat / 2);
      }
      strip.setPixelColor(assignCmd[5], getLEDcode(tempColor));
      
      tempColor.val = tempSat * (modWheel.curValue > 63);
      strip.setPixelColor(assignCmd[4], getLEDcode(tempColor));
    }
  }
  uint32_t applyNotePixelColor(byte x) {
           if (h[x].animate) { return h[x].LEDcodeAnim;
    } else if (h[x].MIDIch)  { return h[x].LEDcodePlay;
    } else if (h[x].inScale) { return h[x].LEDcodeRest;
    } else if (scaleLock)    { return h[x].LEDcodeOff;
    } else                   { return h[x].LEDcodeDim;
    }
  }
  void setupLEDs() { 
    strip.begin();    // INITIALIZE NeoPixel strip object
    strip.show();     // Turn OFF all pixels ASAP
    sendToLog("LEDs started..."); 
    setLEDcolorCodes();
  }
  void lightUpLEDs() {   
    for (byte i = 0; i < LED_COUNT; i++) {      
      if (!(h[i].isCmd)) {
        strip.setPixelColor(i,applyNotePixelColor(i));
      }
    }
    resetVelocityLEDs();
    resetWheelLEDs();
    strip.show();
  }

// @MIDI
  /*
    This section of the code handles all
    things related to MIDI messages.
  */
  #include <Adafruit_TinyUSB.h>   // library of code to get the USB port working
  #include <MIDI.h>               // library of code to send and receive MIDI messages

  /*
    We use pitch bends to retune notes in MPE mode.
    Some setups can adjust to fit this, but some need us to adjust it.
  */
  byte MPEpitchBendSemis = 2;
  /*
    Create a new instance of the Arduino MIDI Library,
    and attach usb_midi as the transport.
  */
  Adafruit_USBD_MIDI usb_midi;
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, UMIDI);
  MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, SMIDI);

  byte midiD = MIDID_USB | MIDID_SER;

  // What program change number we last sent (General MIDI/Roland MT-32)
  byte programChange = 0;

  std::queue<byte> MPEchQueue;
  byte MPEpitchBendsNeeded; 

  float freqToMIDI(float Hz) {             // formula to convert from Hz to MIDI note
    return 69.0 + 12.0 * log2f(Hz / 440.0);
  }
  float MIDItoFreq(float midi) {           // formula to convert from MIDI note to Hz
    return 440.0 * exp2((midi - 69.0) / 12.0);
  }
  float stepsToMIDI(int16_t stepsFromA) {  // return the MIDI pitch associated
    return freqToMIDI(CONCERT_A_HZ) + ((float)stepsFromA * (float)current.tuning().stepSize / 100.0);
  }

  void setPitchBendRange(byte Ch, byte semitones) {
    if (midiD&MIDID_USB) {
        UMIDI.beginRpn(0, Ch);
        UMIDI.sendRpnValue(semitones << 7, Ch);
        UMIDI.endRpn(Ch);
    }
    if (midiD&MIDID_SER) {
        SMIDI.beginRpn(0, Ch);
        SMIDI.sendRpnValue(semitones << 7, Ch);
        SMIDI.endRpn(Ch);
    }
    sendToLog(
      "set pitch bend range on ch " +
      std::to_string(Ch) + " to be " + 
      std::to_string(semitones) + " semitones"
    );
  }

  void setMPEzone(byte masterCh, byte sizeOfZone) {
    if (midiD&MIDID_USB) {
        UMIDI.beginRpn(6, masterCh);
        UMIDI.sendRpnValue(sizeOfZone << 7, masterCh);
        UMIDI.endRpn(masterCh);
    }
    if (midiD&MIDID_SER) {
        SMIDI.beginRpn(6, masterCh);
        SMIDI.sendRpnValue(sizeOfZone << 7, masterCh);
        SMIDI.endRpn(masterCh);
    }
    sendToLog(
      "tried sending MIDI msg to set MPE zone, master ch " +
      std::to_string(masterCh) + ", zone of this size: " + std::to_string(sizeOfZone)
    );
  }

  void resetTuningMIDI() {
    /*
      currently the only way that microtonal
      MIDI works is via MPE (MIDI polyphonic expression).
      This assigns re-tuned notes to an independent channel
      so they can be pitched separately.
    
      if operating in a standard 12-EDO tuning, or in a
      tuning with steps that are all exact multiples of
      100 cents, then MPE is not necessary.
    */
    if (current.tuning().stepSize == 100.0) {
      MPEpitchBendsNeeded = 1;
    /*  this was an attempt to allow unlimited polyphony for certain EDOs. doesn't work in Logic Pro.
    } else if (round(current.tuning().cycleLength * current.tuning().stepSize) == 1200) {
      MPEpitchBendsNeeded = current.tuning().cycleLength / std::gcd(12, current.tuning().cycleLength);
    */
    } else {
      MPEpitchBendsNeeded = 255;
    }
    if (MPEpitchBendsNeeded > 15) {
      setMPEzone(1, 15);   // MPE zone 1 = ch 2 thru 16
      while (!MPEchQueue.empty()) {     // empty the channel queue
        MPEchQueue.pop();
      }
      for (byte i = 2; i <= 16; i++) {
        MPEchQueue.push(i);           // fill the channel queue
        sendToLog("pushed ch " + std::to_string(i) + " to the open channel queue");
      }
    } else {
      setMPEzone(1, 0);
    }
    // force pitch bend back to the expected range of 2 semitones.
    for (byte i = 1; i <= 16; i++) {
      if(midiD&MIDID_USB)UMIDI.sendControlChange(123, 0, i);
      if(midiD&MIDID_SER)SMIDI.sendControlChange(123, 0, i);
      setPitchBendRange(i, MPEpitchBendSemis);   
    }
  }

  void sendMIDImodulationToCh1() {
    if(midiD&MIDID_USB)UMIDI.sendControlChange(1, modWheel.curValue, 1);
    if(midiD&MIDID_SER)SMIDI.sendControlChange(1, modWheel.curValue, 1);
    sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
  }

  void sendMIDIpitchBendToCh1() {
    if(midiD&MIDID_USB)UMIDI.sendPitchBend(pbWheel.curValue, 1);
    if(midiD&MIDID_SER)SMIDI.sendPitchBend(pbWheel.curValue, 1);
    sendToLog("sent pb wheel value " + std::to_string(pbWheel.curValue) + " to ch 1");
  }
  
  void tryMIDInoteOn(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (!(h[x].MIDIch)) {    
      if (MPEpitchBendsNeeded == 1) {
        h[x].MIDIch = 1;
      } else if (MPEpitchBendsNeeded <= 15) {
        h[x].MIDIch = 2 + positiveMod(h[x].stepsFromC, MPEpitchBendsNeeded);
      } else {
        if (MPEchQueue.empty()) {   // if there aren't any open channels
          sendToLog("MPE queue was empty so did not play a midi note");
        } else {
          h[x].MIDIch = MPEchQueue.front();   // value in MIDI terms (1-16)
          MPEchQueue.pop();
          sendToLog("popped " + std::to_string(h[x].MIDIch) + " off the MPE queue");
        }
      }
      if (h[x].MIDIch) {
        if(midiD&MIDID_USB)UMIDI.sendNoteOn(h[x].note, velWheel.curValue, h[x].MIDIch); // ch 1-16
        if(midiD&MIDID_SER)SMIDI.sendNoteOn(h[x].note, velWheel.curValue, h[x].MIDIch); // ch 1-16

        if(midiD&MIDID_USB)UMIDI.sendPitchBend(h[x].bend, h[x].MIDIch); // ch 1-16
        if(midiD&MIDID_SER)SMIDI.sendPitchBend(h[x].bend, h[x].MIDIch); // ch 1-16
        sendToLog(
          "sent MIDI noteOn: " + std::to_string(h[x].note) +
          " pb "  + std::to_string(h[x].bend) +
          " vel " + std::to_string(velWheel.curValue) +
          " ch "  + std::to_string(h[x].MIDIch)
        );
      } 
    }
  } 

  void tryMIDInoteOff(byte x) {
    // this gets called on any non-command hex
    // that is not scale-locked.
    if (h[x].MIDIch) {    // but just in case, check
      if(midiD&MIDID_USB)UMIDI.sendNoteOff(h[x].note, velWheel.curValue, h[x].MIDIch);
      if(midiD&MIDID_SER)SMIDI.sendNoteOff(h[x].note, velWheel.curValue, h[x].MIDIch);
      sendToLog(
        "sent note off: " + std::to_string(h[x].note) +
        " pb " + std::to_string(h[x].bend) +
        " vel " + std::to_string(velWheel.curValue) +
        " ch " + std::to_string(h[x].MIDIch)
      );
      if (MPEpitchBendsNeeded > 15) {
        MPEchQueue.push(h[x].MIDIch);
        sendToLog("pushed " + std::to_string(h[x].MIDIch) + " on the MPE queue");
      }
      h[x].MIDIch = 0;
    }
  }

  void setupMIDI() {
    usb_midi.setStringDescriptor("HexBoard MIDI");  // Initialize MIDI, and listen to all MIDI channels
    UMIDI.begin(MIDI_CHANNEL_OMNI);                 // This will also call usb_midi's begin()
    SMIDI.begin(MIDI_CHANNEL_OMNI);
    resetTuningMIDI();
    sendToLog("setupMIDI okay");
  }

// @synth
  /*
    This section of the code handles audio
    output via the piezo buzzer and/or the
    headphone jack (on hardware v1.2 only)
  */
  #include "hardware/pwm.h"       // library of code to access the processor's built in pulse wave modulation features
  #include "hardware/irq.h"       // library of code to let you interrupt code execution to run something of higher priority
  /*
    It is more convenient to pre-define the correct
    pulse wave modulation slice and channel associated
    with the PIEZO_PIN on this processor (see RP2040
    manual) than to have it looked up each time.
  */
  #define PIEZO_PIN 23
  #define PIEZO_SLICE 3
  #define PIEZO_CHNL 1
  #define AJACK_PIN 25
  #define AJACK_SLICE 4
  #define AJACK_CHNL 1
  // midiD takes the following bitwise flags
  #define AUDIO_NONE 0
  #define AUDIO_PIEZO 1
  #define AUDIO_AJACK 2
  #define AUDIO_BOTH 3
  byte audioD = AUDIO_PIEZO | AUDIO_AJACK;
  /*
    These definitions provide 8-bit samples to emulate.
    You can add your own as desired; it must
    be an array of 256 values, each from 0 to 255.
    Ideally the waveform is normalized so that the
    peaks are at 0 to 255, with 127 representing
    no wave movement.
  */
  byte sine[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   2,   3,   3, 
      4,   5,   6,   7,   8,   9,  10,  12,  13,  15,  16,  18,  19,  21,  23,  25, 
      27,  29,  31,  33,  35,  37,  39,  42,  44,  46,  49,  51,  54,  56,  59,  62, 
      64,  67,  70,  73,  76,  79,  81,  84,  87,  90,  93,  96,  99, 103, 106, 109, 
    112, 115, 118, 121, 124, 127, 131, 134, 137, 140, 143, 146, 149, 152, 156, 159, 
    162, 165, 168, 171, 174, 176, 179, 182, 185, 188, 191, 193, 196, 199, 201, 204, 
    206, 209, 211, 213, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 237, 
    239, 240, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 254, 
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 253, 252, 252, 
    251, 250, 249, 248, 247, 246, 245, 243, 242, 240, 239, 237, 236, 234, 232, 230, 
    228, 226, 224, 222, 220, 218, 216, 213, 211, 209, 206, 204, 201, 199, 196, 193, 
    191, 188, 185, 182, 179, 176, 174, 171, 168, 165, 162, 159, 156, 152, 149, 146, 
    143, 140, 137, 134, 131, 127, 124, 121, 118, 115, 112, 109, 106, 103,  99,  96, 
      93,  90,  87,  84,  81,  79,  76,  73,  70,  67,  64,  62,  59,  56,  54,  51, 
      49,  46,  44,  42,  39,  37,  35,  33,  31,  29,  27,  25,  23,  21,  19,  18, 
      16,  15,  13,  12,  10,   9,   8,   7,   6,   5,   4,   3,   3,   2,   1,   1
  };
  byte strings[] = {
      0,   0,   0,   1,   3,   6,  10,  14,  20,  26,  33,  41,  50,  59,  68,  77, 
      87,  97, 106, 115, 124, 132, 140, 146, 152, 157, 161, 164, 166, 167, 167, 167, 
    165, 163, 160, 157, 153, 149, 144, 140, 135, 130, 126, 122, 118, 114, 111, 109, 
    106, 104, 103, 101, 101, 100, 100, 100, 100, 101, 101, 102, 103, 103, 104, 105, 
    106, 107, 108, 109, 110, 111, 113, 114, 115, 116, 117, 119, 120, 121, 123, 124, 
    126, 127, 129, 131, 132, 134, 135, 136, 138, 139, 140, 141, 142, 144, 145, 146, 
    147, 148, 149, 150, 151, 152, 152, 153, 154, 154, 155, 155, 155, 155, 154, 154, 
    152, 151, 149, 146, 144, 140, 137, 133, 129, 125, 120, 115, 111, 106, 102,  98, 
      95,  92,  90,  88,  88,  88,  89,  91,  94,  98, 103, 109, 115, 123, 131, 140, 
    149, 158, 168, 178, 187, 196, 205, 214, 222, 229, 235, 241, 245, 249, 252, 254, 
    255, 255, 255, 254, 253, 250, 248, 245, 242, 239, 236, 233, 230, 227, 224, 222, 
    220, 218, 216, 215, 214, 213, 212, 211, 210, 210, 209, 208, 207, 206, 205, 203, 
    201, 199, 197, 194, 191, 188, 184, 180, 175, 171, 166, 161, 156, 150, 145, 139, 
    133, 127, 122, 116, 110, 105,  99,  94,  89,  84,  80,  75,  71,  67,  64,  61, 
      58,  56,  54,  52,  50,  49,  48,  47,  46,  45,  45,  44,  43,  42,  41,  40, 
      39,  37,  35,  33,  31,  28,  25,  22,  19,  16,  13,  10,   7,   5,   2,   1
  };
  byte clarinet[] = {
      0,   0,   2,   7,  14,  21,  30,  38,  47,  54,  61,  66,  70,  72,  73,  74, 
      73,  73,  72,  71,  70,  71,  72,  74,  76,  80,  84,  88,  93,  97, 101, 105, 
    109, 111, 113, 114, 114, 114, 113, 112, 111, 110, 109, 109, 109, 110, 112, 114, 
    116, 118, 121, 123, 126, 127, 128, 129, 128, 127, 126, 123, 121, 118, 116, 114, 
    112, 110, 109, 109, 109, 110, 111, 112, 113, 114, 114, 114, 113, 111, 109, 105, 
    101,  97,  93,  88,  84,  80,  76,  74,  72,  71,  70,  71,  72,  73,  73,  74, 
      73,  72,  70,  66,  61,  54,  47,  38,  30,  21,  14,   7,   2,   0,   0,   2, 
      9,  18,  31,  46,  64,  84, 105, 127, 150, 171, 191, 209, 224, 237, 246, 252, 
    255, 255, 253, 248, 241, 234, 225, 217, 208, 201, 194, 189, 185, 183, 182, 181, 
    182, 182, 183, 184, 185, 184, 183, 181, 179, 175, 171, 167, 162, 158, 154, 150, 
    146, 144, 142, 141, 141, 141, 142, 143, 144, 145, 146, 146, 146, 145, 143, 141, 
    139, 136, 134, 132, 129, 128, 127, 126, 127, 128, 129, 132, 134, 136, 139, 141, 
    143, 145, 146, 146, 146, 145, 144, 143, 142, 141, 141, 141, 142, 144, 146, 150, 
    154, 158, 162, 167, 171, 175, 179, 181, 183, 184, 185, 184, 183, 182, 182, 181, 
    182, 183, 185, 189, 194, 201, 208, 217, 225, 234, 241, 248, 253, 255, 255, 252, 
    246, 237, 224, 209, 191, 171, 150, 127, 105,  84,  64,  46,  31,  18,   9,   2, 
  };
  /*
    The hybrid synth sound blends between
    square, saw, and triangle waveforms
    at different frequencies. Said frequencies
    are controlled via constants here.
  */
    #define TRANSITION_SQUARE    220.0
    #define TRANSITION_SAW_LOW   440.0
    #define TRANSITION_SAW_HIGH  880.0
    #define TRANSITION_TRIANGLE 1760.0
  /*
    The poll interval represents how often a
    new sample value is emulated on the PWM
    hardware. It is the inverse of the digital
    audio sample rate. 24 microseconds has been
    determined to be the sweet spot, and corresponds
    to approximately 41 kHz, which is close to
    CD-quality (44.1 kHz). A shorter poll interval
    may produce more pleasant tones, but if the
    poll is too short then the code will not have
    enough time to calculate the new sample and
    the resulting audio becomes unstable and
    inaccurate. 
  */
  #define POLL_INTERVAL_IN_MICROSECONDS 24
  /*
    Eight voice polyphony can be simulated. 
    Any more voices and the
    resolution is too low to distinguish;
    also, the code becomes too slow to keep
    up with the poll interval. This value
    can be safely reduced below eight if
    there are issues.
   
    Note this is NOT the same as the MIDI
    polyphony limit, which is 15 (based
    on using channel 2 through 16 for
    polyphonic expression mode).
  */
  #define POLYPHONY_LIMIT 8
  /*
    This defines which hardware alarm
    and interrupt address are used
    to time the call of the poll() function.
  */
  #define ALARM_NUM 2
  #define ALARM_IRQ TIMER_IRQ_2
  /*
    A basic EQ level can be stored to perform
    simple loudness adjustments at certain
    frequencies where human hearing is sensitive.

    By default it's off but you can change this
    flag to "true" to enable it. This may also
    be moved to a Advanced menu option.
  */
  #define EQUAL_LOUDNESS_ADJUST true
  /*
    This class defines a virtual oscillator.
    It stores an oscillation frequency in
    the form of an increment value, which is
    how much a counter would have to be increased
    every time the poll() interval is reached, 
    such that a counter overflows from 0 to 65,535
    back to zero at some frequency per second.
   
    The value of the counter is useful for reading
    a waveform sample, so that an analog signal
    can be emulated by reading the sample at each
    poll() based on how far the counter has moved
    towards 65,536.
  */
  class oscillator {
  public:
    uint16_t increment = 0;
    uint16_t counter = 0;
    byte a = 127;
    byte b = 128;
    byte c = 255;
    uint16_t ab = 0;
    uint16_t cd = 0;
    byte eq = 0;
  };
  oscillator synth[POLYPHONY_LIMIT];          // maximum polyphony
  std::queue<byte> synthChQueue;
  const byte attenuation[] = {64,24,17,14,12,11,10,9,8}; // full volume in mono mode; equalized volume in poly.

  byte arpeggiatingNow = UNUSED_NOTE;         // if this is 255, set to off (0% duty cycle)
  uint64_t arpeggiateTime = 0;                // Used to keep track of when this note started playing in ARPEG mode
  uint64_t arpeggiateLength = 65536;         // in microseconds. approx a 1/32 note at 114 BPM

  // RUN ON CORE 2
  void poll() {
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    timer_hw->alarm[ALARM_NUM] = getTheCurrentTime() + POLL_INTERVAL_IN_MICROSECONDS;
    uint32_t mix = 0;
    byte voices = POLYPHONY_LIMIT;
    uint16_t p;
    byte t;
    byte level = 0;
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      if (synth[i].increment) {
        synth[i].counter += synth[i].increment; // should loop from 65536 -> 0        
        p = synth[i].counter;
        t = p >> 8;
        switch (currWave) {
          case WAVEFORM_SAW:                                                            break;
          case WAVEFORM_TRIANGLE: p = 2 * ((p >> 15) ? p : (65535 - p));                break;
          case WAVEFORM_SQUARE:   p = 0 - (p > (32768 - modWheel.curValue * 7 * 16));   break;
          case WAVEFORM_HYBRID:   if (t <= synth[i].a) {
                                    p = 0;
                                  } else if (t < synth[i].b) {
                                    p = (t - synth[i].a) * synth[i].ab;
                                  } else if (t <= synth[i].c) {
                                    p = 65535;
                                  } else {
                                    p = (256 - t) * synth[i].cd;
                                  };                                                  break;
          case WAVEFORM_SINE:     p = sine[t] << 8;                                   break;
          case WAVEFORM_STRINGS:  p = strings[t] << 8;                                break;
          case WAVEFORM_CLARINET: p = clarinet[t] << 8;                               break;
          default:                                                                  break;
        }
        mix += (p * synth[i].eq);  // P[16bit] * EQ[3bit] =[19bit]
      } else {
        --voices;
      }
    }
    mix *= attenuation[(playbackMode == SYNTH_POLY) * voices]; // [19bit]*atten[6bit] = [25bit]
    mix *= velWheel.curValue; // [25bit]*vel[7bit]=[32bit], poly+ 
    level = mix >> 24;  // [32bit] - [8bit] = [24bit]
    if(audioD&AUDIO_PIEZO)pwm_set_chan_level(PIEZO_SLICE, PIEZO_CHNL, level);
    if(audioD&AUDIO_AJACK)pwm_set_chan_level(AJACK_SLICE, AJACK_CHNL, level);
  }
  // RUN ON CORE 1
  byte isoTwoTwentySix(float f) {
    /*
      a very crude implementation of ISO 226
      equal loudness curves
        Hz dB  Amp ~ sqrt(10^(dB/10))
       200  0  8
       800 -3  6   
      1500  0  8
      3250 -6  4
      5000  0  8
    */
    if ((f < 8.0) || (f > 12500.0)) {   // really crude low- and high-pass
      return 0;
    } else {
      if (EQUAL_LOUDNESS_ADJUST) {
        if ((f <= 200.0) || (f >= 5000.0)) {
          return 8;
        } else {
          if (f < 1500.0) {
            return 6 + 2 * (float)(abs(f-800) / 700);
          } else {
            return 4 + 4 * (float)(abs(f-3250) / 1750);
          }
        }
      } else {
        return 8;
      }
    }
  }
  void setSynthFreq(float frequency, byte channel) {
    byte c = channel - 1;
    float f = frequency * exp2(pbWheel.curValue * PITCH_BEND_SEMIS / 98304.0);
    synth[c].counter = 0;
    synth[c].increment = round(f * POLL_INTERVAL_IN_MICROSECONDS * 0.065536);   // cycle 0-65535 at resultant frequency
    synth[c].eq = isoTwoTwentySix(f);
    if (currWave == WAVEFORM_HYBRID) {
      if (f < TRANSITION_SQUARE) {
        synth[c].b = 128;
      } else if (f < TRANSITION_SAW_LOW) {
        synth[c].b = (byte)(128 + 127 * (f - TRANSITION_SQUARE) / (TRANSITION_SAW_LOW - TRANSITION_SQUARE));
      } else if (f < TRANSITION_SAW_HIGH) {
        synth[c].b = 255;
      } else if (f < TRANSITION_TRIANGLE) {
        synth[c].b = (byte)(127 + 128 * (TRANSITION_TRIANGLE - f) / (TRANSITION_TRIANGLE - TRANSITION_SAW_HIGH));
      } else {
        synth[c].b = 127;
      }
      if (f < TRANSITION_SAW_LOW) {
        synth[c].a = 255 - synth[c].b;
        synth[c].c = 255;
      } else {
        synth[c].a = 0;
        synth[c].c = synth[c].b;
      }
      if (synth[c].a > 126) {
        synth[c].ab = 65535;
      } else {
        synth[c].ab = 65535 / (synth[c].b - synth[c].a - 1);
      }
      synth[c].cd = 65535 / (256 - synth[c].c);
    }
  }

  // USE THIS IN MONO OR ARPEG MODE ONLY

  byte findNextHeldNote() {
    byte n = UNUSED_NOTE;
    for (byte i = 1; i <= BTN_COUNT; i++) {
      byte j = positiveMod(arpeggiatingNow + i, BTN_COUNT);
      if ((h[j].MIDIch) && (!h[j].isCmd)) {
        n = j;
        break;
      }
    }
    return n;
  }
  void replaceMonoSynthWith(byte x) {
    if (arpeggiatingNow == x) return;
    h[arpeggiatingNow].synthCh = 0;
    arpeggiatingNow = x;
    if (arpeggiatingNow != UNUSED_NOTE) {
      h[arpeggiatingNow].synthCh = 1;
      setSynthFreq(h[arpeggiatingNow].frequency, 1);
    } else {
      setSynthFreq(0, 1);
    }
  }

  void resetSynthFreqs() {
    while (!synthChQueue.empty()) {
      synthChQueue.pop();
    }
    for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
      synth[i].increment = 0;
      synth[i].counter = 0;
    }
    for (byte i = 0; i < BTN_COUNT; i++) {
      h[i].synthCh = 0;
    }
    if (playbackMode == SYNTH_POLY) {
      for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
        synthChQueue.push(i + 1);
      }
    }
  }
  void sendProgramChange() {
    if(midiD&MIDID_USB)UMIDI.sendProgramChange(programChange - 1, 1);
    if(midiD&MIDID_SER)SMIDI.sendProgramChange(programChange - 1, 1);
  }
  
  void updateSynthWithNewFreqs() {
    if(midiD&MIDID_USB)UMIDI.sendPitchBend(pbWheel.curValue, 1);
    if(midiD&MIDID_SER)SMIDI.sendPitchBend(pbWheel.curValue, 1);
    for (byte i = 0; i < BTN_COUNT; i++) {
      if (!(h[i].isCmd)) {
        if (h[i].synthCh) {
          setSynthFreq(h[i].frequency,h[i].synthCh);           // pass all notes thru synth again if the pitch bend changes
        }
      }
    }
  }
  
  void trySynthNoteOn(byte x) {
    if (playbackMode != SYNTH_OFF) {
      if (playbackMode == SYNTH_POLY) {
        // operate independently of MIDI
        if (synthChQueue.empty()) {
          sendToLog("synth channels all firing, so did not add one");
        } else {
          h[x].synthCh = synthChQueue.front();
          synthChQueue.pop();
          sendToLog("popped " + std::to_string(h[x].synthCh) + " off the synth queue");
          setSynthFreq(h[x].frequency, h[x].synthCh);
        }
      } else {    
        // operate in lockstep with MIDI
        if (h[x].MIDIch) {
          replaceMonoSynthWith(x);
        }
      }
    }
  }

  void trySynthNoteOff(byte x) {
    if (playbackMode && (playbackMode != SYNTH_POLY)) {
      if (arpeggiatingNow == x) {
        replaceMonoSynthWith(findNextHeldNote());
      }
    }
    if (playbackMode == SYNTH_POLY) {
      if (h[x].synthCh) {
        setSynthFreq(0, h[x].synthCh);
        synthChQueue.push(h[x].synthCh);
        h[x].synthCh = 0;
      }
    }
  }

  void setupSynth(byte pin, byte slice) {
    gpio_set_function(pin, GPIO_FUNC_PWM);      // set that pin as PWM
    pwm_set_phase_correct(slice, true);           // phase correct sounds better
    pwm_set_wrap(slice, 254);                     // 0 - 254 allows 0 - 255 level
    pwm_set_clkdiv(slice, 1.0f);                  // run at full clock speed
    pwm_set_chan_level(slice, PIEZO_CHNL, 0);        // initialize at zero to prevent whining sound
    pwm_set_enabled(slice, true);                 // ENGAGE!
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);  // initialize the timer
    irq_set_exclusive_handler(ALARM_IRQ, poll);     // function to run every interrupt
    irq_set_enabled(ALARM_IRQ, true);               // ENGAGE!
    timer_hw->alarm[ALARM_NUM] = getTheCurrentTime() + POLL_INTERVAL_IN_MICROSECONDS;
    resetSynthFreqs();
    sendToLog("synth is ready.");
  }

  void arpeggiate() {
    if (playbackMode == SYNTH_ARPEGGIO) {
      if (runTime - arpeggiateTime > arpeggiateLength) {
        arpeggiateTime = runTime;
        replaceMonoSynthWith(findNextHeldNote());
      }
    }
  }

// @animate
  /*
    This section of the code handles
    LED animation responsive to key
    presses
  */
  /*
    The coordinate system used to locate hex buttons
    a certain distance and direction away relies on
    a preset array of coordinate offsets corresponding
    to each of the six linear directions on the hex grid.
    These cardinal directions are enumerated to make
    the code more legible for humans.
  */
  #define HEX_DIRECTION_EAST 0
  #define HEX_DIRECTION_NE   1
  #define HEX_DIRECTION_NW   2
  #define HEX_DIRECTION_WEST 3
  #define HEX_DIRECTION_SW   4
  #define HEX_DIRECTION_SE   5
  // animation variables  E NE NW  W SW SE
  int8_t vertical[] =   { 0,-1,-1, 0, 1, 1};
  int8_t horizontal[] = { 2, 1,-1,-2,-1, 1};

  uint64_t animFrame(byte x) {     
    if (h[x].timePressed) {          // 2^20 microseconds is close enough to 1 second
      return 1 + (((runTime - h[x].timePressed) * animationFPS) >> 20);
    } else {
      return 0;
    }
  }
  void flagToAnimate(int8_t r, int8_t c) {
    if (! 
      (    ( r < 0 ) || ( r >= ROWCOUNT )
        || ( c < 0 ) || ( c >= (2 * COLCOUNT) )
        || ( ( c + r ) & 1 )
      )
    ) {
      h[(10 * r) + (c / 2)].animate = 1;
    }
  }
  void animateMirror() {
    for (byte i = 0; i < LED_COUNT; i++) {                      // check every hex
      if ((!(h[i].isCmd)) && (h[i].MIDIch)) {                   // that is a held note     
        for (byte j = 0; j < LED_COUNT; j++) {                  // compare to every hex
          if ((!(h[j].isCmd)) && (!(h[j].MIDIch))) {            // that is a note not being played
            int16_t temp = h[i].stepsFromC - h[j].stepsFromC;   // look at difference between notes
            if (animationType == ANIMATE_OCTAVE) {              // set octave diff to zero if need be
              temp = positiveMod(temp, current.tuning().cycleLength);
            }
            if (temp == 0) {                                    // highlight if diff is zero
              h[j].animate = 1;
            }
          }
        }  
      }
    }
  }

  void animateOrbit() {
    for (byte i = 0; i < LED_COUNT; i++) {                               // check every hex
      if ((!(h[i].isCmd)) && (h[i].MIDIch) && ((h[i].inScale) || (!scaleLock))) {    // that is a held note
        byte tempDir = (animFrame(i) % 6);
        flagToAnimate(h[i].coordRow + vertical[tempDir], h[i].coordCol + horizontal[tempDir]);       // different neighbor each frame
      }
    }
  }

  void animateRadial() {
    for (byte i = 0; i < LED_COUNT; i++) {                                // check every hex
      if (!(h[i].isCmd) && (h[i].inScale || !scaleLock)) {                                                // that is a note
        uint64_t radius = animFrame(i);
        if ((radius > 0) && (radius < 16)) {                              // played in the last 16 frames
          byte steps = ((animationType == ANIMATE_SPLASH) ? radius : 1);  // star = 1 step to next corner; ring = 1 step per hex
          int8_t turtleRow = h[i].coordRow + (radius * vertical[HEX_DIRECTION_SW]);
          int8_t turtleCol = h[i].coordCol + (radius * horizontal[HEX_DIRECTION_SW]);
          for (byte dir = HEX_DIRECTION_EAST; dir < 6; dir++) {           // walk along the ring in each of the 6 hex directions
            for (byte i = 0; i < steps; i++) {                            // # of steps to the next corner 
              flagToAnimate(turtleRow,turtleCol);                         // flag for animation
              turtleRow += (vertical[dir] * (radius / steps));
              turtleCol += (horizontal[dir] * (radius / steps));
            }
          }
        }
      }      
    }    
  }
  void animateLEDs() {  
    for (byte i = 0; i < LED_COUNT; i++) {      
      h[i].animate = 0;
    }
    if (animationType) {
      switch (animationType) { 
        case ANIMATE_STAR: case ANIMATE_SPLASH:
          animateRadial();
          break;
        case ANIMATE_ORBIT:
          animateOrbit();
          break;
        case ANIMATE_OCTAVE: case ANIMATE_BY_NOTE:
          animateMirror();
          break;  
        default:
          break;
      }
    }
  }

// @assignment
  /*
    This section of the code contains broad
    procedures for assigning musical notes
    and related values to each button
    of the hex grid.
  */
  // run this if the layout, key, or transposition changes, but not if color or scale changes
  void assignPitches() {     
    sendToLog("assignPitch was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        // steps is the distance from C
        // the stepsToMIDI function needs distance from A4
        // it also needs to reflect any transposition, but
        // NOT the key of the scale.
        float N = stepsToMIDI(current.pitchRelToA4(h[i].stepsFromC));
        if (N < 0 || N >= 128) {
          h[i].note = UNUSED_NOTE;
          h[i].bend = 0;
          h[i].frequency = 0.0;
        } else {
          h[i].note = ((N >= 127) ? 127 : round(N));
          h[i].bend = (ldexp(N - h[i].note, 13) / MPEpitchBendSemis);
          h[i].frequency = MIDItoFreq(N);
        }
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "bend=" + std::to_string(h[i].bend) + ", " + 
          "freq=" + std::to_string(h[i].frequency) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      }
    }
    sendToLog("assignPitches complete.");
  }
  void applyScale() {
    sendToLog("applyScale was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {
        if (current.scale().tuning == ALL_TUNINGS) {
          h[i].inScale = 1;
        } else {
          byte degree = current.keyDegree(h[i].stepsFromC); 
          if (degree == 0) {
            h[i].inScale = 1;    // the root is always in the scale
          } else {
            byte tempSum = 0;
            byte iterator = 0;
            while (degree > tempSum) {
              tempSum += current.scale().pattern[iterator];
              iterator++;
            }  // add the steps in the scale, and you're in scale
            h[i].inScale = (tempSum == degree);   // if the note lands on one of those sums
          }
        }
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps=" + std::to_string(h[i].stepsFromC) + ", " +
          "isCmd? " + std::to_string(h[i].isCmd) + ", " +
          "note=" + std::to_string(h[i].note) + ", " + 
          "inScale? " + std::to_string(h[i].inScale) + "."
        );
      }
    }
    setLEDcolorCodes();
    sendToLog("applyScale complete.");
  }
  void applyLayout() {       // call this function when the layout changes
    sendToLog("buildLayout was called:");
    for (byte i = 0; i < LED_COUNT; i++) {
      if (!(h[i].isCmd)) {        
        int8_t distCol = h[i].coordCol - h[current.layout().hexMiddleC].coordCol;
        int8_t distRow = h[i].coordRow - h[current.layout().hexMiddleC].coordRow;
        h[i].stepsFromC = (
          (distCol * current.layout().acrossSteps) + 
          (distRow * (
            current.layout().acrossSteps + 
            (2 * current.layout().dnLeftSteps)
          ))
        ) / 2;  
        sendToLog(
          "hex #" + std::to_string(i) + ", " +
          "steps from C4=" + std::to_string(h[i].stepsFromC) + "."
        );
      }
    }
    applyScale();        // when layout changes, have to re-apply scale and re-apply LEDs
    assignPitches();     // same with pitches
    sendToLog("buildLayout complete.");
  }
  void cmdOn(byte x) {   // volume and mod wheel read all current buttons
    switch (h[x].note) {
      case CMDB + 3:
        toggleWheel = !toggleWheel;
        break;
      case HARDWARE_V1_2:
        Hardware_Version = h[x].note;
        setupHardware();
        break;
      default:
        // the rest should all be taken care of within the wheelDef structure
        break;
    }
  }
  void cmdOff(byte x) {   // pitch bend wheel only if buttons held.
    switch (h[x].note) {
      default:
        break;  // nothing; should all be taken care of within the wheelDef structure
    }
  }

// @menu
  /*
    This section of the code handles the
    dot matrix screen and, most importantly,
    the menu system display and controls.

    The following library is used: documentation
    is also available here.
      https://github.com/Spirik/GEM
  */
  #define GEM_DISABLE_GLCD       // this line is needed to get the B&W display to work
  /*
    The GEM menu library accepts initialization
    values to set the width of various components
    of the menu display, as below.
  */
  #define MENU_ITEM_HEIGHT 10
  #define MENU_PAGE_SCREEN_TOP_OFFSET 10
  #define MENU_VALUES_LEFT_OFFSET 78
  #define CONTRAST_AWAKE 63
  #define CONTRAST_SCREENSAVER 1
  // Create an instance of the U8g2 graphics library.
  U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE);
  // Create menu object of class GEM_u8g2. Supply its constructor with reference to u8g2 object we created earlier
  GEM_u8g2 menu(
    u8g2, GEM_POINTER_ROW, GEM_ITEMS_COUNT_AUTO, 
    MENU_ITEM_HEIGHT, MENU_PAGE_SCREEN_TOP_OFFSET, MENU_VALUES_LEFT_OFFSET
  );
  bool screenSaverOn = 0;                         
  uint64_t screenTime = 0;                        // GFX timer to count if screensaver should go on
  const uint64_t screenSaverTimeout = (1u << 23); // 2^23 microseconds ~ 8 seconds
  /*
    Create menu page object of class GEMPage. 
    Menu page holds menu items (GEMItem) and represents menu level.
    Menu can have multiple menu pages (linked to each other) with multiple menu items each.
   
    GEMPage constructor creates each page with the associated label.
    GEMItem constructor can create many different sorts of menu items.
    The items here are navigation links.
    The first parameter is the item label.
    The second parameter is the destination page when that item is selected.
  */
  GEMPage  menuPageMain("HexBoard MIDI Controller");
  GEMPage  menuPageTuning("Tuning");
  GEMItem  menuGotoTuning("Tuning", menuPageTuning);
  GEMItem  menuTuningBack("<< Back", menuPageMain);
  GEMPage  menuPageLayout("Layout");
  GEMItem  menuGotoLayout("Layout", menuPageLayout); 
  GEMItem  menuLayoutBack("<< Back", menuPageMain);
  GEMPage  menuPageScales("Scales");
  GEMItem  menuGotoScales("Scales", menuPageScales); 
  GEMItem  menuScalesBack("<< Back", menuPageMain);
  GEMPage  menuPageColors("Color options");
  GEMItem  menuGotoColors("Color options", menuPageColors);
  GEMItem  menuColorsBack("<< Back", menuPageMain);
  GEMPage  menuPageSynth("Synth options");
  GEMItem  menuGotoSynth("Synth options", menuPageSynth);
  GEMItem  menuSynthBack("<< Back", menuPageMain);
  GEMPage  menuPageControl("Control wheel");
  GEMItem  menuGotoControl("Control wheel", menuPageControl);
  GEMItem  menuControlBack("<< Back", menuPageMain);
  GEMPage  menuPageAdvanced("Advanced");
  GEMItem  menuGotoAdvanced("Advanced", menuPageAdvanced);
  GEMItem  menuAdvancedBack("<< Back", menuPageMain);
  GEMPage  menuPageReboot("Ready to flash firmware!");
  /*
    We haven't written the code for some procedures,
    but the menu item needs to know the address
    of procedures it has to run when it's selected.
    So we forward-declare a placeholder for the
    procedure like this, so that the menu item
    can be built, and then later we will define
    this procedure in full.
  */
  void changeTranspose();
  void rebootToBootloader();
  /*
    This GEMItem is meant to just be a read-only text label.
    To be honest I don't know how to get just a plain text line to show here other than this!
  */
  void fakeButton() {}
  GEMItem  menuItemVersion("v1.0.1", fakeButton);
  SelectOptionByte optionByteHardware[] =  {
    { "V1.1", HARDWARE_UNKNOWN }, { "V1.1" , HARDWARE_V1_1 },
    { "V1.2", HARDWARE_V1_2 }
  };
  GEMSelect selectHardware( sizeof(optionByteHardware)  / sizeof(SelectOptionByte), optionByteHardware);
  GEMItem  menuItemHardware("HexBoard", Hardware_Version, selectHardware, GEM_READONLY);
  /*
    This GEMItem runs a given procedure when you select it.
    We must declare or define that procedure first.
  */
  GEMItem  menuItemUSBBootloader("Update Firmware", rebootToBootloader);
  /*
    Tunings, layouts, scales, and keys are defined
    earlier in this code. We should not have to
    manually type in menu objects for those
    pre-loaded values. Instead, we will use routines to
    construct menu items automatically.
   
    These lines are forward declarations for
    the menu objects we will make later.
    This allocates space in memory with
    enough size to procedurally fill
    the objects based on the contents of
    the pre-loaded tuning/layout/etc. definitions
    we defined above.
  */
  GEMItem* menuItemTuning[TUNINGCOUNT];       
  GEMItem* menuItemLayout[layoutCount];  
  GEMItem* menuItemScales[scaleCount];       
  GEMSelect* selectKey[TUNINGCOUNT];         
  GEMItem* menuItemKeys[TUNINGCOUNT];       
  /*
    We are now creating some GEMItems that let you
    1) select a value from a list of options,
    2) update a given variable based on what was chosen,
    3) if necessary, run a procedure as well once the value's chosen.
   
    The list of options is in the form of a 2-d array.
    There are A arrays, one for each option.
    Each is 2 entries long. First entry is the label
    for that choice, second entry is the value associated.
   
    These arrays go into a typedef that depends on the type of the variable
    being selected (i.e. Byte for small positive integers; Int for
    sign-dependent and large integers).
   
    Then that typeDef goes into a GEMSelect object, with parameters
    equal to the number of entries in the array, and the storage size of one element
    in the array. The GEMSelect object is basically just a pointer to the
    array of choices. The GEMItem then takes the GEMSelect pointer as a parameter.
    
    The fact that GEM expects pointers and references makes it tricky
    to work with if you are new to C++.
  */
  SelectOptionByte optionByteMPEpitchBend[] = { { "2", 2}, {"12", 12}, {"24", 24}, {"48", 48}, {"96", 96} };
  GEMSelect selectMPEpitchBend( sizeof(optionByteMPEpitchBend) / sizeof(SelectOptionByte), optionByteMPEpitchBend);
  GEMItem menuItemMPEpitchBend( "MPE Bend Range:", MPEpitchBendSemis, selectMPEpitchBend, assignPitches);

  SelectOptionByte optionByteYesOrNo[] =  { { "No", 0 }, { "Yes" , 1 } };
  GEMSelect selectYesOrNo( sizeof(optionByteYesOrNo)  / sizeof(SelectOptionByte), optionByteYesOrNo);
  GEMItem  menuItemScaleLock( "Scale lock?", scaleLock, selectYesOrNo);
  GEMItem  menuItemPercep( "Fix color:", perceptual, selectYesOrNo, setLEDcolorCodes);
  GEMItem  menuItemShiftColor( "ColorByKey", paletteBeginsAtKeyCenter, selectYesOrNo, setLEDcolorCodes);
  GEMItem  menuItemWheelAlt( "Alt wheel?", wheelMode, selectYesOrNo);

  bool rotaryInvert = false;
  GEMItem  menuItemRotary( "Invert Encoder:", rotaryInvert);

  SelectOptionByte optionByteWheelType[] = { { "Springy", 0 }, { "Sticky", 1} };
  GEMSelect selectWheelType( sizeof(optionByteWheelType) / sizeof(SelectOptionByte), optionByteWheelType);
  GEMItem  menuItemPBBehave( "Pitch bend", pbSticky, selectWheelType);
  GEMItem  menuItemModBehave( "Mod wheel", modSticky, selectWheelType);

  SelectOptionByte optionBytePlayback[] = { { "Off", SYNTH_OFF }, { "Mono", SYNTH_MONO }, { "Arp'gio", SYNTH_ARPEGGIO }, { "Poly", SYNTH_POLY } };
  GEMSelect selectPlayback(sizeof(optionBytePlayback) / sizeof(SelectOptionByte), optionBytePlayback);
  GEMItem  menuItemPlayback(  "Synth mode:",       playbackMode,  selectPlayback, resetSynthFreqs);

  // Hardware V1.2-only
  SelectOptionByte optionByteAudioD[] =  {
    { "Buzzer", AUDIO_PIEZO }, { "Jack" , AUDIO_AJACK }, { "Both", AUDIO_BOTH }
  };
  GEMSelect selectAudioD( sizeof(optionByteAudioD)  / sizeof(SelectOptionByte), optionByteAudioD);
  GEMItem  menuItemAudioD("SynthOutput:", audioD, selectAudioD);

  // Roland MT-32 mode (1987)
  SelectOptionByte optionByteRolandMT32[] = {
    // Piano
    {"APiano1",  1}, {"APiano2",  2}, {"APiano3",  3},
    {"EPiano1",  4}, {"EPiano2",  5}, {"EPiano3",  6}, {"EPiano4",  7},
    {"HonkyTonk",8},
    // Organ
    {"EOrgan1",  9}, {"EOrgan2", 10}, {"EOrgan3", 11}, {"EOrgan4", 12},
    {"POrgan2", 13}, {"POrgan3", 14}, {"POrgan4", 15},
    {"Accordion",16},
    // Keybrd
    {"Harpsi1", 17}, {"Harpsi2", 18}, {"Harpsi3", 19},
    {"Clavi 1", 20}, {"Clavi 2", 21}, {"Clavi 3", 22},
    {"Celesta", 23}, {"Celest2", 24},
    // S Brass
    {"SBrass1", 25}, {"SBrass2", 26}, {"SBrass3", 27}, {"SBrass4", 28},
    // SynBass
    {"SynBass", 29}, {"SynBas2", 30}, {"SynBas3", 31}, {"SynBas4", 32},
    // Synth 1
    {"Fantasy", 33}, {"HarmoPan",34}, {"Chorale", 35}, {"Glasses", 36},
    {"Soundtrack",37},{"Atmosphere",38},{"WarmBell",39},{"FunnyVox",40},
    // Synth 2
    {"EchoBell",41}, {"IceRain", 42}, {"Oboe2K1", 43}, {"EchoPan", 44},
    {"Dr.Solo", 45}, {"SchoolDaze",46},{"BellSinger",47},{"SquareWave",48},
    // Strings
    {"StrSec1", 49}, {"StrSec2", 50}, {"StrSec3", 51}, {"Pizzicato", 52},
    {"Violin1", 53}, {"Violin2", 54}, {"Cello 1", 55}, {"Cello 2", 56},
    {"ContraBass",57}, {"Harp  1", 58}, {"Harp  2", 59},
    // Guitar
    {"Guitar1", 60}, {"Guitar2", 61}, {"EGuitr1", 62}, {"EGuitr2", 63},
    {"Sitar", 64},
    // Bass
    {"ABass 1", 65}, {"ABass 2", 66}, {"EBass 1", 67}, {"EBass 2", 68},
    {"SlapBass", 69},{"SlapBa2", 70}, {"Fretless", 71},{"Fretle2", 72},
    // Wind
    {"Flute 1", 73}, {"Flute 2", 74}, {"Piccolo", 75}, {"Piccol2", 76},
    {"Recorder",77}, {"PanPipes",78},
    {"Sax   1", 79}, {"Sax   2", 80}, {"Sax   3", 81}, {"Sax   4", 82},
    {"Clarinet",83}, {"Clarin2", 84}, {"Oboe",    85}, {"EnglHorn", 86},
    {"Bassoon", 87}, {"Harmonica",88},
    // Brass
    {"Trumpet", 89}, {"Trumpe2", 90}, {"Trombone",91}, {"Trombo2", 92},
    {"FrHorn1", 93}, {"FrHorn2", 94},
    {"Tuba", 95},    {"BrsSect", 96}, {"BrsSec2", 97},
    // Mallet
    {"Vibe  1", 98}, {"Vibe  2", 99},
    {"SynMallet",100}, {"WindBell",101}, {"Glock",102}, {"TubeBell",103}, {"XyloPhone",104}, {"Marimba",105},
    // Special
    {"Koto", 106}, {"Sho", 107}, {"Shakuhachi",108},
    {"Whistle",109}, {"Whistl2",110}, {"BottleBlow",111},{"BreathPipe",112},
    // Percussion
    {"Timpani",113}, {"MelTom", 114}, {"DeepSnare",115},
    {"ElPerc1",116}, {"ElPerc2",117}, {"Taiko",  118}, {"TaikoRim",119},
    {"Cymbal",120}, {"Castanets",121}, {"Triangle",122},
    // Effects
    {"OrchHit",123}, {"Telephone",124}, {"BirdTweet",125}, {"1NoteJam",126}, {"WaterBells",127}, {"JungleTune",128},
  };
  GEMSelect selectRolandMT32(sizeof(optionByteRolandMT32) / sizeof(SelectOptionByte), optionByteRolandMT32);
  GEMItem  menuItemRolandMT32("RolandMT32:", programChange,  selectRolandMT32, sendProgramChange);

  // General MIDI 1
  SelectOptionByte optionByteGeneralMidi[] = {
    // Piano
    {"Piano 1", 1}, {"Piano 2", 2}, {"Piano 3", 3}, {"HonkyTonk", 4},
    {"EPiano1", 5}, {"EPiano2", 6}, {"HarpsiChord", 7}, {"Clavinet", 8},
    // Chromatic Percussion
    {"Celesta", 9},  {"Glockenspiel", 10}, {"MusicBox", 11}, {"Vibraphone", 12},
    {"Marimba", 13}, {"Xylophone", 14}, {"TubeBells", 15}, {"Dulcimer", 16},
    // Organ
    {"Organ 1", 17}, {"Organ 2", 18}, {"Organ 3", 19}, {"ChurchOrgan", 20},
    {"ReedOrgan", 21}, {"Accordion", 22}, {"Harmonica", 23}, {"Bandoneon", 24},
    // Guitar
    {"AGtrNylon", 25}, {"AGtrSteel", 26},
    {"EGtrJazz", 27}, {"EGtrClean", 28}, {"EGtrMuted", 29},
    {"EGtrOverdrive", 30}, {"EGtrDistortion", 31}, {"EGtrHarmonics", 32},
    // Bass
    {"ABass", 33}, {"EBasFinger", 34}, {"EBasPicked", 35}, {"EBasFretless", 36},
    {"SlpBass1", 37}, {"SlpBas2", 38}, {"SynBas1", 39}, {"SynBas2", 40},
    // Strings
    {"Violin", 41}, {"Viola", 42}, {"Cello", 43}, {"ContraBass", 44},
    {"TremoloStrings", 45}, {"PizzicatoStrings", 46}, {"OrchHarp", 47}, {"Timpani", 48},
    // Ensemble
    {"StrEns1", 49}, {"StrEns2", 50}, {"SynStr1", 51}, {"SynStr2", 52},
    {"ChoirAahs", 53}, {"VoiceOohs", 54}, {"SynVoice", 55}, {"OrchHit", 56},
    // Brass
    {"Trumpet", 57}, {"Trombone", 58}, {"Tuba", 59}, {"MutedTrumpet", 60},
    {"FrenchHorn", 61}, {"BrassSection", 62}, {"SynBrs1", 63}, {"SynBrs2", 64},
    // Reed
    {"Sop Sax", 65}, {"AltoSax", 66}, {"Ten Sax", 67}, {"BariSax", 68},
    {"Oboe", 69}, {"EnglHorn", 70}, {"Bassoon", 71}, {"Clarinet", 72},
    // Pipe
    {"Piccolo", 73}, {"Flute", 74}, {"Recorder", 75}, {"PanFlute", 76},
    {"BlownBottle", 77}, {"Shakuhachi", 78}, {"Whistle", 79}, {"Ocarina", 80},
    // Synth Lead
    {"Ld1Square", 81}, {"Ld2Sawtooth", 82}, {"Ld3Calliope", 83}, {"Ld4Chiff", 84},
    {"Ld5Charang", 85}, {"Ld6Voice", 86}, {"Ld7Fifths", 87}, {"Ld8Bass&Lead", 88},
    // Synth Pad
    {"Pd1NewAge", 89}, {"Pd2Warm", 90}, {"Pd3Polysynth", 91}, {"Pd4Choir", 92},
    {"Pd5BowedGlass", 93}, {"Pd6Metallic", 94}, {"Pd7Halo", 95}, {"Pd8Sweep", 96},
     // Synth Effects
    {"FX1Rain", 97}, {"FX2Soundtrack", 98}, {"FX3Crystal", 99}, {"FX4Atmosphere", 100},
    {"FX5Bright", 101}, {"FX6Goblins", 102}, {"FX7Echoes", 103}, {"FX8SciFi)", 104},
    // Ethnic
    {"Sitar", 105}, {"Banjo", 106}, {"Shamisen", 107}, {"Koto", 108},
    {"Kalimba", 109}, {"BagPipe", 110}, {"Fiddle", 111}, {"Shanai", 112},
    // Percussive
    {"TinkleBell", 113}, {"Cowbell", 114}, {"SteelDrums", 115}, {"WoodBlock", 116},
    {"TaikoDrum", 117}, {"MeloTom", 118}, {"SynDrum", 119}, {"RevCymbal", 120},
    // Sound Effects
    {"GtrFretNoise", 121}, {"BreathNoise", 122}, {"Seashore", 123}, {"BirdTweet", 124},
    {"TelephoneRing", 125}, {"Helicopter", 126}, {"Applause", 127}, {"Gunshot", 128},
  };
  GEMSelect selectGeneralMidi(sizeof(optionByteGeneralMidi) / sizeof(SelectOptionByte), optionByteGeneralMidi);
  GEMItem  menuItemGeneralMidi("GeneralMidi:", programChange,  selectGeneralMidi, sendProgramChange);


  // doing this long-hand because the STRUCT has problems accepting string conversions of numbers for some reason
  SelectOptionInt optionIntTransposeSteps[] = {
    {"-127",-127},{"-126",-126},{"-125",-125},{"-124",-124},{"-123",-123},{"-122",-122},{"-121",-121},{"-120",-120},{"-119",-119},{"-118",-118},{"-117",-117},{"-116",-116},{"-115",-115},{"-114",-114},{"-113",-113},
    {"-112",-112},{"-111",-111},{"-110",-110},{"-109",-109},{"-108",-108},{"-107",-107},{"-106",-106},{"-105",-105},{"-104",-104},{"-103",-103},{"-102",-102},{"-101",-101},{"-100",-100},{"- 99",- 99},{"- 98",- 98},
    {"- 97",- 97},{"- 96",- 96},{"- 95",- 95},{"- 94",- 94},{"- 93",- 93},{"- 92",- 92},{"- 91",- 91},{"- 90",- 90},{"- 89",- 89},{"- 88",- 88},{"- 87",- 87},{"- 86",- 86},{"- 85",- 85},{"- 84",- 84},{"- 83",- 83},
    {"- 82",- 82},{"- 81",- 81},{"- 80",- 80},{"- 79",- 79},{"- 78",- 78},{"- 77",- 77},{"- 76",- 76},{"- 75",- 75},{"- 74",- 74},{"- 73",- 73},{"- 72",- 72},{"- 71",- 71},{"- 70",- 70},{"- 69",- 69},{"- 68",- 68},
    {"- 67",- 67},{"- 66",- 66},{"- 65",- 65},{"- 64",- 64},{"- 63",- 63},{"- 62",- 62},{"- 61",- 61},{"- 60",- 60},{"- 59",- 59},{"- 58",- 58},{"- 57",- 57},{"- 56",- 56},{"- 55",- 55},{"- 54",- 54},{"- 53",- 53},
    {"- 52",- 52},{"- 51",- 51},{"- 50",- 50},{"- 49",- 49},{"- 48",- 48},{"- 47",- 47},{"- 46",- 46},{"- 45",- 45},{"- 44",- 44},{"- 43",- 43},{"- 42",- 42},{"- 41",- 41},{"- 40",- 40},{"- 39",- 39},{"- 38",- 38},
    {"- 37",- 37},{"- 36",- 36},{"- 35",- 35},{"- 34",- 34},{"- 33",- 33},{"- 32",- 32},{"- 31",- 31},{"- 30",- 30},{"- 29",- 29},{"- 28",- 28},{"- 27",- 27},{"- 26",- 26},{"- 25",- 25},{"- 24",- 24},{"- 23",- 23},
    {"- 22",- 22},{"- 21",- 21},{"- 20",- 20},{"- 19",- 19},{"- 18",- 18},{"- 17",- 17},{"- 16",- 16},{"- 15",- 15},{"- 14",- 14},{"- 13",- 13},{"- 12",- 12},{"- 11",- 11},{"- 10",- 10},{"-  9",-  9},{"-  8",-  8},
    {"-  7",-  7},{"-  6",-  6},{"-  5",-  5},{"-  4",-  4},{"-  3",-  3},{"-  2",-  2},{"-  1",-  1},{"+/-0",   0},{"+  1",   1},{"+  2",   2},{"+  3",   3},{"+  4",   4},{"+  5",   5},{"+  6",   6},{"+  7",   7},
    {"+  8",   8},{"+  9",   9},{"+ 10",  10},{"+ 11",  11},{"+ 12",  12},{"+ 13",  13},{"+ 14",  14},{"+ 15",  15},{"+ 16",  16},{"+ 17",  17},{"+ 18",  18},{"+ 19",  19},{"+ 20",  20},{"+ 21",  21},{"+ 22",  22},
    {"+ 23",  23},{"+ 24",  24},{"+ 25",  25},{"+ 26",  26},{"+ 27",  27},{"+ 28",  28},{"+ 29",  29},{"+ 30",  30},{"+ 31",  31},{"+ 32",  32},{"+ 33",  33},{"+ 34",  34},{"+ 35",  35},{"+ 36",  36},{"+ 37",  37},
    {"+ 38",  38},{"+ 39",  39},{"+ 40",  40},{"+ 41",  41},{"+ 42",  42},{"+ 43",  43},{"+ 44",  44},{"+ 45",  45},{"+ 46",  46},{"+ 47",  47},{"+ 48",  48},{"+ 49",  49},{"+ 50",  50},{"+ 51",  51},{"+ 52",  52},
    {"+ 53",  53},{"+ 54",  54},{"+ 55",  55},{"+ 56",  56},{"+ 57",  57},{"+ 58",  58},{"+ 59",  59},{"+ 60",  60},{"+ 61",  61},{"+ 62",  62},{"+ 63",  63},{"+ 64",  64},{"+ 65",  65},{"+ 66",  66},{"+ 67",  67},
    {"+ 68",  68},{"+ 69",  69},{"+ 70",  70},{"+ 71",  71},{"+ 72",  72},{"+ 73",  73},{"+ 74",  74},{"+ 75",  75},{"+ 76",  76},{"+ 77",  77},{"+ 78",  78},{"+ 79",  79},{"+ 80",  80},{"+ 81",  81},{"+ 82",  82},
    {"+ 83",  83},{"+ 84",  84},{"+ 85",  85},{"+ 86",  86},{"+ 87",  87},{"+ 88",  88},{"+ 89",  89},{"+ 90",  90},{"+ 91",  91},{"+ 92",  92},{"+ 93",  93},{"+ 94",  94},{"+ 95",  95},{"+ 96",  96},{"+ 97",  97},
    {"+ 98",  98},{"+ 99",  99},{"+100", 100},{"+101", 101},{"+102", 102},{"+103", 103},{"+104", 104},{"+105", 105},{"+106", 106},{"+107", 107},{"+108", 108},{"+109", 109},{"+110", 110},{"+111", 111},{"+112", 112},
    {"+113", 113},{"+114", 114},{"+115", 115},{"+116", 116},{"+117", 117},{"+118", 118},{"+119", 119},{"+120", 120},{"+121", 121},{"+122", 122},{"+123", 123},{"+124", 124},{"+125", 125},{"+126", 126},{"+127", 127}
  };
  GEMSelect selectTransposeSteps( 255, optionIntTransposeSteps);
  GEMItem  menuItemTransposeSteps( "Transpose:",   transposeSteps,  selectTransposeSteps, changeTranspose);
    
  SelectOptionByte optionByteColor[] =    { { "Rainbow", RAINBOW_MODE }, { "Tiered" , TIERED_COLOR_MODE }, {"Alt", ALTERNATE_COLOR_MODE} };
  GEMSelect selectColor( sizeof(optionByteColor) / sizeof(SelectOptionByte), optionByteColor);
  GEMItem  menuItemColor( "Color mode:", colorMode, selectColor, setLEDcolorCodes);

  SelectOptionByte optionByteAnimate[] =  { { "None" , ANIMATE_NONE }, { "Octave", ANIMATE_OCTAVE },
    { "By Note", ANIMATE_BY_NOTE }, { "Star", ANIMATE_STAR }, { "Splash" , ANIMATE_SPLASH }, { "Orbit", ANIMATE_ORBIT } };
  GEMSelect selectAnimate( sizeof(optionByteAnimate)  / sizeof(SelectOptionByte), optionByteAnimate);
  GEMItem  menuItemAnimate( "Animation:", animationType, selectAnimate);

  SelectOptionByte optionByteBright[] = { { "Off", BRIGHT_OFF}, {"Dimmer", BRIGHT_DIMMER}, {"Dim", BRIGHT_DIM}, {"Low", BRIGHT_LOW}, {"Normal", BRIGHT_MID}, {"High", BRIGHT_HIGH}, {"THE SUN", BRIGHT_MAX } };
  GEMSelect selectBright( sizeof(optionByteBright) / sizeof(SelectOptionByte), optionByteBright);
  GEMItem menuItemBright( "Brightness", globalBrightness, selectBright, setLEDcolorCodes);

  SelectOptionByte optionByteWaveform[] = { { "Hybrid", WAVEFORM_HYBRID }, { "Square", WAVEFORM_SQUARE }, { "Saw", WAVEFORM_SAW },
  {"Triangl", WAVEFORM_TRIANGLE}, {"Sine", WAVEFORM_SINE}, {"Strings", WAVEFORM_STRINGS}, {"Clrinet", WAVEFORM_CLARINET} };
  GEMSelect selectWaveform(sizeof(optionByteWaveform) / sizeof(SelectOptionByte), optionByteWaveform);
  GEMItem  menuItemWaveform( "Waveform:", currWave, selectWaveform, resetSynthFreqs);

  SelectOptionInt optionIntModWheel[] = { { "too slo", 1 }, { "Turtle", 2 }, { "Slow", 4 }, 
    { "Medium",    8 }, { "Fast",     16 }, { "Cheetah",  32 }, { "Instant", 127 } };
  GEMSelect selectModSpeed(sizeof(optionIntModWheel) / sizeof(SelectOptionInt), optionIntModWheel);
  GEMItem  menuItemModSpeed( "Mod wheel:", modWheelSpeed, selectModSpeed);
  GEMItem  menuItemVelSpeed( "Vel wheel:", velWheelSpeed, selectModSpeed);

  SelectOptionInt optionIntPBWheel[] =  { { "too slo", 128 }, { "Turtle", 256 }, { "Slow", 512 },  
    { "Medium", 1024 }, { "Fast", 2048 }, { "Cheetah", 4096 },  { "Instant", 16384 } };
  GEMSelect selectPBSpeed(sizeof(optionIntPBWheel) / sizeof(SelectOptionInt), optionIntPBWheel);
  GEMItem  menuItemPBSpeed( "PB wheel:", pbWheelSpeed, selectPBSpeed);

  // Call this procedure to return to the main menu
  void menuHome() {
    menu.setMenuPageCurrent(menuPageMain);
    menu.drawMenu();
  }

  void rebootToBootloader() {
    menu.setMenuPageCurrent(menuPageReboot);
    menu.drawMenu();
    strip.clear();
    strip.show();
    rp2040.rebootToBootloader();
  }
  /*
    This procedure sets each layout menu item to be either
    visible if that layout is available in the current tuning,
    or hidden if not.
   
    It should run once after the layout menu items are
    generated, and then once any time the tuning changes.
  */
  void showOnlyValidLayoutChoices() { 
    for (byte L = 0; L < layoutCount; L++) {
      menuItemLayout[L]->hide((layoutOptions[L].tuning != current.tuningIndex));
    }
    sendToLog("menu: Layout choices were updated.");
  }
  /*
    This procedure sets each scale menu item to be either
    visible if that scale is available in the current tuning,
    or hidden if not.
   
    It should run once after the scale menu items are
    generated, and then once any time the tuning changes.
  */
  void showOnlyValidScaleChoices() {
    for (int S = 0; S < scaleCount; S++) {
      menuItemScales[S]->hide((scaleOptions[S].tuning != current.tuningIndex) && (scaleOptions[S].tuning != ALL_TUNINGS));
    }
    sendToLog("menu: Scale choices were updated.");
  }
  /*
    This procedure sets each key spinner menu item to be either
    visible if the key names correspond to the current tuning,
    or hidden if not.
   
    It should run once after the key selectors are
    generated, and then once any time the tuning changes.
  */
  void showOnlyValidKeyChoices() { 
    for (int T = 0; T < TUNINGCOUNT; T++) {
      menuItemKeys[T]->hide((T != current.tuningIndex));
    }
    sendToLog("menu: Key choices were updated.");
  }

  void updateLayoutAndRotate() {
    applyLayout();
    u8g2.setDisplayRotation(current.layout().isPortrait ? U8G2_R2 : U8G2_R1);     // and landscape / portrait rotation
  }
  /*
    This procedure is run when a layout is selected via the menu.
    It sets the current layout to the selected value.
    If it's different from the previous one, then
    re-apply the layout to the grid. In any case, go to the
    main menu when done.
  */
  void changeLayout(GEMCallbackData callbackData) {
    byte selection = callbackData.valByte;
    if (selection != current.layoutIndex) {
      current.layoutIndex = selection;
      updateLayoutAndRotate();
    }
    menuHome();
  }
  /*
    This procedure is run when a scale is selected via the menu.
    It sets the current scale to the selected value.
    If it's different from the previous one, then
    re-apply the scale to the grid. In any case, go to the
    main menu when done.
  */
  void changeScale(GEMCallbackData callbackData) {   // when you change the scale via the menu
    int selection = callbackData.valInt;
    if (selection != current.scaleIndex) {
      current.scaleIndex = selection;
      applyScale();
    }
    menuHome();
  }
  /*
    This procedure is run when the key is changed via the menu.
    A key change results in a shift in the location of the
    scale notes relative to the grid.
    In this program, the only thing that occurs is that
    the scale is reapplied to the grid.
    The menu does not go home because the intent is to stay
    on the scale/key screen.
  */
  void changeKey() {     // when you change the key via the menu
    applyScale();
  }
  /*
    This procedure was declared already and is being defined now.
    It's run when the transposition is changed via the menu.
    It sets the current transposition to the selected value.
    The effect of transposition is to change the sounded
    notes but not the layout or display.
    The procedure to re-assign pitches is therefore called.
    The menu doesn't change because the transpose is a spinner select.
  */
  void changeTranspose() {     // when you change the transpose via the menu
    current.transpose = transposeSteps;
    assignPitches();
    updateSynthWithNewFreqs();
  }
  /*
    This procedure is run when the tuning is changed via the menu.
    It affects almost everything in the program, so
    quite a few items are reset, refreshed, and redone
    when the tuning changes.
  */
  void changeTuning(GEMCallbackData callbackData) { 
    byte selection = callbackData.valByte;
    if (selection != current.tuningIndex) {
      current.tuningIndex = selection;
      current.layoutIndex = current.layoutsBegin();        // reset layout to first in list
      current.scaleIndex = 0;                              // reset scale to "no scale"
      current.keyStepsFromA = current.tuning().spanCtoA(); // reset key to C
      showOnlyValidLayoutChoices();                        // change list of choices in GEM Menu
      showOnlyValidScaleChoices();                         // change list of choices in GEM Menu
      showOnlyValidKeyChoices();                           // change list of choices in GEM Menu
      updateLayoutAndRotate();   // apply changes above
      resetTuningMIDI();  // clear out MIDI queue
      resetSynthFreqs();
    }
    menuHome();
  }
  /*
    The procedure below builds menu items for tuning,
    layout, scales, and keys based on what's preloaded.
    We already declared arrays of menu item objects earlier.
    Now we cycle through those arrays, and create GEMItem objects for
    each index. What's nice about doing this in an array is,
    we do not have to assign a variable name to each object; we just
    refer to it by its index in the array.
   
    The constructor "new GEMItem" is populated with the different
    variables in the preset objects we defined earlier.
    Then the menu item is added to the associated page.
    The item must be entered with the asterisk operator
    because an array index technically returns an address in memory
    pointing to the object; the addMenuItem procedure wants
    the contents of that item, which is what the * beforehand does. 
  */
  void createTuningMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      menuItemTuning[T] = new GEMItem(tuningOptions[T].name.c_str(), changeTuning, T);
      menuPageTuning.addMenuItem(*menuItemTuning[T]);
    }
  }
  void createLayoutMenuItems() {
    for (byte L = 0; L < layoutCount; L++) { // create pointers to all layouts
      menuItemLayout[L] = new GEMItem(layoutOptions[L].name.c_str(), changeLayout, L);
      menuPageLayout.addMenuItem(*menuItemLayout[L]);
    }
    showOnlyValidLayoutChoices();
  }
  void createKeyMenuItems() {
    for (byte T = 0; T < TUNINGCOUNT; T++) {
      selectKey[T] = new GEMSelect(tuningOptions[T].cycleLength, tuningOptions[T].keyChoices);
      menuItemKeys[T] = new GEMItem("Key:", current.keyStepsFromA, *selectKey[T], changeKey);
      menuPageScales.addMenuItem(*menuItemKeys[T]);
    }
    showOnlyValidKeyChoices();
  }
  void createScaleMenuItems() {
    for (int S = 0; S < scaleCount; S++) {  // create pointers to all scale items, filter them as you go
      menuItemScales[S] = new GEMItem(scaleOptions[S].name.c_str(), changeScale, S);
      menuPageScales.addMenuItem(*menuItemScales[S]);
    }
    showOnlyValidScaleChoices();
  }

  void setupMenu() { 
    menu.setSplashDelay(0);
    menu.init();
    /*
      addMenuItem procedure adds that GEM object to the given page.
      The menu items appear in the order they are added,
      so to change the order in the menu change the order in the code.
    */
    menuPageMain.addMenuItem(menuGotoTuning);
      createTuningMenuItems();
      menuPageTuning.addMenuItem(menuTuningBack);
    menuPageMain.addMenuItem(menuGotoLayout);
      createLayoutMenuItems();
      menuPageLayout.addMenuItem(menuLayoutBack);
    menuPageMain.addMenuItem(menuGotoScales);
      createKeyMenuItems();
      menuPageScales.addMenuItem(menuItemScaleLock);
      createScaleMenuItems();
      menuPageScales.addMenuItem(menuScalesBack);
    menuPageMain.addMenuItem(menuGotoControl);
      menuPageControl.addMenuItem(menuItemPBSpeed);
      menuPageControl.addMenuItem(menuItemModSpeed);
      menuPageControl.addMenuItem(menuItemVelSpeed);
      menuPageControl.addMenuItem(menuControlBack);
    menuPageMain.addMenuItem(menuGotoColors);
      menuPageColors.addMenuItem(menuItemColor);
      menuPageColors.addMenuItem(menuItemBright);
      menuPageColors.addMenuItem(menuItemAnimate);
      menuPageColors.addMenuItem(menuColorsBack);
    menuPageMain.addMenuItem(menuGotoSynth);
      menuPageSynth.addMenuItem(menuItemPlayback);  
      menuPageSynth.addMenuItem(menuItemWaveform);
      // menuItemAudioD added here for hardware V1.2
      menuPageSynth.addMenuItem(menuItemRolandMT32);
      menuPageSynth.addMenuItem(menuItemGeneralMidi);
      menuPageSynth.addMenuItem(menuSynthBack);
    menuPageMain.addMenuItem(menuItemTransposeSteps);
    menuPageMain.addMenuItem(menuGotoAdvanced);
      menuPageAdvanced.addMenuItem(menuItemVersion);
      menuPageAdvanced.addMenuItem(menuItemHardware);
      menuPageAdvanced.addMenuItem(menuItemMPEpitchBend);
      menuPageAdvanced.addMenuItem(menuItemRotary);
      menuPageAdvanced.addMenuItem(menuItemPercep);
      menuPageAdvanced.addMenuItem(menuItemShiftColor);
      menuPageAdvanced.addMenuItem(menuItemWheelAlt);
      menuPageAdvanced.addMenuItem(menuItemPBBehave);
      menuPageAdvanced.addMenuItem(menuItemModBehave);
      menuPageAdvanced.addMenuItem(menuItemUSBBootloader);
      menuPageAdvanced.addMenuItem(menuAdvancedBack);
    menuHome();
  }
  void setupGFX() {
    u8g2.begin();                       // Menu and graphics setup
    u8g2.setBusClock(1000000);          // Speed up display
    u8g2.setContrast(CONTRAST_AWAKE);   // Set contrast
    sendToLog("U8G2 graphics initialized.");
  }
  void screenSaver() {
    if (screenTime <= screenSaverTimeout) {
      screenTime = screenTime + lapTime;
      if (screenSaverOn) {
        screenSaverOn = 0;
        u8g2.setContrast(CONTRAST_AWAKE);
      }
    } else {
      if (!screenSaverOn) {
        screenSaverOn = 1;
        u8g2.setContrast(CONTRAST_SCREENSAVER);
      }
    }
  }

// @interface
  /*
    This section of the code handles reading
    the rotary knob and physical hex buttons.

    Documentation:
      Rotary knob code derived from:
        https://github.com/buxtronix/arduino/tree/master/libraries/Rotary
    Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
    Contact: bb@cactii.net

    when the mechanical rotary knob is turned,
    the two pins go through a set sequence of
    states during one physical "click", as follows:
      Direction          Binary state of pin A\B
      Counterclockwise = 1\1, 0\1, 0\0, 1\0, 1\1
      Clockwise        = 1\1, 1\0, 0\0, 0\1, 1\1

    The neutral state of the knob is 1\1; a turn
    is complete when 1\1 is reached again after
    passing through all the valid states above,
    at which point action should be taken depending
    on the direction of the turn.
    
    The variable rotaryState stores all of this
    data and refreshes it each loop of the 2nd processor.
      Value    Meaning
      0, 4     Knob is in neutral state
      1, 2, 3  CCW turn state 1, 2, 3
      5, 6, 7  CW  turn state 1, 2, 3
      8, 16    Completed turn CCW, CW
  */
  #define ROT_PIN_A 20
  #define ROT_PIN_B 21
  #define ROT_PIN_C 24
  rotaryKnob knob(ROT_PIN_A, ROT_PIN_B, ROT_PIN_C, false);  

  void readHexes() {
    /* This is the original way of reading buttons. multiplexer is doing the least movement. May be faster?
    for (byte r = 0; r < ROWCOUNT; r++) {      // Iterate through each of the row pins on the multiplexing chip.
      for (byte d = 0; d < 4; d++) {
        digitalWrite(mPin[d], (r >> d) & 1);
      }
      for (byte c = 0; c < COLCOUNT; c++) {    // Now iterate through each of the column pins that are connected to the current row pin.
        byte p = cPin[c];                      // Hold the currently selected column pin in a variable.
        pinMode(p, INPUT_PULLUP);              // Set that row pin to INPUT_PULLUP mode (+3.3V / HIGH).
        byte i = c + (r * COLCOUNT);
        delayMicroseconds(6);                  // delay while column pin mode
        bool didYouPressHex = (digitalRead(p) == LOW);  // hex is pressed if it returns LOW. else not pressed
        h[i].interpBtnPress(didYouPressHex);
        if (h[i].btnState == BTN_STATE_NEWPRESS) {
          h[i].timePressed = runTime;          // log the time
        }
        pinMode(p, INPUT);                     // Set the selected column pin back to INPUT mode (0V / LOW).
       }
    }*/
    // trying out a new way which may reduce rf noise (and increase reliability) by reducing the ammount of times the columns get energized
    for (byte c = 0; c < COLCOUNT; c++) {      // Iterate through each of the column pins.
      byte p = cPin[c];                        // Hold the currently selected column pin in a variable.
      pinMode(p, INPUT_PULLUP);                // Set that column pin to INPUT_PULLUP mode (+3.3V / HIGH).
      delayMicroseconds(0);                    // delay to energize column and stabilize (may need adjustment)
      for (byte r = 0; r < ROWCOUNT; r++) {    // Then iterate through each of the row pins on the multiplexing chip for the selected column.
       for (byte d = 0; d < 4; d++) {
          digitalWrite(mPin[d], (r >> d) & 1); // Selected multiplexer channel is pulled to ground.
        }
        byte i = c + (r * COLCOUNT);/*
        byte tempSat = SAT_BW;
        colorDef tempColor = {HUE_NONE, tempSat, (byte)(toggleWheel ? VALUE_SHADE : VALUE_LOW)};
        strip.setPixelColor(i, getLEDcode(tempColor));
        strip.show();*/
        delayMicroseconds(14);                  // Delay to allow signal to settle and improve reliability (found this number by experimentation)                 
        bool didYouPressHex = (digitalRead(p) == LOW);  // hex is pressed if it returns LOW. else not pressed
        h[i].interpBtnPress(didYouPressHex);
        if (h[i].btnState == BTN_STATE_NEWPRESS) {
          h[i].timePressed = runTime;          // log the time
        }
      }
      pinMode(p, INPUT);                     // Set the selected column pin back to INPUT mode (0V / LOW).
    }
    for (byte i = 0; i < BTN_COUNT; i++) {   // For all buttons in the deck
      switch (h[i].btnState) {
        case BTN_STATE_NEWPRESS: // just pressed
          if (h[i].isCmd) {
            cmdOn(i);
          } else if (h[i].inScale || (!scaleLock)) {
            tryMIDInoteOn(i);
            trySynthNoteOn(i);
          }
          break;
        case BTN_STATE_RELEASED: // just released
          if (h[i].isCmd) {
            cmdOff(i);
          } else if (h[i].inScale || (!scaleLock)) {
            tryMIDInoteOff(i);
            trySynthNoteOff(i); 
          }
          break;
        case BTN_STATE_HELD: // held
          break;
        default: // inactive
          break;
      }
    }
  }
  void updateWheels() {  
    velWheel.setTargetValue();
    bool upd = velWheel.updateValue(runTime);
    if (upd) {
      sendToLog("vel became " + std::to_string(velWheel.curValue));
    }
    if (toggleWheel) {
      pbWheel.setTargetValue();
      upd = pbWheel.updateValue(runTime);
      if (upd) {
        sendMIDIpitchBendToCh1();
        updateSynthWithNewFreqs();
      }
    } else {
      modWheel.setTargetValue();
      upd = modWheel.updateValue(runTime);
      if (upd) {
        sendMIDImodulationToCh1();
      }
    }
  }

  void dealWithRotary() {
    if (menu.readyForKey()) {
      if (knob.getClick()) {
        menu.registerKeyPress(GEM_KEY_OK);
        screenTime = 0;
      }
      int getTurn = knob.getTurnFromBuffer();
      if (getTurn != 0) {
        menu.registerKeyPress((getTurn > 0) ? GEM_KEY_UP : GEM_KEY_DOWN);
        screenTime = 0;
      }
    }
  }

  void setupHardware() {
    if (Hardware_Version == HARDWARE_V1_2) {
        midiD = MIDID_USB | MIDID_SER;
        audioD = AUDIO_PIEZO | AUDIO_AJACK;
        menuPageSynth.addMenuItem(menuItemAudioD, 2);
        globalBrightness = BRIGHT_DIM;
        setLEDcolorCodes();
        knob.invertDirection();
    }
  }

// @mainLoop
  /*
    An Arduino program runs
    the setup() function once, then
    runs the loop() function on repeat
    until the machine is powered off.

    The RP2040 has two identical cores.
    Anything called from setup() and loop()
    runs on the first core.
    Anything called from setup1() and loop1()
    runs on the second core.

    On the HexBoard, the second core is
    dedicated to two timing-critical tasks:
    running the synth emulator, and tracking
    the rotary knob inputs.
    Everything else runs on the first core.
  */
  void setup() {
    #if (defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040))
    TinyUSB_Device_Init(0);  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
    #endif
    setupMIDI();
    setupFileSystem();
    Wire.setSDA(SDAPIN);
    Wire.setSCL(SCLPIN);
    setupPins();
    setupGrid();
    applyLayout();
    setupLEDs();
    setupGFX();
    setupMenu();
    for (byte i = 0; i < 5 && !TinyUSBDevice.mounted(); i++) {
      delay(1);  // wait until device mounted, maybe
    }
  }
  void loop() {   // run on first core
    timeTracker();  // Time tracking functions
    screenSaver();  // Reduces wear-and-tear on OLED panel
    readHexes();       // Read and store the digital button states of the scanning matrix
    arpeggiate();      // arpeggiate if synth mode allows it
    updateWheels();   // deal with the pitch/mod wheel
    animateLEDs();     // deal with animations
    lightUpLEDs();      // refresh LEDs
    dealWithRotary();  // deal with menu
  }
  void setup1() {  // set up on second core
    setupSynth(PIEZO_PIN, PIEZO_SLICE);
    setupSynth(AJACK_PIN, AJACK_SLICE);
  }
  void loop1() {  // run on second core
    knob.update();
  }
