	#pragma once

	#define HARDWARE_UNKNOWN 0
	#define HARDWARE_V1_1 1
	#define HARDWARE_V1_2 2           // Software-detected hardware revision

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
  #define SDAPIN 16
  #define SCLPIN 17

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
  #define ROWCOUNT 16
  #define BTN_COUNT COLCOUNT*ROWCOUNT
	
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




  #define SYNTH_OFF 0
  #define SYNTH_MONO 1
  #define SYNTH_ARPEGGIO 2
  #define SYNTH_POLY 3

  #define WAVEFORM_SINE 0
  #define WAVEFORM_STRINGS 1
  #define WAVEFORM_CLARINET 2
  #define WAVEFORM_HYBRID 7
  #define WAVEFORM_SQUARE 8
  #define WAVEFORM_SAW 9
  #define WAVEFORM_TRIANGLE 10 
	
	#define RAINBOW_MODE 0
  #define TIERED_COLOR_MODE 1
  #define ALTERNATE_COLOR_MODE 2
	
  #define ANIMATE_NONE 0
  #define ANIMATE_STAR 1 
  #define ANIMATE_SPLASH 2 
  #define ANIMATE_ORBIT 3 
  #define ANIMATE_OCTAVE 4 
  #define ANIMATE_BY_NOTE 5
	
  #define BRIGHT_MAX 255
  #define BRIGHT_HIGH 210
  #define BRIGHT_MID 180
  #define BRIGHT_LOW 150
  #define BRIGHT_DIM 110
  #define BRIGHT_DIMMER 70
  #define BRIGHT_OFF 0

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
	  // midiD takes the following bitwise flags
  #define MIDID_NONE 0
  #define MIDID_USB 1
  #define MIDID_SER 2
  #define MIDID_BOTH 3

  /*
    When sending smoothly-varying pitch bend
    or modulation messages over MIDI, the
    code uses a cool-down period of about
    1/30 of a second in between messages, enough
    for changes to sound continuous without
    overloading the MIDI message queue.
  */
  #define CC_MSG_COOLDOWN_MICROSECONDS 32768
	
  #define DIAGNOSTICS_ON true 
