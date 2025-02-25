#ifndef J_STRPARSE_H
#define J_STRPARSE_H
///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
///////////////////////////////////////////////////////////////////////////////

// remove trailing and leading whitespace
char* RemoveWhiteSpace(char c[]);

// remove all of the characters that are in remove[] from c[], return the new c[]
char* RemoveChars(char c[], const char remove[]);

#endif // strParse.h