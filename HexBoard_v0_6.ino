// @readme
  /*
    HexBoard
    Copyright 2022-2023 Jared DeCook and Zach DeCook
    with help from Nicholas Fox
    Firmware v0.6.prerelease 2024-05-14
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
  #define HARDWARE_VERSION 1      // 1 = v1.1 board. 2 = v1.2 board.
  #include <Arduino.h>            // this is necessary to talk to the Hexboard!
  #include <Wire.h>               // this is necessary to deal with the pins and wires
  #define SDAPIN 16
  #define SCLPIN 17
  #include <GEM_u8g2.h>           // library of code to create menu objects on the B&W display
  #include <numeric>              // need that GCD function, son
  #include <string>               // standard C++ library string classes (use "std::string" to invoke it); these do not cause the memory corruption that Arduino::String does.
  #include <queue>                // standard C++ library construction to store open channels in microtonal mode (use "std::queue" to invoke it)
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

// @defaults
  /*
    This section sets default values
    for user-editable options
  */
  int transposeSteps = 0;
  byte scaleLock = 0;
  byte perceptual = 1;
  byte paletteBeginsAtKeyCenter = 1;
  byte animationFPS = 32;             // actually frames per 2^20 microseconds. close enough to 30fps

  byte wheelMode = 0;                 // standard vs. fine tune mode
  byte modSticky = 1;
  byte pbSticky = 0;
  byte velSticky = 1;
  int modWheelSpeed = 8;
  int pbWheelSpeed = 1024;
  int velWheelSpeed = 8;

  #define SYNTH_OFF 0
  #define SYNTH_MONO 1
  #define SYNTH_ARPEGGIO 2
  #define SYNTH_POLY 3
  byte playbackMode = SYNTH_OFF;

  #define WAVEFORM_SINE 0
  #define WAVEFORM_STRINGS 1
  #define WAVEFORM_CLARINET 2
  #define WAVEFORM_HYBRID 7
  #define WAVEFORM_SQUARE 8
  #define WAVEFORM_SAW 9
  #define WAVEFORM_TRIANGLE 10 
  byte currWave = WAVEFORM_HYBRID;

  #define RAINBOW_MODE 0
  #define TIERED_COLOR_MODE 1
  #define ALTERNATE_COLOR_MODE 2
  byte colorMode = RAINBOW_MODE;

  #define ANIMATE_NONE 0
  #define ANIMATE_STAR 1 
  #define ANIMATE_SPLASH 2 
  #define ANIMATE_ORBIT 3 
  #define ANIMATE_OCTAVE 4 
  #define ANIMATE_BY_NOTE 5
  byte animationType = ANIMATE_NONE;
  
  #define BRIGHT_MAX 255
  #define BRIGHT_HIGH 210
  #define BRIGHT_MID 180
  #define BRIGHT_LOW 150
  #define BRIGHT_DIM 110
  byte globalBrightness = BRIGHT_MID;

// @microtonal
  /*
    Most users will stick to playing in standard Western
    tuning, but for those looking to play microtonally,
    the Hexboard accommodates equal step tuning systems
    of any arbitrary size.
  */
  /*
    Each tuning system needs to be
    pre-defined, pre-counted, and enumerated as below.
    Future editions of this sketch may enable free
    definition and smart pointer references to tuning
    presets without requiring an enumeration.
  */
  #define TUNINGCOUNT 13
  #define TUNING_12EDO 0
  #define TUNING_17EDO 1
  #define TUNING_19EDO 2
  #define TUNING_22EDO 3
  #define TUNING_24EDO 4
  #define TUNING_31EDO 5
  #define TUNING_41EDO 6
  #define TUNING_53EDO 7
  #define TUNING_72EDO 8
  #define TUNING_BP 9
  #define TUNING_ALPHA 10
  #define TUNING_BETA 11
  #define TUNING_GAMMA 12     
  /*
    Note names and palette arrays are allocated in memory
    at runtime. Their usable size is based on the number
    of steps (in standard tuning, semitones) in a tuning 
    system before a new period is reached (in standard
    tuning, the octave). This value provides a maximum
    array size that handles almost all useful tunings
    without wasting much space.
  */
  #define MAX_SCALE_DIVISIONS 72
  /*
    A dictionary of musical scales is defined in the code.
    A scale is tied to one tuning system, with the exception
    of "no scale" (i.e. every note is part of the scale).
    "No scale" is tied to this value "ALL_TUNINGS" so it can
    always be chosen in the menu.
  */
  #define ALL_TUNINGS 255
  /*
    MIDI notes are enumerated 0-127 (7 bits).
    Values of 128-255 can be used to indicate
    command instructions for non-note buttons.
    These definitions support this function.
  */
  #define CMDB 192
  #define UNUSED_NOTE 255
  /*
    When sending smoothly-varying pitch bend
    or modulation messages over MIDI, the
    code uses a cool-down period of about
    1/30 of a second in between messages, enough
    for changes to sound continuous without
    overloading the MIDI message queue.
  */
  #define CC_MSG_COOLDOWN_MICROSECONDS 32768
  /*
    This class provides the seed values
    needed to map buttons to note frequencies
    and palette colors, and to populate
    the menu with correct key names and
    scale choices, for a given equal step
    tuning system.
  */
  class tuningDef {
  public:
    std::string name;         // limit is 17 characters for GEM menu
    byte cycleLength;         // steps before period/cycle/octave repeats
    float stepSize;           // in cents, 100 = "normal" semitone.
    SelectOptionInt keyChoices[MAX_SCALE_DIVISIONS];
    int spanCtoA() {
      return keyChoices[0].val_int;
    }
  };
  /*
    Note that for all practical musical purposes,
    expressing step sizes to six significant figures is
    sufficient to eliminate any detectable tuning artifacts
    due to rounding.
   
    The note names are formatted in an array specifically to
    match the format needed for the GEM Menu to accept directly
    as a spinner selection item. The number next to the note name
    is the number of steps from the anchor note A that key is.
   
    There are other ways the tuning could be calculated.
    Some microtonal players choose an anchor note
    other than A 440. Future versions will allow for
    more flexibility in anchor selection, which will also
    change the implementation of key options.
  */ 
  tuningDef tuningOptions[] = {
    { "12 EDO", 12, 100.000, 
      {{"C" ,-9},{"C#",-8},{"D" ,-7},{"Eb",-6},{"E" ,-5},{"F",-4}
      ,{"F#",-3},{"G" ,-2},{"G#",-1},{"A" , 0},{"Bb", 1},{"B", 2}
    }},
    { "17 EDO", 17, 70.5882, 
      {{"C",-13},{"Db",-12},{"C#",-11},{"D",-10},{"Eb",-9},{"D#",-8}
      ,{"E", -7},{"F" , -6},{"Gb", -5},{"F#",-4},{"G", -3},{"Ab",-2}
      ,{"G#",-1},{"A" ,  0},{"Bb",  1},{"A#", 2},{"B",  3}
    }},
    { "19 EDO", 19, 63.1579, 
      {{"C" ,-14},{"C#",-13},{"Db",-12},{"D",-11},{"D#",-10},{"Eb",-9},{"E",-8}
      ,{"E#", -7},{"F" , -6},{"F#", -5},{"Gb",-4},{"G",  -3},{"G#",-2}
      ,{"Ab", -1},{"A" ,  0},{"A#",  1},{"Bb", 2},{"B",   3},{"Cb", 4}
    }},  
    { "22 EDO", 22, 54.5455, 
      {{" C", -17},{"^C",-16},{"vC#",-15},{"vD",-14},{" D",-13},{"^D",-12}
      ,{"^Eb",-11},{"vE",-10},{" E",  -9},{" F", -8},{"^F", -7},{"vF#",-6}
      ,{"vG",  -5},{" G", -4},{"^G",  -3},{"vG#",-2},{"vA", -1},{" A",  0}
      ,{"^A",   1},{"^Bb", 2},{"vB",   3},{" B",  4}
    }},
    { "24 EDO", 24, 50.0000, 
      {{"C", -18},{"C+",-17},{"C#",-16},{"Dd",-15},{"D",-14},{"D+",-13}
      ,{"Eb",-12},{"Ed",-11},{"E", -10},{"E+", -9},{"F", -8},{"F+", -7}
      ,{"F#", -6},{"Gd", -5},{"G",  -4},{"G+", -3},{"G#",-2},{"Ad", -1}
      ,{"A",   0},{"A+",  1},{"Bb",  2},{"Bd",  3},{"B",  4},{"Cd",  5}
    }},
    { "31 EDO", 31, 38.7097, 
      {{"C",-23},{"C+",-22},{"C#",-21},{"Db",-20},{"Dd",-19}
      ,{"D",-18},{"D+",-17},{"D#",-16},{"Eb",-15},{"Ed",-14}
      ,{"E",-13},{"E+",-12}                      ,{"Fd",-11}
      ,{"F",-10},{"F+", -9},{"F#", -8},{"Gb", -7},{"Gd", -6}
      ,{"G", -5},{"G+", -4},{"G#", -3},{"Ab", -2},{"Ad", -1}
      ,{"A",  0},{"A+",  1},{"A#",  2},{"Bb",  3},{"Bd",  4}
      ,{"B",  5},{"B+",  6}                      ,{"Cd",  7}
    }},
    { "41 EDO", 41, 29.2683, 
      {{" C",-31},{"^C",-30},{" C+",-29},{" Db",-28},{" C#",-27},{" Dd",-26},{"vD",-24}
      ,{" D",-24},{"^D",-23},{" D+",-22},{" Eb",-21},{" D#",-20},{" Ed",-19},{"vE",-18}
      ,{" E",-17},{"^E",-16}                                                ,{"vF",-15}
      ,{" F",-14},{"^F",-13},{" F+",-12},{" Gb",-11},{" F#",-10},{" Gd", -9},{"vG", -8}
      ,{" G", -7},{"^G", -6},{" G+", -5},{" Ab", -4},{" G#", -3},{" Ad", -2},{"vA", -1}
      ,{" A",  0},{"^A",  1},{" A+",  2},{" Bb",  3},{" A#",  4},{" Bd",  5},{"vB",  6}
      ,{" B",  7},{"^B",  8}                                                ,{"vC",  9}
    }},
    { "53 EDO", 53, 22.6415, 
      {{" C", -40},{"^C", -39},{">C",-38},{"vDb",-37},{"Db",-36}
      ,{" C#",-35},{"^C#",-34},{"<D",-33},{"vD", -32}
      ,{" D", -31},{"^D", -30},{">D",-29},{"vEb",-28},{"Eb",-27}
      ,{" D#",-26},{"^D#",-25},{"<E",-24},{"vE", -23}
      ,{" E", -22},{"^E", -21},{">E",-20},{"vF", -19}
      ,{" F", -18},{"^F", -17},{">F",-16},{"vGb",-15},{"Gb",-14}
      ,{" F#",-13},{"^F#",-12},{"<G",-11},{"vG", -10}
      ,{" G",  -9},{"^G",  -8},{">G", -7},{"vAb", -6},{"Ab", -5}
      ,{" G#", -4},{"^G#", -3},{"<A", -2},{"vA",  -1}
      ,{" A",   0},{"^A",   1},{">A",  2},{"vBb",  3},{"Bb",  4}
      ,{" A#",  5},{"^A#",  6},{"<B",  7},{"vB",   8}
      ,{" B",   9},{"^B",  10},{"<C", 11},{"vC",  12}
    }},
    { "72 EDO", 72, 16.6667, 
      {{" C", -54},{"^C", -53},{">C", -52},{" C+",-51},{"<C#",-50},{"vC#",-49}
      ,{" C#",-48},{"^C#",-47},{">C#",-46},{" Dd",-45},{"<D" ,-44},{"vD" ,-43}
      ,{" D", -42},{"^D", -41},{">D", -40},{" D+",-39},{"<Eb",-38},{"vEb",-37}
      ,{" Eb",-36},{"^Eb",-35},{">Eb",-34},{" Ed",-33},{"<E" ,-32},{"vE" ,-31}
      ,{" E", -30},{"^E", -29},{">E", -28},{" E+",-27},{"<F" ,-26},{"vF" ,-25}
      ,{" F", -24},{"^F", -23},{">F", -22},{" F+",-21},{"<F#",-20},{"vF#",-19}
      ,{" F#",-18},{"^F#",-17},{">F#",-16},{" Gd",-15},{"<G" ,-14},{"vG" ,-13}
      ,{" G", -12},{"^G", -11},{">G", -10},{" G+", -9},{"<G#", -8},{"vG#", -7}
      ,{" G#", -6},{"^G#", -5},{">G#", -4},{" Ad", -3},{"<A" , -2},{"vA" , -1}
      ,{" A",   0},{"^A",   1},{">A",   2},{" A+",  3},{"<Bb",  4},{"vBb",  5}
      ,{" Bb",  6},{"^Bb",  7},{">Bb",  8},{" Bd",  9},{"<B" , 10},{"vB" , 11}
      ,{" B",  12},{"^B",  13},{">B",  14},{" Cd", 15},{"<C" , 16},{"vC" , 17}
    }},
    { "Bohlen-Pierce", 13, 146.304, 
      {{"C",-10},{"Db",-9},{"D",-8},{"E",-7},{"F",-6},{"Gb",-5}
      ,{"G",-4},{"H",-3},{"Jb",-2},{"J",-1},{"A",0},{"Bb",1},{"B",2}
    }},
    { "Carlos Alpha", 9, 77.9650, 
      {{"I",0},{"I#",1},{"II-",2},{"II+",3},{"III",4}
      ,{"III#",5},{"IV-",6},{"IV+",7},{"Ib",8}
    }},
    { "Carlos Beta", 11, 63.8329,
      {{"I",0},{"I#",1},{"IIb",2},{"II",3},{"II#",4},{"III",5}
      ,{"III#",6},{"IVb",7},{"IV",8},{"IV#",9},{"Ib",10}
    }},
    { "Carlos Gamma", 20, 35.0985,
      {{" I",  0},{"^I",  1},{" IIb", 2},{"^IIb", 3},{" I#",   4},{"^I#",   5}
      ,{" II", 6},{"^II", 7}
      ,{" III",8},{"^III",9},{" IVb",10},{"^IVb",11},{" III#",12},{"^III#",13}
      ,{" IV",14},{"^IV",15},{" Ib", 16},{"^Ib", 17},{" IV#", 18},{"^IV#", 19}
    }},
  };

// @layout
  /*
    This section defines the different
    preset note layout options.
  */  
  /*
    This class provides the seed values
    needed to implement a given isomorphic
    note layout. From it, the map of buttons
    to note frequencies can be calculated.
   
    A layout is tied to a specific tuning.
  */
  class layoutDef {
  public:
    std::string name;    // limit is 17 characters for GEM menu
    bool isPortrait;     // affects orientation of the GEM menu only.
    byte hexMiddleC;     // instead of "what note is button 1", "what button is the middle"
    int8_t acrossSteps;  // defined this way to be compatible with original v1.1 firmare
    int8_t dnLeftSteps;  // defined this way to be compatible with original v1.1 firmare
    byte tuning;         // index of the tuning that this layout is designed for
  };
  /*
    Isomorphic layouts are defined by
    establishing where the center of the
    layout is, and then the number of tuning
    steps to go up or down for the hex button
    across or down diagonally.
  */
  layoutDef layoutOptions[] = {
    { "Wicki-Hayden",      1, 64,   2,  -7, TUNING_12EDO },
    { "Harmonic Table",    0, 75,  -7,   3, TUNING_12EDO },
    { "Janko",             0, 65,  -1,  -1, TUNING_12EDO },
    { "Gerhard",           0, 65,  -1,  -3, TUNING_12EDO },
    { "Accordion C-sys.",  1, 75,   2,  -3, TUNING_12EDO },
    { "Accordion B-sys.",  1, 64,   1,  -3, TUNING_12EDO },

    { "Full Gamut",        1, 65,   1,  -9, TUNING_17EDO },
    { "Bosanquet-Wilson",  0, 65,  -2,  -1, TUNING_17EDO },
    { "Neutral Thirds A",  0, 65,  -1,  -2, TUNING_17EDO },
    { "Neutral Thirds B",  0, 65,   1,  -3, TUNING_17EDO },

    { "Full Gamut",        1, 65,   1,  -9, TUNING_19EDO },
    { "Bosanquet-Wilson",  0, 65,  -1,  -2, TUNING_19EDO },
    { "Kleismic",          0, 65,  -1,  -4, TUNING_19EDO },
    
    { "Full Gamut",        1, 65,   1,  -8, TUNING_22EDO },
    { "Bosanquet-Wilson",  0, 65,  -3,  -1, TUNING_22EDO },
    { "Porcupine",         0, 65,   1,  -4, TUNING_22EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_24EDO },
    { "Bosanquet-Wilson",  0, 65,  -1,  -3, TUNING_24EDO },
    { "Inverted",          0, 65,   1,  -4, TUNING_24EDO },
    
    { "Full Gamut",        1, 65,   1,  -7, TUNING_31EDO },
    { "Bosanquet-Wilson",  0, 65,  -2,  -3, TUNING_31EDO },
    { "Double Bosanquet",  0, 65,  -1,  -4, TUNING_31EDO },
    { "Anti-Double Bos.",  0, 65,   1,  -5, TUNING_31EDO },
    
    { "Full Gamut",        0, 65,   1,  -8, TUNING_41EDO },  // forty-one #3
    { "Bosanquet-Wilson",  0, 65,  -4,  -3, TUNING_41EDO },  // forty-one #1
    { "Gerhard",           0, 65,   3, -10, TUNING_41EDO },  // forty-one #2
    { "Baldy",             0, 65,  -1,  -6, TUNING_41EDO },  
    { "Rodan",             1, 65,  -1,  -7, TUNING_41EDO },  
    
    { "Wicki-Hayden",      1, 64,   9, -31, TUNING_53EDO },
    { "Bosanquet-Wilson",  0, 65,  -5,  -4, TUNING_53EDO },
    { "Kleismic A",        0, 65,  -8,  -3, TUNING_53EDO },
    { "Kleismic B",        0, 65,  -5,  -3, TUNING_53EDO },
    { "Harmonic Table",    0, 75, -31,  14, TUNING_53EDO },
    { "Buzzard",           0, 65,  -9,  -1, TUNING_53EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_72EDO },
    { "Expanded Janko",    0, 65,  -1,  -6, TUNING_72EDO },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_BP },
    { "Standard",          0, 65,  -2,  -1, TUNING_BP },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_ALPHA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_ALPHA },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_BETA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_BETA },
    
    { "Full Gamut",        1, 65,   1,  -9, TUNING_GAMMA },
    { "Compressed",        0, 65,  -2,  -1, TUNING_GAMMA }    
  };
  const byte layoutCount = sizeof(layoutOptions) / sizeof(layoutDef);
// @scales
  /*
    This class defines a scale pattern
    for a given tuning. It is basically
    an array with the number of steps in
    between each degree of the scale. For
    example, the major scale in 12EDO
    is 2, 2, 1, 2, 2, 2, 1.
   
    A scale is tied to a specific tuning.
  */
  class scaleDef {
  public:
    std::string name;
    byte tuning;
    byte pattern[MAX_SCALE_DIVISIONS];
  };
  scaleDef scaleOptions[] = {
    { "None",              ALL_TUNINGS,      { 0 } },
    // 12 EDO
    { "Major",             TUNING_12EDO,     { 2,2,1,2,2,2,1 } },
    { "Minor, natural",    TUNING_12EDO,     { 2,1,2,2,1,2,2 } },
    { "Minor, melodic",    TUNING_12EDO,     { 2,1,2,2,2,2,1 } },
    { "Minor, harmonic",   TUNING_12EDO,     { 2,1,2,2,1,3,1 } },
    { "Pentatonic, major", TUNING_12EDO,     { 2,2,3,2,3 } },
    { "Pentatonic, minor", TUNING_12EDO,     { 3,2,2,3,2 } },
    { "Blues",             TUNING_12EDO,     { 3,1,1,1,1,3,2 } },
    { "Double Harmonic",   TUNING_12EDO,     { 1,3,1,2,1,3,1 } },
    { "Phrygian",          TUNING_12EDO,     { 1,2,2,2,1,2,2 } },
    { "Phrygian Dominant", TUNING_12EDO,     { 1,3,1,2,1,2,2 } },
    { "Dorian",            TUNING_12EDO,     { 2,1,2,2,2,1,2 } },
    { "Lydian",            TUNING_12EDO,     { 2,2,2,1,2,2,1 } },
    { "Lydian Dominant",   TUNING_12EDO,     { 2,2,2,1,2,1,2 } },
    { "Mixolydian",        TUNING_12EDO,     { 2,2,1,2,2,1,2 } },
    { "Locrian",           TUNING_12EDO,     { 1,2,2,1,2,2,2 } },
    { "Whole tone",        TUNING_12EDO,     { 2,2,2,2,2,2 } },
    { "Octatonic",         TUNING_12EDO,     { 2,1,2,1,2,1,2,1 } },
    // 17 EDO; for more: https://en.xen.wiki/w/17edo#Scales
    { "Diatonic",          TUNING_17EDO,  { 3,3,1,3,3,3,1 } },
    { "Pentatonic",        TUNING_17EDO,  { 3,3,4,3,4 } },
    { "Harmonic",          TUNING_17EDO,  { 3,2,3,2,2,2,3 } },
    { "Husayni maqam",     TUNING_17EDO,  { 2,2,3,3,2,1,1,3 } },
    { "Blues",             TUNING_17EDO,  { 4,3,1,1,1,4,3 } },
    { "Hydra",             TUNING_17EDO,  { 3,3,1,1,2,3,2,1,1 } },
    // 19 EDO; for more: https://en.xen.wiki/w/19edo#Scales
    { "Diatonic",          TUNING_19EDO,   { 3,3,2,3,3,3,2 } },
    { "Pentatonic",        TUNING_19EDO,   { 3,3,5,3,5 } },
    { "Semaphore",         TUNING_19EDO,   { 3,1,3,1,3,3,1,3,1 } },
    { "Negri",             TUNING_19EDO,   { 2,2,2,2,2,1,2,2,2,2 } },
    { "Sensi",             TUNING_19EDO,   { 2,2,1,2,2,2,1,2,2,2,1 } },
    { "Kleismic",          TUNING_19EDO,   { 1,3,1,1,3,1,1,3,1,3,1 } },
    { "Magic",             TUNING_19EDO,   { 3,1,1,1,3,1,1,1,3,1,1,1,1 } },
    { "Kind of blues",     TUNING_19EDO,   { 4,4,1,2,4,4 } },
    // 22 EDO; for more: https://en.xen.wiki/w/22edo_modes
    { "Diatonic",          TUNING_22EDO,  { 4,4,1,4,4,4,1 } },
    { "Pentatonic",        TUNING_22EDO,  { 4,4,5,4,5 } },
    { "Orwell",            TUNING_22EDO,  { 3,2,3,2,3,2,3,2,2 } },
    { "Porcupine",         TUNING_22EDO,  { 4,3,3,3,3,3,3 } },
    { "Pajara",            TUNING_22EDO,  { 2,2,3,2,2,2,3,2,2,2 } },
    // 24 EDO; for more: https://en.xen.wiki/w/24edo_scales
    { "Diatonic 12",       TUNING_24EDO, { 4,4,2,4,4,4,2 } },
    { "Diatonic Soft",     TUNING_24EDO, { 3,5,2,3,5,4,2 } },
    { "Diatonic Neutral",  TUNING_24EDO, { 4,3,3,4,3,4,3 } },
    { "Pentatonic (12)",   TUNING_24EDO, { 4,4,6,4,6 } },
    { "Pentatonic (Haba)", TUNING_24EDO, { 5,5,5,5,4 } },
    { "Invert Pentatonic", TUNING_24EDO, { 6,3,6,6,3 } },
    { "Rast maqam",        TUNING_24EDO, { 4,3,3,4,4,2,1,3 } },
    { "Bayati maqam",      TUNING_24EDO, { 3,3,4,4,2,1,3,4 } },      
    { "Hijaz maqam",       TUNING_24EDO, { 2,6,2,4,2,1,3,4 } },
    { "8-EDO",             TUNING_24EDO, { 3,3,3,3,3,3,3,3 } },
    { "Wyschnegradsky",    TUNING_24EDO, { 2,2,2,2,2,1,2,2,2,2,2,2,1 } },
    // 31 EDO; for more: https://en.xen.wiki/w/31edo#Scales
    { "Diatonic",          TUNING_31EDO,  { 5,5,3,5,5,5,3 } },
    { "Pentatonic",        TUNING_31EDO,  { 5,5,8,5,8 } },
    { "Harmonic",          TUNING_31EDO,  { 5,5,4,4,4,3,3,3 } },
    { "Mavila",            TUNING_31EDO,  { 5,3,3,3,5,3,3,3,3 } },
    { "Quartal",           TUNING_31EDO,  { 2,2,7,2,2,7,2,7 } },
    { "Orwell",            TUNING_31EDO,  { 4,3,4,3,4,3,4,3,3 } },
    { "Neutral",           TUNING_31EDO,  { 4,4,4,4,4,4,4,3 } },
    { "Miracle",           TUNING_31EDO,  { 4,3,3,3,3,3,3,3,3,3 } },
    // 41 EDO; for more: https://en.xen.wiki/w/41edo#Scales_and_modes
    { "Diatonic",          TUNING_41EDO,   { 7,7,3,7,7,7,3 } },
    { "Pentatonic",        TUNING_41EDO,   { 7,7,10,7,10 } },
    { "Pure major",        TUNING_41EDO,   { 7,6,4,7,6,7,4 } },
    { "5-limit chromatic", TUNING_41EDO,   { 4,3,4,2,4,3,4,4,2,4,3,4 } },
    { "7-limit chromatic", TUNING_41EDO,   { 3,4,2,4,4,3,4,2,4,3,3,4 } },
    { "Harmonic",          TUNING_41EDO,   { 5,4,4,4,4,3,3,3,3,3,2,3 } },
    { "Middle East-ish",   TUNING_41EDO,   { 7,5,7,5,5,7,5 } },
    { "Thai",              TUNING_41EDO,   { 6,6,6,6,6,6,5 } },
    { "Slendro",           TUNING_41EDO,   { 8,8,8,8,9 } },
    { "Pelog / Mavila",    TUNING_41EDO,   { 8,5,5,8,5,5,5 } },
    // 53 EDO
    { "Diatonic",          TUNING_53EDO, { 9,9,4,9,9,9,4 } },
    { "Pentatonic",        TUNING_53EDO, { 9,9,13,9,13 } },
    { "Rast makam",        TUNING_53EDO, { 9,8,5,9,9,4,4,5 } },
    { "Usshak makam",      TUNING_53EDO, { 7,6,9,9,4,4,5,9 } },
    { "Hicaz makam",       TUNING_53EDO, { 5,12,5,9,4,9,9 } },
    { "Orwell",            TUNING_53EDO, { 7,5,7,5,7,5,7,5,5 } },
    { "Sephiroth",         TUNING_53EDO, { 6,5,5,6,5,5,6,5,5,5 } },
    { "Smitonic",          TUNING_53EDO, { 11,11,3,11,3,11,3 } },
    { "Slendric",          TUNING_53EDO, { 7,3,7,3,7,3,7,3,7,3,3 } },
    { "Semiquartal",       TUNING_53EDO, { 9,2,9,2,9,2,9,2,9 } },
    // 72 EDO
    { "Diatonic",          TUNING_72EDO, { 12,12,6,12,12,12,6 } },
    { "Pentatonic",        TUNING_72EDO, { 12,12,18,12,18 } },
    { "Ben Johnston",      TUNING_72EDO, { 6,6,6,5,5,5,9,8,4,4,7,7 } },
    { "18-EDO",            TUNING_72EDO, { 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4 } },
    { "Miracle",           TUNING_72EDO, { 5,2,5,2,5,2,2,5,2,5,2,5,2,5,2,5,2,5,2,5,2 } },
    { "Marvolo",           TUNING_72EDO, { 5,5,5,5,5,5,5,2,5,5,5,5,5,5 } },
    { "Catakleismic",      TUNING_72EDO, { 4,7,4,4,4,7,4,4,4,7,4,4,4,7,4 } },
    { "Palace",            TUNING_72EDO, { 10,9,11,12,10,9,11 } },
    // BP
    { "Lambda",            TUNING_BP, { 2,1,2,1,2,1,2,1,1 } },
    // Alpha
    { "Super Meta Lydian", TUNING_ALPHA, { 3,2,2,2 } },
    // Beta
    { "Super Meta Lydian", TUNING_BETA,  { 3,3,3,2 } },
    // Gamma
    { "Super Meta Lydian", TUNING_GAMMA, { 6,5,5,4 } }
  };
  const byte scaleCount = sizeof(scaleOptions) / sizeof(scaleDef);

// @palettes
  /*
    This section defines the code needed
    to determine colors for each hex.
  */  
  /*
    LED colors are defined in the code
    on a perceptual basis. Instead of 
    calculating RGB codes, the program
    uses an artist's color wheel approach.
   
    For value / brightness, two sets of
    named constants are defined. The BRIGHT_
    series (see the defaults section above)
    corresponds to the overall
    level of lights from the HexBoard, from
    dim to maximum. The VALUE_ series
    is used to differentiate light and dark
    colors in a palette. The BRIGHT and VALUE
    are multiplied together (and normalized)
    to get the output brightness.
  */
  #define VALUE_BLACK 0
  #define VALUE_LOW   127
  #define VALUE_SHADE 164
  #define VALUE_NORMAL 180
  #define VALUE_FULL  255
  /*
    Saturation is zero for black and white, and 255
    for fully chromatic color. Value is the
    brightness level of the LED, from 0 = off
    to 255 = max.
  */
  #define SAT_BW 0
  #define SAT_TINT 32
  #define SAT_DULL 85
  #define SAT_MODERATE 120
  #define SAT_VIVID 255
  /*
    Hues are angles from 0 to 360, starting
    at red and towards yellow->green->blue
    when the hue angle increases. 
  */
  #define HUE_NONE 0.0
  #define HUE_RED 0.0
  #define HUE_ORANGE 36.0
  #define HUE_YELLOW 72.0
  #define HUE_LIME 108.0
  #define HUE_GREEN 144.0
  #define HUE_CYAN 180.0
  #define HUE_BLUE 216.0
  #define HUE_INDIGO 252.0
  #define HUE_PURPLE 288.0
  #define HUE_MAGENTA 324.0
  /*
    This class is a basic hue, saturation,
    and value triplet, with some limited
    transformation functions. Rather than
    load a full color space library, this
    program uses non-class procedures to
    perform conversions to and from LED-
    friendly color codes.
  */
  class colorDef {
  public:
    float hue;
    byte sat;
    byte val;
    colorDef tint() {
      colorDef temp;
      temp.hue = this->hue;
      temp.sat = ((this->sat > SAT_MODERATE) ? SAT_MODERATE : this->sat);
      temp.val = VALUE_FULL;
      return temp;
    }
    colorDef shade() {
      colorDef temp;
      temp.hue = this->hue;
      temp.sat = ((this->sat > SAT_DULL) ? SAT_DULL : this->sat);
      temp.val = VALUE_LOW;
      return temp;
    }
  };
  /*
    This class defines a palette, which is
    a map of musical scale degrees to
    colors. A palette is tied to a specific
    tuning but not to a specific layout.
  */
  class paletteDef {
  public:
    colorDef swatch[MAX_SCALE_DIVISIONS]; // the different colors used in this palette
    byte colorNum[MAX_SCALE_DIVISIONS];   // map key (c,d...) to swatches
    colorDef getColor(byte givenStepFromC) {
      return swatch[colorNum[givenStepFromC] - 1];
    }
    float getHue(byte givenStepFromC) {
      return getColor(givenStepFromC).hue;
    }
    byte getSat(byte givenStepFromC) {
      return getColor(givenStepFromC).sat;
    }
    byte getVal(byte givenStepFromC) {
      return getColor(givenStepFromC).val;
    }
  };
  /*
    Palettes are defined by creating
    a set of colors, and then making
    an array of numbers that map the
    intervals of that tuning to the
    chosen colors. It's like paint
    by numbers! Note that the indexes
    start with 1, because the arrays are
    padded with 0 for entries after
    those intialized.
  */
  paletteDef palette[] = {
    // 12 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL}
      , {HUE_BLUE,    SAT_DULL,  VALUE_SHADE }
      , {HUE_CYAN,    SAT_DULL,  VALUE_NORMAL}
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL}
      },{1,2,1,2,1,3,4,3,4,3,4,3}},
    // 17 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL}
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL}
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL}
      },{1,2,3,1,2,3,1,1,2,3,1,2,3,1,2,3,1}},
    // 19 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL} //  #
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL} //  b
      , {HUE_MAGENTA, SAT_VIVID, VALUE_NORMAL} // enh
      },{1,2,3,1,2,3,1,4,1,2,3,1,2,3,1,2,3,1,4}},
    // 22 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL} // ^
      , {HUE_MAGENTA, SAT_VIVID, VALUE_NORMAL} // mid
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL} // v
      },{1,2,3,4,1,2,3,4,1,1,2,3,4,1,2,3,4,1,2,3,4,1}},
    // 24 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_LIME,    SAT_DULL,  VALUE_SHADE } //  +
      , {HUE_CYAN,    SAT_VIVID, VALUE_NORMAL} //  #/b  
      , {HUE_INDIGO,  SAT_DULL,  VALUE_SHADE } //  d
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } // enh
      },{1,2,3,4,1,2,3,4,1,5,1,2,3,4,1,2,3,4,1,2,3,4,1,5}},
    // 31 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_RED,     SAT_DULL,  VALUE_NORMAL} //  +
      , {HUE_YELLOW,  SAT_DULL,  VALUE_SHADE } //  #
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } //  b
      , {HUE_INDIGO,  SAT_DULL,  VALUE_NORMAL} //  d
      , {HUE_RED,     SAT_DULL,  VALUE_SHADE } //  enh E+ Fb
      , {HUE_INDIGO,  SAT_DULL,  VALUE_SHADE } //  enh E# Fd
      },{1,2,3,4,5,1,2,3,4,5,1,6,7,1,2,3,4,5,1,2,3,4,5,1,2,3,4,5,1,6,7}},
    // 41 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_RED,     SAT_DULL,  VALUE_NORMAL} //  ^
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL} //  +
      , {HUE_CYAN,    SAT_DULL,  VALUE_SHADE } //  b
      , {HUE_GREEN,   SAT_DULL,  VALUE_SHADE } //  #
      , {HUE_MAGENTA, SAT_DULL,  VALUE_NORMAL} //  d
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL} //  v
      },{1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,2,3,1,2,3,4,5,6,7,
         1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,6,7}},
    // 53 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_ORANGE,  SAT_VIVID, VALUE_NORMAL} //  ^
      , {HUE_MAGENTA, SAT_DULL,  VALUE_NORMAL} //  L
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL} // bv
      , {HUE_GREEN,   SAT_VIVID, VALUE_SHADE } // b
      , {HUE_YELLOW,  SAT_VIVID, VALUE_SHADE } // #
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL} // #^
      , {HUE_PURPLE,  SAT_DULL,  VALUE_NORMAL} //  7
      , {HUE_CYAN,    SAT_VIVID, VALUE_SHADE } //  v
      },{1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9,1,2,3,9,1,2,3,4,5,6,7,8,9,
         1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9,1,2,3,9}},
    // 72 EDO
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_GREEN,   SAT_DULL,  VALUE_SHADE } // ^
      , {HUE_RED,     SAT_DULL,  VALUE_SHADE } // L
      , {HUE_PURPLE,  SAT_DULL,  VALUE_SHADE } // +/d
      , {HUE_BLUE,    SAT_DULL,  VALUE_SHADE } // 7
      , {HUE_YELLOW,  SAT_DULL,  VALUE_SHADE } // v
      , {HUE_INDIGO,  SAT_VIVID, VALUE_SHADE } // #/b
      },{1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,1,2,3,4,5,6,
         7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6,7,2,3,4,5,6,1,2,3,4,5,6}},
    // BOHLEN PIERCE
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL}
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL}
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL}
      },{1,2,3,1,2,3,1,1,2,3,1,2,3}},
    // ALPHA
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL} // #
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL} // d
      , {HUE_LIME,    SAT_VIVID, VALUE_NORMAL} // +
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL} // enharmonic
      , {HUE_CYAN,    SAT_VIVID, VALUE_NORMAL} // b
      },{1,2,3,4,1,2,3,5,6}},
    // BETA
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_INDIGO,  SAT_VIVID, VALUE_NORMAL} // #
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL} // b
      , {HUE_MAGENTA, SAT_DULL,  VALUE_NORMAL} // enharmonic
      },{1,2,3,1,4,1,2,3,1,2,3}},
    // GAMMA
      {{{HUE_NONE,    SAT_BW,    VALUE_NORMAL} // n
      , {HUE_RED,     SAT_VIVID, VALUE_NORMAL} // b
      , {HUE_BLUE,    SAT_VIVID, VALUE_NORMAL} // #
      , {HUE_YELLOW,  SAT_VIVID, VALUE_NORMAL} // n^
      , {HUE_PURPLE,  SAT_VIVID, VALUE_NORMAL} // b^
      , {HUE_GREEN,   SAT_VIVID, VALUE_NORMAL} // #^
      }, {1,4,2,5,3,6,1,4,1,4,2,5,3,6,1,4,2,5,3,6}},
  };

// @presets
  /*
    This section of the code defines
    a "preset" as a collection of
    parameters that control how the
    hexboard is operating and playing.

    In the long run this will serve as
    a foundation for saving and loading
    preferences / settings through the
    file system.
  */
  class presetDef { 
    public:
      std::string presetName; 
      int tuningIndex;     // instead of using pointers, i chose to store index value of each option, to be saved to a .pref or .ini or something
      int layoutIndex;
      int scaleIndex;
      int keyStepsFromA; // what key the scale is in, where zero equals A.
      int transpose;
      // define simple recall functions
      tuningDef tuning() {
        return tuningOptions[tuningIndex];
      }
      layoutDef layout() {
        return layoutOptions[layoutIndex];
      }
      scaleDef scale() {
        return scaleOptions[scaleIndex];
      }
      int layoutsBegin() {
        if (tuningIndex == TUNING_12EDO) {
          return 0;
        } else {
          int temp = 0;
          while (layoutOptions[temp].tuning < tuningIndex) {
            temp++;
          }
          return temp;
        }
      }
      int keyStepsFromC() {
        return tuning().spanCtoA() - keyStepsFromA;
      }
      int pitchRelToA4(int givenStepsFromC) {
        return givenStepsFromC + tuning().spanCtoA() + transpose;
      }
      int keyDegree(int givenStepsFromC) {
        return positiveMod(givenStepsFromC + keyStepsFromC(), tuning().cycleLength);
      }
    };

    presetDef current = {
      "Default",      // name
      TUNING_12EDO,   // tuning
      0,              // default to the first layout, wicki hayden
      0,              // default to using no scale (chromatic)
      -9,             // default to the key of C, which in 12EDO is -9 steps from A.
      0               // default to no transposition
    };

// @diagnostics
  /*
    This section of the code handles
    optional sending of log messages
    to the Serial port
  */
  #define DIAGNOSTICS_ON true 
  void sendToLog(std::string msg) {
    if (DIAGNOSTICS_ON) {
      Serial.println(msg.c_str());
    }
  }

// @timing
  /*
    This section of the code handles basic
    timekeeping stuff
  */
  #include "hardware/timer.h"     // library of code to access the processor's clock functions
  uint64_t runTime = 0;                // Program loop consistent variable for time in microseconds since power on
  uint64_t lapTime = 0;                // Used to keep track of how long each loop takes. Useful for rate-limiting.
  uint64_t loopTime = 0;               // Used to check speed of the loop
  uint64_t readClock() {
    uint64_t temp = timer_hw->timerawh;
    return (temp << 32) | timer_hw->timerawl;
  }
  void timeTracker() {
    lapTime = runTime - loopTime;
    loopTime = runTime;                                 // Update previousTime variable to give us a reference point for next loop
    runTime = readClock();   // Store the current time in a uniform variable for this program loop
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
  #define MPLEX_1_PIN 4
  #define MPLEX_2_PIN 5
  #define MPLEX_4_PIN 2
  #define MPLEX_8_PIN 3
  #define COLUMN_PIN_0 6
  #define COLUMN_PIN_1 7
  #define COLUMN_PIN_2 8
  #define COLUMN_PIN_3 9
  #define COLUMN_PIN_4 10
  #define COLUMN_PIN_5 11
  #define COLUMN_PIN_6 12
  #define COLUMN_PIN_7 13
  #define COLUMN_PIN_8 14
  #define COLUMN_PIN_9 15
  /*
    There are 140 LED pixels on the Hexboard.
    LED instructions all go through the LED_PIN.
    It so happens that each LED pixel corresponds
    to one and only one hex button, so both a LED
    and its button can have the same index from 0-139.
    Since these parameters are pre-defined by the
    hardware build, the dimensions of the grid
    are therefore constants.
  */
  #define LED_COUNT 140
  #define COLCOUNT 10
  #define ROWCOUNT 14
  /*
    Of the 140 buttons, 7 are offset to the bottom left
    quadrant of the Hexboard and are reserved as command
    buttons. Their LED reference is pre-defined here.
    If you want those seven buttons remapped to play
    notes, you may wish to change or remove these
    variables and alter the value of CMDCOUNT to agree
    with how many buttons you reserve for non-note use.
  */
  #define CMDBTN_0 0
  #define CMDBTN_1 20
  #define CMDBTN_2 40
  #define CMDBTN_3 60
  #define CMDBTN_4 80
  #define CMDBTN_5 100
  #define CMDBTN_6 120
  #define CMDCOUNT 7
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
  buttonDef h[LED_COUNT];        
  
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
    for (byte i = 0; i < LED_COUNT; i++) {
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
    These values support correct MIDI output.
    Note frequencies are converted to MIDI note
    and pitch bend messages assuming note 69
    equals concert A4, as defined below. 
  */
  #define CONCERT_A_HZ 440.0
  /*
    Pitch bend messages are calibrated 
    to a pitch bend range where
    -8192 to 8191 = -200 to +200 cents, 
    or two semitones.
  */
  #define PITCH_BEND_SEMIS 2
  /*
    Create a new instance of the Arduino MIDI Library,
    and attach usb_midi as the transport.
  */
  Adafruit_USBD_MIDI usb_midi;
  MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);
  std::queue<byte> MPEchQueue;
  byte MPEpitchBendsNeeded; 

  float freqToMIDI(float Hz) {             // formula to convert from Hz to MIDI note
    return 69.0 + 12.0 * log2f(Hz / 440.0);
  }
  float MIDItoFreq(float MIDI) {           // formula to convert from MIDI note to Hz
    return 440.0 * exp2((MIDI - 69.0) / 12.0);
  }
  float stepsToMIDI(int16_t stepsFromA) {  // return the MIDI pitch associated
    return freqToMIDI(CONCERT_A_HZ) + ((float)stepsFromA * (float)current.tuning().stepSize / 100.0);
  }

  void setPitchBendRange(byte Ch, byte semitones) {
    MIDI.beginRpn(0, Ch);
    MIDI.sendRpnValue(semitones << 7, Ch);
    MIDI.endRpn(Ch);
    sendToLog(
      "set pitch bend range on ch " +
      std::to_string(Ch) + " to be " + 
      std::to_string(semitones) + " semitones"
    );
  }

  void setMPEzone(byte masterCh, byte sizeOfZone) {
    MIDI.beginRpn(6, masterCh);
    MIDI.sendRpnValue(sizeOfZone << 7, masterCh);
    MIDI.endRpn(masterCh);
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
      MIDI.sendControlChange(123, 0, i);
      setPitchBendRange(i, PITCH_BEND_SEMIS);   
    }
  }

  void sendMIDImodulationToCh1() {
    MIDI.sendControlChange(1, modWheel.curValue, 1);
    sendToLog("sent mod value " + std::to_string(modWheel.curValue) + " to ch 1");
  }

  void sendMIDIpitchBendToCh1() {
    MIDI.sendPitchBend(pbWheel.curValue, 1);
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
        MIDI.sendNoteOn(h[x].note, velWheel.curValue, h[x].MIDIch); // ch 1-16
        MIDI.sendPitchBend(h[x].bend, h[x].MIDIch); // ch 1-16
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
      MIDI.sendNoteOff(h[x].note, velWheel.curValue, h[x].MIDIch);    
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
    MIDI.begin(MIDI_CHANNEL_OMNI);                  // This will also call usb_midi's begin()
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
  #define AUDIO_PIN 25
  #define AUDIO_SLICE 4
  #define AUDIO_CHNL 1
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
    be moved to a Testing menu option.
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
  uint64_t arpeggiateLength = 65'536;         // in microseconds. approx a 1/32 note at 114 BPM

  // RUN ON CORE 2
  void poll() {
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    timer_hw->alarm[ALARM_NUM] = readClock() + POLL_INTERVAL_IN_MICROSECONDS;
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
    pwm_set_chan_level(PIEZO_SLICE, PIEZO_CHNL, level);
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
    for (byte i = 1; i <= LED_COUNT; i++) {
      byte j = positiveMod(arpeggiatingNow + i, LED_COUNT);
      if ((h[j].MIDIch) && (!h[j].isCmd)) {
        n = j;
        break;
      }
    }
    return n;
  }
  void replaceMonoSynthWith(byte x) {
    h[arpeggiatingNow].synthCh = 0;
    if (arpeggiatingNow != x) {
      arpeggiatingNow = x;
      if (arpeggiatingNow != UNUSED_NOTE) {
        h[arpeggiatingNow].synthCh = 1;
        setSynthFreq(h[arpeggiatingNow].frequency, 1);
      } else {
        setSynthFreq(0, 1);
      }
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
    for (byte i = 0; i < LED_COUNT; i++) {
      h[i].synthCh = 0;
    }
    if (playbackMode == SYNTH_POLY) {
      for (byte i = 0; i < POLYPHONY_LIMIT; i++) {
        synthChQueue.push(i + 1);
      }
    }
  }
  
  void updateSynthWhenPitchBend() {
    MIDI.sendPitchBend(pbWheel.curValue, 1);
    for (byte i = 0; i < LED_COUNT; i++) {
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
      replaceMonoSynthWith(findNextHeldNote());
    }
    if (playbackMode == SYNTH_POLY) {
      if (h[x].synthCh) {
        setSynthFreq(0, h[x].synthCh);
        synthChQueue.push(h[x].synthCh);
        h[x].synthCh = 0;
      }
    }
  }

  void setupSynth() {
    gpio_set_function(PIEZO_PIN, GPIO_FUNC_PWM);      // set that pin as PWM
    pwm_set_phase_correct(PIEZO_SLICE, true);           // phase correct sounds better
    pwm_set_wrap(PIEZO_SLICE, 254);                     // 0 - 254 allows 0 - 255 level
    pwm_set_clkdiv(PIEZO_SLICE, 1.0f);                  // run at full clock speed
    pwm_set_chan_level(PIEZO_SLICE, PIEZO_CHNL, 0);        // initialize at zero to prevent whining sound
    pwm_set_enabled(PIEZO_SLICE, true);                 // ENGAGE!
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);  // initialize the timer
    irq_set_exclusive_handler(ALARM_IRQ, poll);     // function to run every interrupt
    irq_set_enabled(ALARM_IRQ, true);               // ENGAGE!
    timer_hw->alarm[ALARM_NUM] = readClock() + POLL_INTERVAL_IN_MICROSECONDS;
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
          h[i].bend = (ldexp(N - h[i].note, 13) / PITCH_BEND_SEMIS);
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
  GEMItem  menuTuningBack("<< Back", menuPageMain);
  GEMItem  menuGotoTuning("Tuning", menuPageTuning);
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
  GEMPage  menuPageTesting("Advanced");
  GEMItem  menuGotoTesting("Advanced", menuPageTesting);
  GEMItem  menuTestingBack("<< Back", menuPageMain);
  GEMPage  menuPageReboot("Ready to flash! Plug into PC");
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
  GEMItem  menuItemVersion("v0.6.0", fakeButton);
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
  SelectOptionByte optionByteYesOrNo[] =  { { "No", 0 }, { "Yes" , 1 } };
  GEMSelect selectYesOrNo( sizeof(optionByteYesOrNo)  / sizeof(SelectOptionByte), optionByteYesOrNo);
  GEMItem  menuItemScaleLock( "Scale lock?", scaleLock, selectYesOrNo);
  GEMItem  menuItemPercep( "Fix color:", perceptual, selectYesOrNo, setLEDcolorCodes);
  GEMItem  menuItemShiftColor( "ColorByKey", paletteBeginsAtKeyCenter, selectYesOrNo, setLEDcolorCodes);
  GEMItem  menuItemWheelAlt( "Alt wheel?", wheelMode, selectYesOrNo);

  SelectOptionByte optionByteWheelType[] = { { "Springy", 0 }, { "Sticky", 1} };
  GEMSelect selectWheelType( sizeof(optionByteWheelType) / sizeof(SelectOptionByte), optionByteWheelType);
  GEMItem  menuItemPBBehave( "Pitch bend", pbSticky, selectWheelType);
  GEMItem  menuItemModBehave( "Mod wheel", modSticky, selectWheelType);

  SelectOptionByte optionBytePlayback[] = { { "Off", SYNTH_OFF }, { "Mono", SYNTH_MONO }, { "Arp'gio", SYNTH_ARPEGGIO }, { "Poly", SYNTH_POLY } };
  GEMSelect selectPlayback(sizeof(optionBytePlayback) / sizeof(SelectOptionByte), optionBytePlayback);
  GEMItem  menuItemPlayback(  "Synth mode:",       playbackMode,  selectPlayback, resetSynthFreqs);

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

  SelectOptionByte optionByteBright[] = { { "Dim", BRIGHT_DIM}, {"Low", BRIGHT_LOW}, {"Normal", BRIGHT_MID}, {"High", BRIGHT_HIGH}, {"THE SUN", BRIGHT_MAX } };
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
      menuPageSynth.addMenuItem(menuSynthBack);
    menuPageMain.addMenuItem(menuItemTransposeSteps);
    menuPageMain.addMenuItem(menuGotoTesting);
      menuPageTesting.addMenuItem(menuItemVersion);
      menuPageTesting.addMenuItem(menuItemPercep);
      menuPageTesting.addMenuItem(menuItemShiftColor);
      menuPageTesting.addMenuItem(menuItemWheelAlt);
      menuPageTesting.addMenuItem(menuItemPBBehave);
      menuPageTesting.addMenuItem(menuItemModBehave);
      menuPageTesting.addMenuItem(menuItemUSBBootloader);
      menuPageTesting.addMenuItem(menuTestingBack);
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
      Rotary knob code:
        https://github.com/buxtronix/arduino/tree/master/libraries/Rotary

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
  byte rotaryState = 0;
  const byte rotaryStateTable[8][4] = {
    {0,5,1,0},{2,0,1,0},{2,3,1,0},{2,3,0,8},
    {0,5,1,0},{6,5,0,0},{6,5,7,0},{6,0,7,16}
  };
  byte storeRotaryTurn = 0;
  bool rotaryClicked = HIGH;          

  void readHexes() {
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
        if (h[i].btnState == 1) {
          h[i].timePressed = runTime;          // log the time
        }
        pinMode(p, INPUT);                     // Set the selected column pin back to INPUT mode (0V / LOW).
       }
    }
    for (byte i = 0; i < LED_COUNT; i++) {   // For all buttons in the deck
      switch (h[i].btnState) {
        case 1: // just pressed
          if (h[i].isCmd) {
            cmdOn(i);
          } else if (h[i].inScale || (!scaleLock)) {
            tryMIDInoteOn(i);
            trySynthNoteOn(i);
          }
          break;
        case 2: // just released
          if (h[i].isCmd) {
            cmdOff(i);
          } else if (h[i].inScale || (!scaleLock)) {
            tryMIDInoteOff(i);
            trySynthNoteOff(i); 
          }
          break;
        case 3: // held
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
        updateSynthWhenPitchBend();
      }
    } else {
      modWheel.setTargetValue();
      upd = modWheel.updateValue(runTime);
      if (upd) {
        sendMIDImodulationToCh1();
      }
    }
  }
  void setupRotary() {
    pinMode(ROT_PIN_A, INPUT_PULLUP);
    pinMode(ROT_PIN_B, INPUT_PULLUP);
    pinMode(ROT_PIN_C, INPUT_PULLUP);
  }
  void readKnob() {
    rotaryState = rotaryStateTable[rotaryState & 7][
      (digitalRead(ROT_PIN_B) << 1) | digitalRead(ROT_PIN_A)
    ];
    if (rotaryState & 24) {
      storeRotaryTurn = rotaryState;
    }
  }
  void dealWithRotary() {
    if (menu.readyForKey()) {
      bool temp = digitalRead(ROT_PIN_C);
      if (temp > rotaryClicked) {
        menu.registerKeyPress(GEM_KEY_OK);
        screenTime = 0;
      }
      rotaryClicked = temp;
      if (storeRotaryTurn != 0) {
        menu.registerKeyPress((storeRotaryTurn == 8) ? GEM_KEY_UP : GEM_KEY_DOWN);
        storeRotaryTurn = 0;
        screenTime = 0;
      }
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
    setupRotary();
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
    setupSynth();
  }
  void loop1() {  // run on second core
    readKnob();
  }
