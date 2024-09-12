#pragma once
#include <Arduino.h>
#include "hardware/timer.h"

uint64_t getTheCurrentTime() {
  uint64_t temp = timer_hw->timerawh;
  return (temp << 32) | timer_hw->timerawl;
}

class softTimer {
  public:
    softTimer();                  // declare constructor
    void start(uint64_t _delay_uS, uint64_t _defer_uS);
                                  // declare function to start the timer
    void stop();                  // declare function to stop the timer
    void repeat();                // declare function to repeat the timer once after it finishes
    void restart();               // declare function to restart the timer immediately
    void finish();                // declare function to flag timer as finished but have it keep running
    bool justFinished();          // declare function to return whether the timer just finished (and stops if so)
    bool isRunning();             // declare function to return whether the timer is still running
    uint64_t execWhenFinished(void (*_callback)());
                                  // declare function to run callback function when timer is just finished
                                  // and return the length of time the callback function took to run
    uint64_t getStartTime();      // declare function to return the start time parameter
    uint64_t getElapsed();        // declare function to return the elapsed time on the timer
    uint64_t getRemaining();      // declare function to return the time remaining on the timer
    uint64_t getDelay();          // declare function to return the delay entered on this timer
  private:
    uint64_t startTime;
    uint64_t delay_uS;
    bool running;
    bool finishNow;
};

softTimer::softTimer() {
  startTime = 0;
  delay_uS = 0;
  running = false;
  finishNow = false;
}

void softTimer::start(uint64_t _delay_uS, uint64_t _defer_uS) {
  startTime = getTheCurrentTime() + _defer_uS;
  delay_uS = _delay_uS;
  running = true;
  finishNow = false;
}

void softTimer::stop() {
  running = false;
  finishNow = false;
}

void softTimer::repeat() {
  startTime = startTime + delay_uS;
  running = true;
  finishNow = false;  
}

void softTimer::restart() {
  start(delay_uS, 0);
}

void softTimer::finish() {
  finishNow = true;
}

bool softTimer::justFinished() {
  if (running && (finishNow || (getElapsed() >= delay_uS))) {
    stop();
    return true;
  } // else {
  return false;  
}

bool softTimer::isRunning() {
  return running;
}

uint64_t softTimer::execWhenFinished(void (*_callback)()) {
  uint64_t temp = 0;
  if (justFinished()) {
      temp = getTheCurrentTime();
      _callback();
      temp = getTheCurrentTime() - temp;
      repeat();
  }
  return temp;
}

uint64_t softTimer::getStartTime() {
  return startTime;  
}

uint64_t softTimer::getElapsed() {
  uint64_t temp = getTheCurrentTime();
  return (temp < startTime ? 0 : temp - startTime);
}

uint64_t softTimer::getRemaining() {
  if (running) {
    uint64_t temp = getElapsed();
    if (finishNow || (temp >= delay_uS)) {
      return 0;
    } else {
      return (delay_uS - temp);
    }
  } else {
    return 0;
  }  
}

uint64_t softTimer::getDelay() {
  return delay_uS;
}