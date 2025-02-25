#ifndef J_JOSHDEFS_H
#define J_JOSHDEFS_H
///////////////////////////////////////////////////////////////////////////////
//
//	JoshDefs.h
//
//	January 1995.
//
//  Contents: Common shared data types and macros
//
//	The fact that every file includes this one can be very useful.
//
//	ABOUT THIS FILE: Please send any questions, comments, suggestions, bug-
//		reports, bug-fixes, and useful modifications to joallen@trentu.ca.
//		Released to the public domain.
//		For more information visit www.trentu.ca/~joallen.
//
///////////////////////////////////////////////////////////////////////////////
//
//	REVISION LOG:
//
//	Date			By	Revision
//
//	1999			JTA	Removed old complex macros and functions of NewCheck and
//								replaced with CHECK_POINTER macro.  If exceptions are used
//								you can use the NEW_RETURNS_NULL macro to essentially comment
//								out CHECK_POINTER.
//  2000      JTA Added MemCheck.h, with code written mostly by Dr. Domzy.
//
///////////////////////////////////////////////////////////////////////////////

// Conditional compilation macros
//		Don't actually define them here, instead do it from within the IDE or on
//		the command line.
#define NDEBUG			 // turn off extra debugging and checking code
#define NCAST				 // no new style casts: dynamic_cast<>, static_cast<>, etc.
#define NO_BOOL_TYPE // no bool type
//#define NNEW         // cannot overload new[]
//	new returns NULL when it can't allocate enough memory,
//		instead of throwing an exception
#define NEW_RETURNS_NULL
//#define USE_MEMCHECK // use versions of New() and Delete that check memory management //??! no longer fully trust them, as a GPE sometimes occures during heavy usage, need more investigation as to cause
//#define USE_TRACE // TRACE is a macro used during debugging to quickly output the value of an identifier to stderr


// HEADER FILES

#include <stdlib.h>
#include <iostream.h>
#include <assert.h>


// DATA TYPES

// boolean data type

#ifdef NO_BOOL_TYPE
enum bool {false, true};
#endif


// MACROS

//---------------------------------------------------------------------------
//	Macro:	TOBOOL
//  Author:	Joshua Allen
//
//	Arguments: x, an expression that should be either 0 or
//							non-zero (usually 1)
//
//	Purpose: Converts an expression to boolean;
//						avoids "initializing bool with int" warnings and errors
//
//---------------------------------------------------------------------------
#ifdef NO_BOOL_TYPE
#define TOBOOL(x) ((x)?true:false)
#else
#define TOBOOL(x) (x)
#endif

//---------------------------------------------------------------------------
//	Function:	CHECK_POINTER
//  Author:	Joshua Allen
//
//	Arguments:	p, a pointer
//
//	Purpose:	to assert that a pointer is not null;
//						 used after operator new
//
//	Behavior: if pointer is == null then an error message is printed to cerr
//						and the program exits;
//						does nothing otherwise
//
//	Special:	assert(0) is used in case we're in debug mode so that we can
//							see line & file info, also why a macro is used instead of a
//							function
//
//---------------------------------------------------------------------------
#ifdef NEW_RETURNS_NULL
#define CHECK_POINTER(p) {if(!(p)){\
												cerr<<"\nError: out of dynamic memory, exiting..\n";\
												assert(0);\
												exit(EXIT_FAILURE);}}
#else
#define CHECK_POINTER(p) ((void)0)
#endif

//-----------------------------------------------------------------------------
#ifdef USE_TRACE
#define TRACE(x) cerr<<"\n" #x ": '"<<( x )<<"'"
#else
#define TRACE(x) ((void)0)
#endif

// include at bottom so that memcheck.h will have the necessary definitions
#ifndef J_MEMCHECK_H
#include "memcheck.h" // New(), Delete()
#endif

//-----------------------------------------------------------------------------
// got idea from std::auto_ptr
template <class T>
class AdoptArray
{
public:
	AdoptArray(T* ptr) : fPtr(ptr) {}
	~AdoptArray() { DeleteArray(fPtr); fPtr = 0;}

private:
	AdoptArray(const AdoptArray&); // cannot copy
	AdoptArray& operator=(const AdoptArray&);
	T* fPtr;
};


#endif // JoshDefs.h
