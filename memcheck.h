#ifndef J_MEMCHECK_H
#define J_MEMCHECK_H
///////////////////////////////////////////////////////////////////////////////
//	Memcheck.h
//
//	Overload new and delete to provide checking against memory leaks and out-
//		of-bounds checking.
//
//	Use New() and Delete() wherever you would normally use new and delete and
//		NewArray() and DeleteArray() instead of new[] and delete[].
//
//	If an error is detected, the program aborts, sending any information to the
// 		file STARTUP_INFO_FILE_NAME.  Errors that are detected include: failure
//		to call delete, calling delete twice for the same object, going out of
//		bounds, using delete instead of delete[].
//
//	If USE_MEMCHECK is not defined then it just translates back to new and delete.
//
//	Link in Memcheck.cpp in order to have the file gStartupInfoFileName created
//		with any debug info in it.
//
//	PERFORMANCE ISSUES:
//		Using Memcheck.cpp has a very large negative impact on performance, both
//		in terms of speed and size.  However, during testing I wouldn't do
//		without them.
//
//	CREDITS:
//		Most of this code was written by Dr. B. Domzy of the Computer Science
//			Department at Trent University.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug
//		reports, bug fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <fstream>

#include "joshdefs.h"


#ifndef USE_MEMCHECK

// do not put () in these macros to make them 'safer', the compiler may choke
//		unexpectedly when using them in some contexts.  It may be better to name
//		them NEW and DELETE in the future.

#define New(t)          new t
#define NewArray(t, c)  new t [ c ]

#define Delete(p)       delete p
#define DeleteArray(p)  delete [] p

#else

// the name of the file that all debug information is placed in.
#define STARTUP_INFO_FILE_NAME "main.err"

// these are declared and/or defined in memcheck.cpp

class Checker;

class Startup
	{
	public:

		Startup (const char* errFile);
		~Startup ();

		static void check  (void*, const char*, int, bool);
		static void record (void*, size_t, size_t, const char*, int, bool, size_t);

	private:

		// Disallow copy and assignment.
		Startup (const Startup&);
		Startup& operator = (const Startup&);

	private:

			static int       _count;
			static ofstream* _ferr;
			static Checker*  _checker;
	};


void* operator new (size_t, const char*, int, bool, int, size_t);
#ifndef NNEW
void* operator new [] (size_t, const char*, int, bool, int, size_t);
#endif


#define New(t)         new (__FILE__, __LINE__, false, 0, 0)         t
#define NewArray(t, c) new (__FILE__, __LINE__, true,  c, sizeof(t)) t [ c ]

#define Delete(p) Startup::check(p, __FILE__, __LINE__, false), delete p

#define DeleteArray(p) Startup::check(p, __FILE__, __LINE__, true), delete [] p

#endif // USE_MEMCHECK


#endif // MemCheck.h
