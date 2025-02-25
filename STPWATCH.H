#ifndef J_STOPWATCH_H
#define J_STOPWATCH_H
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
//  The accuracy is the same as clock() (about 50ms).
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

// HEADER FILES

// standard
#include <time.h>

// my own
#include "joshdefs.h"

class StopWatch
	{
	public:

		typedef float stopWatchT; // the type that the seconds are reported in

		StopWatch	(); // constructor

		// start the timer
		void start();

		// stop the timer
		void stop();

		// return the time
		stopWatchT	time();

		// clears the time
		void reset();

		// copy constructor and assignment operator
		// 	defaults are okay
		// 		StopWatch (const StopWatch&);
		//		StopWatch& operator= (const StopWatch&);

		// destructor
		~StopWatch() {}

	private:

		bool fIsTiming; 	// are we between start and stop?
		clock_t fStart; 	// time when we hit the start button
		clock_t	fCumTime; // the cummulative time
	};

#endif	// J_STOPWATCH_H