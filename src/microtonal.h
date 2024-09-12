	#pragma once
	#include <Arduino.h>
	#include "constants.h"
	
// @microtonal

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