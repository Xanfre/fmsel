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

#ifndef _OS_H_
#define _OS_H_

#include <time.h>


class Fl_Window;

#ifndef BOOL
typedef int BOOL;
#endif


void MainWndInitOS(Fl_Window *pMainWnd);
void MainWndTermOS(Fl_Window *pMainWnd);
void WndInitOS(Fl_Window *pWnd);
void FltkTermOS();
unsigned int GetProcessIdOS();
void WaitOS(int ms);
void WaitForProcessExitOS(unsigned int pid, int waitms);
BOOL OpenFileWithAssociatedApp(const char *filename, const char *dirname);
BOOL OpenUrlWithAssociatedApp(const char *url, const char *custombrowser = 0);
BOOL FileDialog(Fl_Window *parent, BOOL bSave, const char *title, const char **pattern, const char *defext, const char *initial, char *result, int len, BOOL bOpenNoExist = 0);
BOOL GetFreeDiskSpaceOS(const char *path, unsigned __int64 &freeMB);
BOOL CreateThreadOS(void* (*f)(void*), void *p);
BOOL GetFileMTimeOS(const char *fname, time_t &tm);
BOOL GetFileSizeAndMTimeOS(const char *fname, unsigned __int64 &sz, time_t &tm);
BOOL CloneFileMTimeOS(const char *srcfile, const char *dstfile);

void* LoadDynamicLibOS(const char *name);
void CloseDynamicLibOS(void *handle);
void* GetDynamicLibProcOS(void *handle, const char *procname);

BOOL GetFmselModulePath(char *buf, int len);

#endif // _OS_H_
