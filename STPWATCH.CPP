//-----------------------------------------------------------------------------
//
//	StpWatch.cpp
//
//		Definitions for StpWatch.h
//
//		ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//			reports, bug fixes, and useful modifications to joallen@trentu.ca.
//			Released to the public domain.
//			For more information visit www.trentu.ca/~joallen.
//
//-----------------------------------------------------------------------------

// HEADER FILES

// standard
#include <time.h>
#include <assert.h>

// my own
#include "stpwatch.h"
#include "joshdefs.h"

StopWatch::StopWatch() :
fStart(0),
fCumTime(0),
fIsTiming(false)
{ }

void StopWatch::start()
	{
		if(!fIsTiming)
		{
			fStart = clock();

			// clock() returns -1 on error
			if(fStart  < 0)
				{
					cerr << "\nClock unable to report time.";
					fStart = 0;
				}

			fIsTiming = true;
		}
	}

void StopWatch::stop()
	{
		if(fIsTiming)
		{
			fCumTime += clock() - fStart;

			// clock() returns -1 on error
			if(fCumTime < 0)
				{
					cerr << "Clock Unable to report time.";
					fCumTime = 0;
				}

			fIsTiming = false;
		}
	}

void StopWatch::reset()
	{
		fStart = fCumTime = 0;
	}

StopWatch::stopWatchT StopWatch::time()
	{
		// CLK_TCK is the number of times the clock ticks per second, may be
		// CLOCKS_PER_SEC on some systems, was unable to determine which was ANSI
		if(fIsTiming)
			{
				// calling stop() and start() is necessary
				stop();
				const stopWatchT tm = (stopWatchT)fCumTime / CLK_TCK;
				start();
				return tm;
			}

		return (stopWatchT)fCumTime / CLK_TCK;
	}

// Test Test Test Test Test Test Test Test Test Test Test Test Test Test
//#define TEST
#ifdef TEST
#undef TEST

#include <iostream.h>
#include <dos.h>

int main()
	{

		char wait[3];
		int i;
		StopWatch tm;

		tm.start();

		for(i = 0; i < 100; ++i)
			{
				cout << endl << tm.time();
			}
		StopWatch tm2(tm);
		tm.stop();

		cin.read(wait, 2);

		for(i = 0; i < 100; ++i)
			{
				cout << endl << tm.time();
			}

		cout << endl << tm2.time();

		tm.reset();
		tm.start();
		delay(500);
		tm.stop();
		cout << endl << tm.time();

		tm.start();
		delay(500);
		tm.stop();
		cout << endl << tm.time();

		tm.reset();
		cout << endl << tm.time();

		return 0;
	}

#endif // test

