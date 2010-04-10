#ifndef dirr3_wildmatch_hh
#define dirr3_wildmatch_hh

/***********************************************
 *
 * WildMatch(Pattern, What)
 *
 *   Compares with wildcards.
 *
 *   This routine is a descendant of the fnmatch()
 *   function in the GNU fileutils-3.12 package.
 *
 *   Supported wildcards:
 *       *
 *           matches multiple characters
 *       ?
 *           matches one character
 *       [much]
 *           "much" may have - (range) and ^ or ! (negate)
 *       \d
 *           matches a digit (same as [0-9])
 *       \w
 *           matches alpha (same as [a-zA-Z])
 *       \
 *           quote next wildcard
 *
 *   Global variable IgnoreCase controls the case
 *   sensitivity of the operation as 0=case sensitive, 1=not.
 *
 *   Return value: 0=no match, 1=match
 *
 **********************************************************/

extern int IgnoreCase;
extern int WildMatch(const char *pattern, const char *what);

/*
 * rmatch(Name, Items)
 *
 *   Finds a name from the space-separated wildcard list
 *   Uses WildMatch() to compare.
 *
 *   Return value: 0=no match, >0=index of matched string
 */

extern int rmatch(const char *Name, const char *Items);

#endif
