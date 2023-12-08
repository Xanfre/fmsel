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

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#define TRUE 1
#define FALSE 0
#define _utime utime
#define _utimbuf utimbuf
#define sprintf_s snprintf
#define strcat_s(a,b,c) strncat(a,c,b-strlen(a)-1)
#define strcpy_s(a,b,c) memset(a,0,b); strncpy(a,c,b-1);
#define MAX_PATH PATH_MAX
#define PREF_PROG "xdg-open"
#endif
#include <sys/stat.h>
#include <FL/Fl.H>
#ifdef _WIN32
#define FL_INTERNALS
#include <FL/x.H>
#include <process.h>
#include <sys/utime.h>
#else
#include <pthread.h>
#include <utime.h>
#include <dlfcn.h>
#endif
#include <FL/Fl_File_Chooser.H>
#include "lang.h"


#ifdef _WIN32

static HMODULE g_hDllHandle = NULL;

BOOL WINAPI DllMain(HANDLE hInstance, ULONG reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
		g_hDllHandle = (HMODULE)hInstance;

	return TRUE;
}

#endif


#ifdef _WIN32
void WndInitOS(Fl_Window *pWnd)
{
	// make sure window isn't off-screen
	POINT pt;
	pt.x = pWnd->x();
	pt.y = pWnd->y();
	HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
	if (!hMon)
	{
		// off-screen, center on desktop
		pWnd->position(
			Fl::x() + ((Fl::w() - pWnd->w()) / 2),
			Fl::y() + ((Fl::h() - pWnd->h()) / 2)
			);
	}
}
#else
void WndInitOS(Fl_Window *) { }
#endif

#ifdef _WIN32
void MainWndInitOS(Fl_Window *pMainWnd)
{
	// set window icon to that of the dark exe
	pMainWnd->icon( (const void *)LoadImageA(fl_display, "APPICON", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE/*|LR_SHARED*/) );

	WndInitOS(pMainWnd);
}
#else
void MainWndInitOS(Fl_Window *) { }
#endif

void WaitOS(int ms)
{
#ifdef _WIN32
	Sleep(ms);
#else
	usleep(ms*1000);
#endif
}

#ifdef _WIN32
void MainWndTermOS(Fl_Window *pMainWnd)
{
	if ( pMainWnd->icon() )
	{
		HICON hIcon = (HICON)pMainWnd->icon();
		pMainWnd->icon((const void *)NULL);
		DestroyIcon(hIcon);
	}
}
#else
void MainWndTermOS(Fl_Window *) { }
#endif

void MainWndRestoreOS(Fl_Window *pMainWnd)
{
#ifdef _WIN32
	SendMessage(fl_xid(pMainWnd), WM_SYSCOMMAND, SC_RESTORE, 0);
#else
	pMainWnd->show();
#endif
}

#ifdef _WIN32
void MainWndMaximizeOS(Fl_Window *pMainWnd)
{
	SendMessage(fl_xid(pMainWnd), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
}
#else
void MainWndMaximizeOS(Fl_Window *) { }
#endif

#ifdef _WIN32
BOOL MainWndIsMaximizedOS(Fl_Window *pMainWnd)
{
	return IsZoomed( fl_xid(pMainWnd) );
}
#else
BOOL MainWndIsMaximizedOS(Fl_Window *)
{
	return FALSE;
}
#endif

void FltkTermOS()
{
}

unsigned int GetProcessIdOS()
{
#ifdef _WIN32
	return GetProcessId( GetCurrentProcess() );
#else
	return 0;
#endif
}

#ifdef _WIN32
void WaitForProcessExitOS(unsigned int pid, int waitms)
{
	// wait for process to exit, at most 'waitms' but at least 250ms
	const int MINWAIT = 250;
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (h)
	{
		DWORD dw;
		int maxloops = ((waitms|1)+MINWAIT-1) / MINWAIT;
		do
		{
			Sleep(MINWAIT);
			if (--maxloops <= 0)
				break;
		}
		while (GetExitCodeProcess(h, &dw) == STILL_ACTIVE);
		CloseHandle(h);
	}
	else
		WaitOS(MINWAIT);
}
#else
void WaitForProcessExitOS(unsigned int, int waitms)
{
	WaitOS(waitms);
}
#endif

BOOL OpenFileWithAssociatedApp(const char *filename, const char *dirname)
{
#ifdef _WIN32
	int res = (int) ShellExecuteA(NULL, "open", filename, NULL, dirname, SW_SHOWNORMAL);
	return res > 32;
#else
	pid_t pid = fork();
	if (pid < 0)
		return FALSE;
	else if (0 == pid)
	{
		setsid();
		pid = fork();
		// exit unless this is the child
		if (0 != pid)
			exit(pid > 0 ? 0 : 1);
		if ((NULL != dirname && -1 == chdir(dirname))
			|| -1 == execlp(PREF_PROG, PREF_PROG, filename, NULL))
			exit(1);
	}
	int status;
	return -1 != waitpid(pid, &status, 0) && WIFEXITED(status) && 0 == WEXITSTATUS(status);
#endif
}

BOOL OpenUrlWithAssociatedApp(const char *url, const char *custombrowser)
{
	if (!custombrowser || !*custombrowser)
		return OpenFileWithAssociatedApp(url, NULL);

#ifdef _WIN32
	int res = (int) ShellExecuteA(NULL, "open", custombrowser, url, NULL, SW_SHOWNORMAL);
	return res > 32;
#else
	pid_t pid = fork();
	if (pid < 0)
		return FALSE;
	else if (0 == pid)
	{
		setsid();
		pid = fork();
		// exit unless this is the child
		if (0 != pid)
			exit(pid > 0 ? 0 : 1);
		if (-1 == execlp(custombrowser, custombrowser, url, NULL))
			exit(1);
	}
	int status;
	return -1 != waitpid(pid, &status, 0) && WIFEXITED(status) && 0 == WEXITSTATUS(status);
#endif
}

#ifdef _WIN32
BOOL FileDialog(Fl_Window *parent, BOOL bSave, const char *title, const char **pattern, const char *defext, const char *initial, char *result, int len, BOOL bOpenNoExist)
{
	wchar_t filter[1024];
	char initial_[MAX_PATH];
	wchar_t title_w[256];
	wchar_t defext_w[32];
	wchar_t *result_w;

	if (title)
		fl_utf8towc(title, strlen(title), title_w, sizeof(title_w)/sizeof(title_w[0]));
	if (defext)
		fl_utf8towc(defext, strlen(defext), defext_w, sizeof(defext_w)/sizeof(defext_w[0]));

	// 'initial' and 'result' may be the same buffer
	strcpy_s(initial_, sizeof(initial_), initial);

	result[0] = 0;

	if (!pattern || !*pattern)
	{
		const char *s = $("All Files");
		fl_utf8towc(s, strlen(s), filter, sizeof(filter)/sizeof(filter[0]));
		memcpy(filter+wcslen(filter), L"\0*.*\0\0", 6*sizeof(wchar_t));
	}
	else
	{
		filter[0] = 0;

		while (*pattern)
		{
			char s[128];
			wchar_t ws[128];
			sprintf_s(s, sizeof(s), "%s (%s)\t%s\t", pattern[0], pattern[1], pattern[1]);
			fl_utf8towc(s, strlen(s), ws, sizeof(ws)/sizeof(ws[0]));
			wcscat_s(filter, sizeof(filter)/sizeof(filter[0]), ws);
			pattern += 2;
		}

		wcscat_s(filter, sizeof(filter)/sizeof(filter[0]), L"\t");
		wchar_t *s = filter;
		for (wchar_t *s=filter; *s; s++)
			if (*s == L'\t')
				*s = 0;
	}

	char cwd[MAX_PATH];
	_getcwd(cwd, sizeof(cwd));

#ifdef CUSTOM_FLTK
	fl_filename_absolute_ex(result, len, initial_, TRUE);
#else
	fl_filename_absolute(result, len, initial_);
#endif

	char *s = result;
	for (; *s; s++)
		if (*s == '/')
			*s = '\\';

	result_w = new wchar_t[len];
	fl_utf8towc(result, strlen(result), result_w, len);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrTitle = title ? title_w : NULL;
	ofn.hwndOwner = parent ? Fl_X::i(parent)->xid : NULL;
	ofn.hInstance = fl_display;
	ofn.lpstrFilter = filter;
	ofn.lpstrDefExt = defext ? defext_w : NULL;
	ofn.nFilterIndex = 0;
	ofn.lpstrInitialDir = result_w;
	ofn.lpstrFile = result_w;
	ofn.nMaxFile = len;
	ofn.Flags = OFN_NOCHANGEDIR | OFN_LONGNAMES | OFN_DONTADDTORECENT;
	if (bSave)
		ofn.Flags |= OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	else
		ofn.Flags |= (bOpenNoExist ? 0 : OFN_FILEMUSTEXIST) | OFN_PATHMUSTEXIST;

	BOOL res = bSave ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);

	if (res)
	{
		int n = WideCharToMultiByte(CP_ACP, 0, result_w, wcslen(result_w), result, len-1, NULL, NULL);
		result[n] = 0;

		if (!n)
			res = FALSE;
	}

	delete[] result_w;

	// OFN_NOCHANGEDIR has no effect with GetOpenFileName on win2k+ so let's make really sure no one messes with cwd
	_chdir(cwd);

	if (!res)
		return FALSE;

	return TRUE;
}
#else
BOOL FileDialog(Fl_Window *, BOOL bSave, const char *title, const char **pattern, const char *, const char *initial, char *result, int len, BOOL bOpenNoExist)
{
	// requires png lib, don't want to bloat the dll only for this dialog
	/*static int init = 0;
	if (!init)
	{
		Fl_File_Icon::load_system_icons();
		init = 1;
	}*/

	char filter[1024];
	char initial_[MAX_PATH];

	strcpy_s(initial_, sizeof(initial_), initial);

	if (!pattern || !*pattern)
	{
		strcpy(filter, $("All Files"));
		strcat(filter, " (*)");
	}
	else
	{
		filter[0] = 0;

		int i = 0;
		while (*pattern)
		{
			char s[128];
			sprintf_s(s, sizeof(s), i?"\t%s (%s)":"%s (%s)", pattern[0], strcmp(pattern[1], "*.*") ? pattern[1] : "*");
			strcat_s(filter, sizeof(filter), s);
			pattern += 2;
			i++;
		}
	}

retry:
	int iFontSize = FL_NORMAL_SIZE;
	BOOL bChangeFont = iFontSize > 18;
	if (bChangeFont)
		FL_NORMAL_SIZE = 18;
	const char *s = fl_file_chooser(title, filter, initial_);
	if (bChangeFont)
		FL_NORMAL_SIZE = iFontSize;
	if (!s)
		return FALSE;

	if (!bSave && !bOpenNoExist)
	{
		// make sure file exists, otherwise return to dialog (emulate win32 open file dialog behavior)
		struct stat st = {};
		if (stat(s, &st) || (st.st_mode & S_IFDIR))
		{
			fl_alert($("%s\nFile not found.\nCheck the filename and try again."), s);
			strcpy_s(initial_, sizeof(initial_), s);
			goto retry;
		}
	}

	strcpy_s(result, len, s);

	return TRUE;
}
#endif

BOOL GetFreeDiskSpaceOS(const char *path, unsigned __int64 &freeMB)
{
#ifdef WIN32
	ULARGE_INTEGER avail, tot, totavail;
	if ( !GetDiskFreeSpaceExA(path, &avail, &tot, &totavail) )
		return FALSE;
	freeMB = avail.QuadPart / (ULONGLONG)(1024*1024);
#else
	struct statvfs st = {};
	if (-1 == statvfs(path, &st))
		return FALSE;
	freeMB = (st.f_bsize*st.f_bavail) / (1024*1024);
#endif
	return TRUE;
}

BOOL CreateThreadOS(void* (*f)(void*), void *p)
{
#ifdef _WIN32
	return _beginthread((void(__cdecl*)(void*))f, 0, p) != 0;
#else
	pthread_t t;
	return pthread_create(&t, 0, f, p) == 0;
#endif
}

BOOL GetFileMTimeOS(const char *fname, time_t &tm)
{
#ifdef _WIN32
	HANDLE hFile = CreateFileA(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		FILETIME ftWrite;
		const BOOL bRes = GetFileTime(hFile, NULL, NULL, &ftWrite);
		CloseHandle(hFile);
		if (bRes)
		{
			ULARGE_INTEGER ul;
			ul.LowPart = ftWrite.dwLowDateTime;
			ul.HighPart = ftWrite.dwHighDateTime;
			tm = (ul.QuadPart / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
			return TRUE;
		}
	}
	return FALSE;
#else
	struct stat st = {};
	if ( !stat(fname, &st) )
	{
		tm = st.st_mtime;
		return TRUE;
	}
	return FALSE;
#endif
}

BOOL GetFileSizeAndMTimeOS(const char *fname, unsigned __int64 &sz, time_t &tm)
{
#ifdef _WIN32
	HANDLE hFile = CreateFileA(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		FILETIME ftWrite;
		const BOOL bRes = GetFileTime(hFile, NULL, NULL, &ftWrite) && GetFileSizeEx(hFile, (PLARGE_INTEGER)&sz);
		CloseHandle(hFile);
		if (bRes)
		{
			ULARGE_INTEGER ul;
			ul.LowPart = ftWrite.dwLowDateTime;
			ul.HighPart = ftWrite.dwHighDateTime;
			tm = (ul.QuadPart / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
			return TRUE;
		}
	}
	return FALSE;
#else
	struct stat st = {};
	if ( !stat(fname, &st) )
	{
		sz = st.st_size;
		tm = st.st_mtime;
		return TRUE;
	}
	return FALSE;
#endif
}

BOOL CloneFileMTimeOS(const char *srcfile, const char *dstfile)
{
#ifdef _WIN32
	HANDLE hFile = CreateFileA(srcfile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		FILETIME ftWrite;
		BOOL bRes = GetFileTime(hFile, NULL, NULL, &ftWrite);
		CloseHandle(hFile);
		if (bRes)
		{
			hFile = CreateFileA(dstfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				bRes = SetFileTime(hFile, NULL, NULL, &ftWrite);
				CloseHandle(hFile);
				return bRes;
			}
		}
	}
	return FALSE;
#else
	struct stat st = {};
	if ( !stat(srcfile, &st) )
	{
		struct _utimbuf ut;
		ut.actime = st.st_mtime;
		ut.modtime = st.st_mtime;

		return !_utime(dstfile, &ut);
	}
	return FALSE;
#endif
}

void* LoadDynamicLibOS(const char *name)
{
	if (!name || !*name)
		return NULL;

#ifdef _WIN32
	return LoadLibraryA(name);
#else
	return dlopen(name, RTLD_NOW);
#endif
}

void CloseDynamicLibOS(void *handle)
{
	if (!handle)
		return;

#ifdef _WIN32
	FreeLibrary((HMODULE)handle);
#else
	dlclose(handle);
#endif
}

void* GetDynamicLibProcOS(void *handle, const char *procname)
{
	if (!handle || !procname || !*procname)
		return NULL;

#ifdef _WIN32
	return (void*)GetProcAddress((HMODULE)handle, procname);
#else
	return dlsym(handle, procname);
#endif
}

BOOL GetFmselModulePath(char *buf, int len)
{
#ifdef _WIN32
	ASSERT(g_hDllHandle != NULL);

	if (!g_hDllHandle)
		return FALSE;

	if ( !GetModuleFileNameA(g_hDllHandle, buf, len) )
	{
		ASSERT(FALSE);
		return FALSE;
	}
#else
	Dl_info info;

	if ( !dladdr((void*)&InitLocalization, &info) )
	{
		ASSERT(FALSE);
		return FALSE;
	}

	buf[len-1] = 0;

	strncpy(buf, info.dli_fname, len);

	if (buf[len-1] != 0)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	char *sep = strrchr(buf, '/');
	if (!sep)
		sep = buf;
	char *ext = strstr(sep, ".so.");
	if (ext)
		ext[3] = '\0';
#endif

	return TRUE;
}

// recursive mkdir adapted from http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html
BOOL mkdir_parents(const char *dir)
{
	char tmp[MAX_PATH] = {0};
	char *p = NULL;
	size_t len;

	strncpy(tmp, dir, sizeof(tmp)-1);
	len = strlen(tmp);

#ifdef _WIN32
	if (tmp[len - 1] == '\\')
		tmp[len - 1] = 0;

	// search starts past the drive letter
	for (p = tmp + 3; *p; p++)
	{
		if (*p == '\\')
		{
			*p = 0;
			_mkdir(tmp);
			*p = '\\';
		}
	}

	return _mkdir(tmp);
#else
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for (p = tmp + 1; *p; p++)
	{
		if (*p == '/')
		{
			*p = 0;
			mkdir(tmp, 0755);
			*p = '/';
		}
	}

	return mkdir(tmp, 0755);
#endif
}
