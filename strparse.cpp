///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
///////////////////////////////////////////////////////////////////////////////
#include "strparse.h"

// remove trailing and leading whitespace
void RemoveWhiteSpace(std::string& s) {
    if (auto b = s.find_first_not_of(" \t\n\f\r"), e = s.find_last_not_of(" \t\n\f\r");
        b == std::string::npos) {
        s.clear();
    } else {
        s = s.substr(b, e - b + 1);
    }
}

void RemoveChars(std::string& s, std::string_view remove) {
    for (size_t p = 0; p < s.size(); ++p) {
        if (remove.contains(s[p])) {
            s.erase(p, 1);
            --p;
        }
    }
}
