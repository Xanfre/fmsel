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

#include "lang.h"
#include "os.h"
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else
#ifdef USE_XDG
#include <limits.h>
#endif
#define _strnicmp strncasecmp
#define strnicmp strncasecmp
#endif

#ifdef LOCALIZATION_SUPPORT

#include <FL/fl_utf8.h>

#include <string>
#include <unordered_map>
#include <algorithm>
using std::string;
#ifdef _MSC_VER
using std::tr1::unordered_map;
#else
using std::unordered_map;
#endif


char* skip_ws(char *s);


// case sensitive string based hash key
typedef string tStrHashKey;

/*// override default hash calc for better hash search performance (original algo doesn't include all characters
// in hash calc if string is longer than 10 chars, which reduces uniqueness)
struct StrKeyHash : public std::unary_function<tStrHashKey, size_t>
{
	size_t operator()(const tStrHashKey& _Keyval) const
	{
		size_t _Val = 2166136261U;
		size_t _First = 0;
		size_t _Last = _Keyval.size();
		for(; _First < _Last; _First++)
			_Val = 16777619U * _Val ^ (size_t)_Keyval[_First];
		return _Val;
	}
};*/

typedef unordered_map<tStrHashKey, string> tLangHash;


static tLangHash g_langdb;
BOOL localization_supported = 0;
BOOL localization_initialized = 0;


#define isspace_(_ch) isspace((unsigned char)(_ch))
static inline int isodigit(int C)
{
	return C >= '0' && C <= '7';
}

char* next_line(char *s, char *&eol)
{
	// scans to next line without null-terminating or trimming trailing spaces (like the other next_line in fmsel.cpp)
	// returns pointer to end of line that can by null-terminated by caller to get the same effect

	eol = NULL;

	if (!*s)
		return s;

	char *t = s;

	// find end of line
	while (*s && *s != '\n' && *s != '\r') s++;

	char *q = *s ? s-1 : s;
	const char nl = *s;

	eol = s;

	if (*s)
		s++;

	// skip past \r\n combo
	if (nl == '\r' && *s == '\n')
		s++;

	// trim trailing spaces
	while (q >= t && isspace_(*q))
	{
		eol = q;
		q--;
	}

	return s;
}

static char* parse_string(char *s)
{
	// handles multiple consecutive string literals and concatenates them

	s = skip_ws(s);

	if (*s != '"')
		return NULL;

	s++;
	char *start = s;
	char *dst = s;

	while (*s)
	{
		// check for escaped characters
		if (*s == '\\')
		{
			s++;
			switch (*s)
			{
			case '\r':
			case '\n':
				// illegal line break in string, abort parsing
				ASSERT(0);
				*dst = 0;
				return start;

			case '"': *dst++ = '"'; break;
			case 'n': *dst++ = '\n'; break;
			case 'r': *dst++ = '\r'; break;
			case 't': *dst++ = '\t'; break;
			case '\\': *dst++ = '\\'; break;
			case '\'': *dst++ = '\''; break;
			case 'a': *dst++ = '\a'; break;
			case 'b': *dst++ = '\b'; break;
			case 'f': *dst++ = '\f'; break;
			case 'v': *dst++ = '\v'; break;
			case '?': *dst++ = '\?'; break;
			case 'x':
				if ( isxdigit((unsigned char)s[1]) )
				{
					if ( isxdigit((unsigned char)s[2]) )
					{
						char hex[3] = { s[1], s[2], 0 };
						*dst++ = (unsigned char) strtoul(hex, NULL, 16);
						s += 2;
					}
					else
					{
						char hex[2] = { s[1], 0 };
						*dst++ = (unsigned char) strtoul(hex, NULL, 16);
						s++;
					}
				}
				else
					*dst++ = 'x';
				break;
			default:
				if ( isodigit((unsigned char)*s) )
				{
					if ( isodigit((unsigned char)s[1]) )
					{
						if ( isodigit((unsigned char)s[2]) )
						{
							char oct[4] = { *s, s[1], s[2], 0 };
							*dst++ = (unsigned char) strtoul(oct, NULL, 8);
							s += 2;
						}
						else
						{
							char oct[3] = { *s, s[1], 0 };
							*dst++ = (unsigned char) strtoul(oct, NULL, 8);
							s++;
						}
					}
					else
					{
						char oct[2] = { *s, 0 };
						*dst++ = atoi(oct);
					}
				}
			}
			s++;
		}
		else if (*s == '"')
		{
			// end of string, check if there's another string to concatenate
			s++;
			s = skip_ws(s);
			if (*s == '"')
			{
				// found another string, continue parsing
				s++;
				continue;
			}

			*dst = 0;
			return start;
		}
		else
			*dst++ = *s++;
	}

	// badly formatted string (return it anyway)
	ASSERT(0);

	*dst = 0;

	return start;
}


void InitLocalization()
{
	ASSERT(!localization_initialized);

	localization_initialized = 1;

#if defined(_WIN32) || !defined(USE_XDG)
	// generate filename with path and file title same as dll file

	char fname[2048] = { 0, };
	if ( !GetFmselModulePath(fname, sizeof(fname)) )
		return;

	char *ext = strrchr(fname, '.');
	if (!ext)
		return;
	strcpy(ext+1, "po");
#else
	// get file from the XDG data directory

	char fname[PATH_MAX];
	const char *xdg = getenv("XDG_DATA_HOME");
	int result = snprintf(fname, sizeof(fname), "%s%s/fmsel/fmsel.po", xdg ? xdg : getenv("HOME"), xdg ? "" : "/.local/share");
	if (result < 0 || result >= PATH_MAX)
		return;
#endif

	// read file into buffer

	FILE *f = fopen(fname, "rb");
	if (!f)
		return;

#ifdef _WIN32
	int n = _filelength( _fileno(f) );
#else
	fseek(f, 0, SEEK_END);
	int n = ftell(f);
	fseek(f, 0, SEEK_SET);
#endif
	if (n <= 0)
	{
		fclose(f);
		return;
	}

	char *data = new char[n+2];
	data[n] = 0;
	data[n+1] = 0;

	fread(data, 1, n, f);
	fclose(f);

	// parse file

	char *msgid = NULL;
	char *prev_eol = NULL;

	char *s = data;
	while (*s)
	{
		char *eol;
		char *nextline = next_line(s, eol);

		if (*s != '"' && msgid)
		{
			// in order handle multiline msgid strings (that need to concatenated) next_line does't null-terminate each
			// line, instead it waits until it finds the first line not starting with " and null-terminates previous line
			if (prev_eol)
				*prev_eol = 0;
		}

		if (!_strnicmp(s, "msgid", 5) && isspace_(s[5]))
		{
			msgid = s + 5;
		}
		else if (!_strnicmp(s, "msgstr", 6) && isspace_(s[6]))
		{
			if (msgid)
			{
				msgid = parse_string(msgid);
				if (msgid && *msgid)
				{
					// check for multline string literals for current msgstr
					while (nextline && *nextline == '"')
						nextline = next_line(nextline, eol);
					if (eol)
						*eol = 0;

					char *msgstr = parse_string(s + 6);
					if (msgstr && *msgstr)
					{
						// add translation string to db
						//TRACE("**SRC=%s"DBGNEWLINE" *DST=%s", msgid, msgstr);
						g_langdb[msgid] = msgstr;
						localization_supported = 1;
					}
				}

				msgid = NULL;
			}
		}

		s = nextline;
		prev_eol = eol;
	}

	//

	delete[] data;
}

void TermLocalization()
{
	g_langdb.clear();
}


const char* TranslateString(const char *s)
{
	ASSERT(localization_supported);

	tLangHash::iterator dataIter = g_langdb.find(s);
	if (dataIter != g_langdb.end())
		return dataIter->second.c_str();

	return s;
}

BOOL UnTranslateTag(string &s)
{
	// first see that string isn't english, on the off chance that a localized string is the same as another english string
	// (not entirely failsafe. requires there to be a localization entry for the english tag string)
	string tmp = LOCALIZED_TAG_PREFIX;
	tmp += s;
	const char *xlat = TranslateString( tmp.c_str() );
	if (xlat != tmp.c_str())
		// 's' was found as a non-localized tag string
		return 0;

	// search entire db for any localized string matching 's' that where the key has a "tag_" prefix
	const int prefixlen = strlen(LOCALIZED_TAG_PREFIX);
	for (tLangHash::iterator it=g_langdb.begin(); it!=g_langdb.end(); it++)
	{
		if ( !fl_utf_strcasecmp(s.c_str(), it->second.c_str()) )
		{
			// found match, see if key has "tag_" prefix
			if ( !strnicmp(it->first.c_str(), LOCALIZED_TAG_PREFIX, prefixlen) )
			{
				// yes it does, we found a reverse-translation
				s = it->first.c_str() + prefixlen;
				return 1;
			}
		}
	}

	return 0;
}

#endif // LOCALIZATION_SUPPORT
