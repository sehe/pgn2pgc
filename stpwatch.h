#pragma once
///////////////////////////////////////////////////////////////////////////////
//	StpWatch.h
//
//	A StopWatch class and a function Delay (number of seconds)
//
//	Use class StopWatch exactly as you would a stopwatch.
//	start..stop..time..reset..start..time..time..reset..start..time..stop..
//	start..stop..start..stop..time..reset..time..start..start..stop..time..
//	You get the idea.
//
//	Useful as a profiler for programs, as long as you don't need too much
//	accuracy.  If you need more accuracy, see "Zen of Code Optimization"
//	for a non-portable timer in assembly that has a very high accuracy.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////
#include <chrono>
#include <utility> // std::exchange

class StopWatch {
public:
  using Clock = std::chrono::high_resolution_clock;
  using Duration = Clock::duration;

  void start() {
    if (std::exchange(fIsTiming, true))
      fStart = Clock::now();
  }

  void stop() {
    if (std::exchange(fIsTiming, false))
      fCumTime += Clock::now() - fStart;
  }

  void reset() { fCumTime = {}; }

  Duration time() { return fCumTime; }

private:
  bool fIsTiming = false;   // are we between start and stop?
  Clock::time_point fStart; // time when we hit the start button
  Duration fCumTime;      // the cummulative time
};
