/* FMSel is free software; you can redistribute it and/or modify
 * it under the terms of the FLTK License.
 *
 * FMSel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * FLTK License for more details.
 *
 * You should have received a copy of the FLTK License along with
 * FMSel.
 */

#pragma once

#ifndef _LANG_H_
#define _LANG_H_

#include "os.h"
#include "dbgutil.h"

#include <string>


// enable localization support
#define LOCALIZATION_SUPPORT

#ifdef LOCALIZATION_SUPPORT


#define LOCALIZED_TAG_PREFIX "tag_"


void InitLocalization();
void TermLocalization();

// reverse lookup a tag string to see if there's a "tag_" prefixed entry
// if so it returns the original language string without the "tag_" prefix in 's'
// returns FALSE if no translation was found
BOOL UnTranslateTag(std::string &s);

// internal functions and vars
const char* TranslateString(const char *s);
extern BOOL localization_supported;
extern BOOL localization_initialized;

// macros to use for localizable strings and conversion

__inline const char* $(const char *s)
{
	// make sure no global var uses localization on init
	// not supported because localization subsystem has to be inited first
	ASSERT(localization_initialized);

	return localization_supported ? TranslateString(s) : s;
}

#define $mid(_x) + string($(_x)) +
#define $right(_x) + string($(_x))
#define $left(_x) string($(_x)) +

#else // !defined(LOCALIZATION_SUPPORT)

#define InitLocalization()
#define TermLocalization()

#define $(_x) _x
#define $mid(_x) _x
#define $right(_x) _x
#define $left(_x) _x

#endif // LOCALIZATION_SUPPORT

#endif // _LANG_H_
