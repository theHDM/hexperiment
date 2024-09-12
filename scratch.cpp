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
	
	button state.
	button location.
	button light codes and animations
	button what synth command
		what channel
		what frequency
		bend? modulation?
	button what MIDI command
		note, channel, base velo
		emulate wheel
		cmd on/off/set
	