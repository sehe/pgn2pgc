///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
///////////////////////////////////////////////////////////////////////////////
#include <cassert>
#include <cctype>
#include <cstring>

#include "strparse.h"

// remove trailing and leading whitespace
char* RemoveWhiteSpace(char c[]) {
    assert(c);
    if (!std::strlen(c))
        return c;

    while (std::isspace(*c))
        ++c;

    if (!std::strlen(c))
        return c;

    while (std::isspace(c[std::strlen(c) - 1]))
        c[std::strlen(c) - 1] = '\0';

    return c;
}

// remove all of the characters that are in remove[] from c[], return the new
// c[]
char* RemoveChars(char c[], char const remove[]) {
    assert(remove);
    assert(c);

    size_t source = 0, dest = 0;

    while (c[source] != '\0') {
        // not a remove character
        if (std::strcspn(&c[source], remove)) {
            c[dest] = c[source];
            ++dest;
        }
        ++source;
    }
    assert(dest <= source);
    c[dest] = '\0';
    return c;
}
