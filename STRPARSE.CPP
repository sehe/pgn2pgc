///////////////////////////////////////////////////////////////////////////////
//	StringParse.cpp
//
// small functions to help parse strings
//
///////////////////////////////////////////////////////////////////////////////
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "joshdefs.h"
#include "strparse.h"

// remove trailing and leading whitespace
char* RemoveWhiteSpace(char c[])
{
	assert(c);
	if(!strlen(c))
		return c;

	while(isspace(*c))
		++c;

	if(!strlen(c))
		return c;

	while(isspace(c[strlen(c) - 1]))
		c[strlen(c) - 1] = '\0';

	return c;

}

// remove all of the characters that are in remove[] from c[], return the new c[]
char* RemoveChars(char c[], const char remove[])
{
	assert(remove);
	assert(c);

	size_t source = 0, dest = 0;

//	unsigned allChars = strlen(c);
	while(c[source] != '\0')
		{
			// not a remove character
			if(strcspn(&c[source], remove)) // BC documentation is wrong about this function
			{
				c[dest] = c[source];
				++dest;
			}
			++source;
		}
	assert(dest <= source);
	c[dest] = '\0';
	return c;
}
