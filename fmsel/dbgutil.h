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

#ifndef _DBGUTIL_H_
#define _DBGUTIL_H_

#ifdef _WIN32
#define DBGNEWLINE "\r\n"
#define DBGNEWLINEW L"\r\n"
#else
#define DBGNEWLINE "\n"
#define DBGNEWLINEW L"\n"
#endif

#ifdef _DEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#define ASSERT(_x) assert(_x)
#define TRACE(_fmt, ...) DbgPrintf("TRACE: " _fmt DBGNEWLINE, __VA_ARGS__)
#define TRACEW(_fmt, ...) DbgPrintfW(L"TRACE: " _fmt DBGNEWLINEW, __VA_ARGS__)
#else
#define ASSERT(_x)
#define TRACE(_fmt, ...)
#define TRACEW(_fmt, ...)
#endif



#ifdef _DEBUG

#ifdef _WIN32
#define WINAPI __stdcall
extern "C" __declspec(dllimport) void WINAPI OutputDebugStringA(const char *lpOutputString);
extern "C" __declspec(dllimport) void WINAPI OutputDebugStringW(const wchar_t *lpOutputString);
#endif

static void DbgPrintf(const char *fmt, ...)
{
	va_list vargs;
	char buff[2048];
	va_start(vargs, fmt);
	vsnprintf_s(buff, sizeof(buff), _TRUNCATE, fmt, vargs);
	va_end(vargs);

#ifdef _WIN32
	OutputDebugStringA(buff);
#else
	fprintf(stderr, buff);
#endif
}

static void DbgPrintfW(wchar_t *fmt, ...)
{
	va_list vargs;
	wchar_t buff[2048];
	va_start(vargs, fmt);
	vswprintf_s(buff, sizeof(buff)/sizeof(buff[0]), fmt, vargs);
	va_end(vargs);

#ifdef _WIN32
	OutputDebugStringW(buff);
#else
	fwprintf(stderr, buff);
#endif
}

#endif

#endif // _DBGUTIL_H_
