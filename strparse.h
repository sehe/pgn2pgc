#pragma once
///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
// SEHE: FIXME remove unsafe C-style string functions
///////////////////////////////////////////////////////////////////////////////

// remove trailing and leading whitespace
char* RemoveWhiteSpace(char c[]);

// remove all of the characters that are in remove[] from c[], return the new
// c[]
char* RemoveChars(char c[], char const remove[]);
