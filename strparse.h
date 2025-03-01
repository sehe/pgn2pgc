#pragma once
///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
// SEHE: FIXME remove unsafe C-style string functions
///////////////////////////////////////////////////////////////////////////////

#include <string>
// remove trailing and leading whitespace
void RemoveWhiteSpace(std::string& s);
void RemoveChars(std::string& s, std::string_view remove);
