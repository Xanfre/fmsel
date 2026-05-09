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

#include "archive.h"
#include "os.h"
#include "lang.h"
#if !defined(_WIN32) && defined(__int64)
#undef __int64
#endif
#include <lib7zip.h>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/fl_utf8.h>
#include <stdarg.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <zip.h>
#include <direct.h>
#include <io.h>
#include <iowin32.h>
#include <sys/utime.h>
#else
#include <minizip/zip.h>
#include <unistd.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#define TRUE 1
#define FALSE 0
#define MAX_PATH PATH_MAX
#define max(a,b) (((a)>(b))?(a):(b))
#endif

#include "dbgutil.h"


// NOTE: lib7zip unfortunately doesn't have compression support, so we use a slimmed down zlib (only stuff needed to compress)


#ifndef _WIN32
#define INVALID_HANDLE_VALUE NULL
#endif


#define ERR_7ZINIT()	if (ppErrMsg) *ppErrMsg = $("failed to init 7z")
#define ERR_OPENARCH()	if (ppErrMsg) *ppErrMsg = $("failed to open archive")
#define ERR_NOFILE()	if (ppErrMsg) *ppErrMsg = $("file not found")
#define ERR_FCREATE()	if (ppErrMsg) *ppErrMsg = $("failed to create file, check write access")
#define ERR_WRITE()		if (ppErrMsg) *ppErrMsg = $("write error, disk possibly full")


void ShowBusyCursor(BOOL bShow);

void InitProgress(int nSteps, const char *label);
void TermProgress();
int RunProgress();
void StepProgress(int nSteps);
void EndProgress(int result);

static BOOL CreateAllSubDirs(char *filepath, int subdir_start, int subdir_end);


static C7ZipLibrary *g_p7zLib = NULL;
static int g_bFailed7z = 0;


/////////////////////////////////////////////////////////////////////
// LOCAL TYPES/FUNCTIONS

struct BusyCursor
{
public:
	BusyCursor() { ShowBusyCursor(TRUE); }
	~BusyCursor() { ShowBusyCursor(FALSE); }
};

#define BUSY_CURSOR() BusyCursor __busycursor

//

class ArchiveInStream : public C7ZipInStream
{
protected:
#ifdef _WIN32
	HANDLE m_pFile;
#else
	FILE *m_pFile;
#endif
	std::string m_strFileName;
	std::string m_strFileExt;
	int m_nFileSize;
	time_t m_tmFiletime;

protected:
	const char* ProbeArchiveType()
	{
		char signature[4];

		Read(signature, 4, NULL);
		Seek(0, SEEK_SET, NULL);

		if (!strncmp(signature, "PK", 2) && signature[2] == 0x03 && signature[3] == 0x04)
			return "zip";
		else if ( !strncmp(signature, "Rar!", 4) )
			return "rar";
		else
			return "7z";
	}

public:
	ArchiveInStream(const char *fileName)
		: m_strFileName(fileName),
		m_strFileExt("7z")
	{
#ifdef _WIN32
		m_pFile = CreateFileW(WidenStrOS(fileName).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
#else
		if ( !GetFileMTimeOS(fileName, m_tmFiletime) )
			m_tmFiletime = 0;

		m_pFile = fl_fopen(fileName, "rb");
#endif
		if (m_pFile != INVALID_HANDLE_VALUE)
		{
#ifdef _WIN32
			GetFileSize(m_pFile, (LPDWORD)&m_nFileSize);
			if ( !GetFileTime(m_pFile, NULL, NULL, (LPFILETIME)&m_tmFiletime) )
				m_tmFiletime = 0;
#else
			m_nFileSize = GetFILESizeOS(m_pFile);
#endif

			const std::string::size_type pos = m_strFileName.find_last_of(".");
			if (pos != m_strFileName.npos)
			{
				const char *ext = m_strFileName.c_str() + pos + 1;

				if ( !fl_utf_strcasecmp(ext, "ss2mod") )
					ext = ProbeArchiveType();

				m_strFileExt = ext;
			}
		}
	}

	virtual ~ArchiveInStream()
	{
		if (m_pFile != INVALID_HANDLE_VALUE)
#ifdef _WIN32
			CloseHandle(m_pFile);
#else
			fclose(m_pFile);
#endif
	}

	time_t GetMTime() const { return m_tmFiletime; }

	virtual wstring GetExt() const { return WidenStrOS(m_strFileExt.c_str()); }

	virtual int Read(void *data, unsigned int size, unsigned int *processedSize)
	{
		if (m_pFile == INVALID_HANDLE_VALUE)
			return 1;

#ifdef _WIN32
		DWORD count;
		if ( !ReadFile(m_pFile, data, size, &count, NULL) )
			return 1;
#else
		int count = fread(data, 1, size, m_pFile);
		if (ferror(m_pFile))
			return 1;
#endif

		if (processedSize != NULL)
			*processedSize = count;

		return 0;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition)
	{
		if (m_pFile == INVALID_HANDLE_VALUE)
			return 1;

#ifdef _WIN32
		return !SetFilePointerEx(m_pFile, (LARGE_INTEGER&)offset, (PLARGE_INTEGER)newPosition, seekOrigin);
#else
		int result = fseek(m_pFile, (long)offset, seekOrigin);
		if (!result)
		{
			if (newPosition)
				*newPosition = ftell(m_pFile);

			return 0;
		}

		return result;
#endif
	}

	virtual int GetSize(unsigned __int64 *size)
	{
		if (size)
			*size = m_nFileSize;
		return 0;
	}
};

//

class FileOutStream : public C7ZipOutStream
{
protected:
#ifdef _WIN32
	HANDLE m_pFile;
#else
	FILE *m_pFile;
#endif
	int m_nFileSize;

	time_t m_tmFiletime;
	BOOL m_bWriteError;

protected:
	void InitFileTime(C7ZipArchiveItem *pItem, time_t tmFiletimeFallback)
	{
		unsigned __int64 val = 0;

		if (pItem->GetFileTimeProperty(lib7zip::kpidMTime, val)
			|| pItem->GetFileTimeProperty(lib7zip::kpidCTime, val))
#ifndef _WIN32
			// convert FILETIME to time_t
			m_tmFiletime = (val / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
#else
			m_tmFiletime = val;
#endif
		else
			m_tmFiletime = tmFiletimeFallback;
	}

public:
	FileOutStream(const char *fileName, C7ZipArchiveItem *pItem, time_t tmFiletimeFallback = 0)
	{
#ifdef _WIN32
		m_pFile = CreateFileW(WidenStrOS(fileName).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
#else
		m_pFile = fl_fopen(fileName, "wb");
#endif
		m_nFileSize = 0;
		m_bWriteError = FALSE;
		InitFileTime(pItem, tmFiletimeFallback);
	}

	// special constructor that creates all dirs in 'itempath' is necessary (param constellation optimized
	// to minimize overhead when extracting full archives with thousands of files)
	FileOutStream(const std::string &dest, const std::string &itempath, std::string &tmp, C7ZipArchiveItem *pItem, time_t tmFiletimeFallback = 0)
	{
		// 'wdest' is slash-terminated

		m_bWriteError = FALSE;

		tmp = dest + itempath;

#ifdef _WIN32
		m_pFile = CreateFileW(WidenStrOS(tmp.c_str()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
#else
		m_pFile = fl_fopen(tmp.c_str(), "wb");
#endif
		m_nFileSize = 0;

#ifdef _WIN32
		if (m_pFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND)
#else
		if (m_pFile == INVALID_HANDLE_VALUE && errno == ENOENT)
#endif
		{
			// failed to open file due to non-existing path, create path and try again
			const std::string::size_type pos = itempath.find_last_of(DIRSEP_STR);
			if (pos != itempath.npos
				&& CreateAllSubDirs((char*)tmp.c_str(), dest.length(), dest.length()+pos))
#ifdef _WIN32
				m_pFile = CreateFileW(WidenStrOS(tmp.c_str()).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
#else
				m_pFile = fl_fopen(tmp.c_str(), "wb");
#endif
		}

		InitFileTime(pItem, tmFiletimeFallback);
	}

	bool IsValid() const { return m_pFile != INVALID_HANDLE_VALUE; }

	BOOL WriteError() const { return m_bWriteError; }

	virtual ~FileOutStream()
	{
		if (m_pFile != INVALID_HANDLE_VALUE)
		{
			// update file time if available
			if (m_tmFiletime != 0)
			{
#ifdef _WIN32
				SetFileTime(m_pFile, NULL, NULL, (LPFILETIME)&m_tmFiletime);
#else
				fflush(m_pFile);

				struct timespec ts[2];
				ts[0].tv_sec = m_tmFiletime;
				ts[0].tv_nsec = 0;
				ts[1].tv_sec = m_tmFiletime;
				ts[1].tv_nsec = 0;

				futimens(fileno(m_pFile), ts);
#endif
			}

#ifdef _WIN32
			CloseHandle(m_pFile);
#else
			fclose(m_pFile);
#endif
		}
	}

	int GetFileSize() const { return m_nFileSize; }

	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize)
	{
#ifdef _WIN32
		DWORD count;
		if ( !WriteFile(m_pFile, data, size, &count, NULL) )
		{
			m_bWriteError = TRUE;
			return 1;
		}
#else
		int count = fwrite(data, 1, size, m_pFile);
		if (ferror(m_pFile))
		{
			m_bWriteError = TRUE;
			return 1;
		}
#endif

		if ((unsigned int)count < size)
			m_bWriteError = TRUE;

		if (processedSize != NULL)
			*processedSize = count;

		m_nFileSize += count;
		return 0;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition)
	{
#ifdef _WIN32
		return !SetFilePointerEx(m_pFile, (LARGE_INTEGER&)offset, (PLARGE_INTEGER)newPosition, seekOrigin);
#else
		int result = fseek(m_pFile, (long)offset, seekOrigin);
		if (!result)
		{
			if (newPosition)
				*newPosition = ftell(m_pFile);

			return 0;
		}

		return result;
#endif
	}

	virtual int SetSize(unsigned __int64) { return 0; }
};

//

class MemOutStream : public C7ZipOutStream
{
protected:
	char *m_pData;
	int m_nMaxSize;
	int m_nSize;
	__int64 m_nPos;

public:
	MemOutStream(char *data, int len)
	{
		m_pData = data;
		m_nMaxSize = len;
		m_nSize = 0;
		m_nPos = 0;
	}

	virtual ~MemOutStream()
	{
	}

	int GetFileSize() const { return m_nSize; }

	virtual int Write(const void *data, unsigned int size, unsigned int *processedSize)
	{
		memcpy(m_pData+m_nPos, data, size);
		m_nPos += size;
		m_nSize += size;
		if (processedSize != NULL)
			*processedSize = size;

		return 0;
	}

	virtual int Seek(__int64 offset, unsigned int seekOrigin, unsigned __int64 *newPosition)
	{
		switch (seekOrigin)
		{
		case FILE_BEGIN: m_nPos = offset; break;
		case FILE_CURRENT: m_nPos += offset; break;
		case FILE_END: m_nPos = m_nSize-offset; break;
		}

		if (newPosition)
			*newPosition = m_nPos;

		return 0;
	}

	virtual int SetSize(unsigned __int64) { return 0; }
};

//

class NullOutStream : public C7ZipOutStream
{
protected:
	int m_nFileSize;

public:
	NullOutStream()
	{
		m_nFileSize = 0;
	}
	NullOutStream(const std::string &, const std::string &, std::string &)
	{
		m_nFileSize = 0;
	}

	bool IsValid() const { return TRUE; }
	int GetFileSize() const { return m_nFileSize; }

	virtual int Write(const void *, unsigned int size, unsigned int *processedSize)
	{
		if (processedSize != NULL)
			*processedSize = size;
		m_nFileSize += size;
		return 0;
	}

	virtual int Seek(__int64, unsigned int, unsigned __int64 *newPosition)
	{
		if (newPosition)
			*newPosition = 0;
		return 0;
	}

	virtual int SetSize(unsigned __int64) { return 0; }
};

//

class FileOutStreamFactory : public C7ZipOutStreamFactory
{
protected:
	C7ZipArchive *m_pArchive;
	BOOL m_bFailed;
	const std::string &m_dest;
	std::string &m_tmp;

	time_t m_tmFiletimeFallback;

	NullOutStream m_null;

	BOOL m_bWriteError;

public:
	FileOutStreamFactory(C7ZipArchive *pArchive, const std::string &dest, std::string &tmp, time_t arch_mtime)
		: m_pArchive(pArchive),
		m_bFailed(FALSE),
		m_dest(dest),
		m_tmp(tmp),
		m_tmFiletimeFallback(arch_mtime),
		m_bWriteError(FALSE)
	{
	}

	C7ZipArchive* GetArchive() { return m_pArchive; }

	BOOL Failed() const { return m_bFailed; }

	BOOL WriteError() const { return m_bWriteError; }

	virtual C7ZipOutStream* GetStream(C7ZipArchiveItem * pItem)
	{
		StepProgress(1);

		// we don't handle dirs so return a dummy stream object (it won't actually be used for anything)
		if ( pItem->IsDir() )
			return &m_null;

		// no encyption support, fail
		if ( pItem->IsEncrypted() )
		{
			m_bFailed = TRUE;
			return NULL;
		}

		FileOutStream *pFile = new FileOutStream(m_dest, NarrowStrOS(pItem->GetFullPath().c_str()), m_tmp, pItem, m_tmFiletimeFallback);

		if ( !pFile->IsValid() )
		{
			// failed to open file
			m_bFailed = TRUE;
			delete pFile;
			return NULL;
		}

		return pFile;
	}

	virtual void CloseStream(C7ZipOutStream * pStream)
	{
		if (pStream != &m_null)
		{
			if ( ((FileOutStream*)pStream)->WriteError() )
				m_bWriteError = TRUE;

			delete (FileOutStream*)pStream;
		}
	}
};

//

struct ArchiveContext
{
public:
	std::string name;
	ArchiveInStream stream;
	C7ZipArchive *pArchive;

public:
	ArchiveContext(const char *archive) : stream(archive) { pArchive = NULL; }
	~ArchiveContext() { if (pArchive) { pArchive->Close(); delete pArchive; } }
};

// cache currently open archive so that subsequent accesses are faster
static ArchiveContext *g_pArchive = NULL;

static C7ZipArchive* OpenArchive(const char *archive, time_t *mtime = NULL)
{
	if (g_pArchive && g_pArchive->name == archive)
		return g_pArchive->pArchive;

	if (g_pArchive)
		delete g_pArchive;

	g_pArchive = new ArchiveContext(archive);

	if (!g_p7zLib->OpenArchive(&g_pArchive->stream, &g_pArchive->pArchive) || !g_pArchive->pArchive)
	{
		delete g_pArchive;
		g_pArchive = NULL;
		return NULL;
	}

	g_pArchive->name = archive;
	if (mtime)
		*mtime = g_pArchive->stream.GetMTime();

	return g_pArchive->pArchive;
}

static void CloseArchive(C7ZipArchive *)
{
	// do nothing
}

//

static C7ZipArchiveItem* FindArchiveItem(C7ZipArchive *pArchive, const char *fname)
{
	unsigned int nItems = 0;

	pArchive->GetItemCount(&nItems);

	if (!nItems)
		return NULL;

	std::string itempath;

	for (unsigned int i=0; i<nItems; i++)
	{
		C7ZipArchiveItem *pArchiveItem = NULL;

		if ( !pArchive->GetItemInfo(i, &pArchiveItem) )
			continue;

		if (pArchiveItem->IsDir() || pArchiveItem->IsEncrypted())
			continue;

		itempath = NarrowStrOS(pArchiveItem->GetFullPath().c_str());

		// NOTE: itempath is assumed to use OS specific dir separators, that's what GetFullPath() also returns
		if ( !fl_utf_strcasecmp(fname, itempath.c_str()) )
			return pArchiveItem;
	}

	return NULL;
}

//

// takes a full pathname to a file, subdir_start is the first character of the subdir portion of which
// dirs should be created, subdir_end is the dir separator right before the filename starts
static BOOL CreateAllSubDirs(char *filepath, int subdir_start, int subdir_end)
{
	subdir_end++;
	const char ch = filepath[subdir_end];
	filepath[subdir_end] = 0;

	char *sub = filepath + subdir_start;
	int ok = 1;

	for (;;)
	{
		char *s = strchr(sub, DIRSEP);
		if (!s)
			break;

		*s = 0;
		ok = !fl_mkdir(filepath, DEF_DIR_MODE) || errno == EEXIST;
		*s = DIRSEP;

		if (!ok)
			break;

		sub = s+1;
	}

	filepath[subdir_end] = ch;

	return ok;
}

//

static void GetExtractErrorString(int err, const char *&pErrMsg)
{
	switch (err)
	{
	case lib7zip::kxerrUnSupportedMethod: pErrMsg = $("unsupported compression type"); break;
	case lib7zip::kxerrDataError: pErrMsg = $("corrupted archive data"); break;
	case lib7zip::kxerrCRCError: pErrMsg = $("checksum error"); break;
	default:
		// check if multiple different errors are set (i.e. more than 1 bit set)
		if (err & (err-1))
			pErrMsg = $("multiple errors");
		else
			pErrMsg = $("unknown error");
	}
}


/////////////////////////////////////////////////////////////////////
// ZLIB ZIP COMPRESSION UTILS

// filetime() from zlib's minzip.c 1.01e, Copyright (C) 1998-2005 Gilles Vollant
#define MAXFILENAME (MAX_PATH-1)
#ifdef WIN32
uLong filetime(const char *f, tm_zip *tmzip, uLong *dt)
{
  int ret = 0;
  {
      FILETIME ftLocal;
      HANDLE hFind;
      WIN32_FIND_DATAW  ff32;

      hFind = FindFirstFileW(WidenStrOS(f).c_str(),&ff32);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
        FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
        FindClose(hFind);
        ret = 1;
      }
  }
  return ret;
}
#else
uLong filetime(const char *f, tm_zip *tmzip, uLong *)
{
  int ret=0;
  struct stat s;        /* results of stat() */
  struct tm* filedate;
  time_t tm_t=0;

  if (strcmp(f,"-")!=0)
  {
    char name[MAXFILENAME+1];
    int len = strlen(f);
    if (len > MAXFILENAME)
      len = MAXFILENAME;

    strncpy(name, f,MAXFILENAME-1);
    /* strncpy doesnt append the trailing NULL, of the string is too long. */
    name[ MAXFILENAME ] = '\0';

    if (name[len - 1] == '/')
      name[len - 1] = '\0';
    /* not all systems allow stat'ing a file with / appended */
    if (stat(name,&s)==0)
    {
      tm_t = s.st_mtime;
      ret = 1;
    }
  }
  filedate = localtime(&tm_t);

  tmzip->tm_sec  = filedate->tm_sec;
  tmzip->tm_min  = filedate->tm_min;
  tmzip->tm_hour = filedate->tm_hour;
  tmzip->tm_mday = filedate->tm_mday;
  tmzip->tm_mon  = filedate->tm_mon ;
  tmzip->tm_year = filedate->tm_year;

  return ret;
}
#endif


struct ZipOutContext
{
	zipFile zf;
	zlib_filefunc_def ffunc;
	std::string fname;

	int num_files;
};

static ZipOutContext *g_pZipOutContext = NULL;


static bool BeginCreateZipFile(const char *zipfile)
{
	if (g_pZipOutContext)
	{
		ASSERT(0);
		return false;
	}

	g_pZipOutContext = new ZipOutContext;
	g_pZipOutContext->num_files = 0;

#ifdef _WIN32
	fill_win32_filefunc(&g_pZipOutContext->ffunc);
#else
	fill_fopen_filefunc(&g_pZipOutContext->ffunc);
#endif
	g_pZipOutContext->zf = zipOpen2(zipfile, 0, NULL, &g_pZipOutContext->ffunc);

	if (!g_pZipOutContext->zf)
	{
		TRACE("failed to create zip %s", zipfile);
		delete g_pZipOutContext;
		g_pZipOutContext = NULL;
		return false;
	}

	g_pZipOutContext->fname = zipfile;

	return true;
}

static bool EndCreateZipFile(bool bAbort)
{
	if (!g_pZipOutContext)
	{
		ASSERT(FALSE);
		return false;
	}

	const int errclose = zipClose(g_pZipOutContext->zf, NULL);

	if (errclose != ZIP_OK || bAbort)
	{
		// failed, delete faulty archive
		fl_unlink( g_pZipOutContext->fname.c_str() );
		delete g_pZipOutContext;
		g_pZipOutContext = NULL;
		return false;
	}

	// if we produced an empty archive then remove the file
	if (g_pZipOutContext->num_files == 0)
		fl_unlink( g_pZipOutContext->fname.c_str() );

	delete g_pZipOutContext;
	g_pZipOutContext = NULL;

	return true;
}

static bool AddFileToZip(const char *fname, const char *zipfname)
{
	ASSERT(g_pZipOutContext != NULL);

	FILE *f = fl_fopen(fname, "rb");
	if (!f)
	{
		ASSERT(FALSE);
		return false;
	}

	const int compress_level = Z_DEFAULT_COMPRESSION;
	zip_fileinfo zi = {};

	filetime(fname, &zi.tmz_date, &zi.dosDate);

#ifdef _WIN32
	int err = zipOpenNewFileInZip3(g_pZipOutContext->zf, DemoteStrOS(zipfname).c_str(), &zi, NULL, 0, NULL, 0, NULL,
									Z_DEFLATED, compress_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, NULL, 0);
#else
	int err = zipOpenNewFileInZip3(g_pZipOutContext->zf, zipfname, &zi, NULL, 0, NULL, 0, NULL,
									Z_DEFLATED, compress_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, NULL, 0);
#endif
	if (err != ZIP_OK)
	{
		fclose(f);
		return false;
	}

	const int RBUF_SIZE = 128*1024;
	char *buf = new char[RBUF_SIZE];
	size_t size_read;

	BOOL bEOF = FALSE;
	do
	{
		size_read = fread(buf, 1, RBUF_SIZE, f);
		if (size_read < RBUF_SIZE)
		{
			if ( !feof(f) )
			{
				// read error
				err = ZIP_ERRNO;
				ASSERT(FALSE);
				break;
			}
			else if (!size_read)
				break;

			bEOF = TRUE;
		}

		err = zipWriteInFileInZip(g_pZipOutContext->zf, buf, size_read);
		if (err != ZIP_OK)
		{
			ASSERT(FALSE);
			break;
		}
	}
	while (!bEOF);

	delete[] buf;

	fclose(f);

	if (zipCloseFileInZip(g_pZipOutContext->zf) == ZIP_OK && err == ZIP_OK)
	{
		g_pZipOutContext->num_files++;
		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////
// INIT/TERM

static bool InitArchiveLib(BOOL bSilent = FALSE)
{
	if (!g_p7zLib && !g_bFailed7z)
	{
		BUSY_CURSOR();

		g_p7zLib = new C7ZipLibrary;

		if ( !g_p7zLib->Initialize() )
		{
			delete g_p7zLib;
			g_p7zLib = NULL;
			g_bFailed7z = 1;
			if (!bSilent)
				fl_alert($("Failed to initialize 7-zip library, archive support will be disabled."));
			return false;
		}

		/*WStringArray extensions;
		if ( !g_p7zLib->GetSupportedExts(extensions) )
		{
			delete g_p7zLib;
			g_p7zLib = NULL;
			g_bFailed7z = 1;
			if (!bSilent)
				fl_alert($("Failed to initialize 7-zip library, archive support will be disabled."));
			return false;
		}

		// TODO: check that minimum requirements are supported (unpack zip, rar, 7z and pack zip)
		int size = (int) extensions.size();
		for(int i=0; i<size; i++)
		{
			const wstring &ext = extensions[i];
		}*/
	}

	return g_p7zLib != NULL;
}

void TermArchiveSystem()
{
	ASSERT(!g_pZipOutContext);

	if (g_pArchive)
	{
		delete g_pArchive;
		g_pArchive = NULL;
	}

	if (g_p7zLib)
	{
		g_p7zLib->Deinitialize();
		delete g_p7zLib;
		g_p7zLib = NULL;
	}
}


/////////////////////////////////////////////////////////////////////
// ARCHIVE API

bool IsArchiveFormatSupported(const char *ext)
{
	// TODO: could actually check which formats 7z enumerated
	return !fl_utf_strcasecmp(ext, "zip") || !fl_utf_strcasecmp(ext, "7z") || !fl_utf_strcasecmp(ext, "rar")
		|| !fl_utf_strcasecmp(ext, "ss2mod");
}

bool GetUnpackedArchiveSize(const char *archive, unsigned __int64 &sz, unsigned int &numfiles, bool nocache)
{
	if ( !InitArchiveLib() )
		return false;

	BUSY_CURSOR();

	ArchiveContext *pCachedArchive = g_pArchive;
	if (nocache)
		g_pArchive = NULL;

	C7ZipArchive *pArchive = OpenArchive(archive);
	if (!pArchive)
	{
		if (nocache)
			g_pArchive = pCachedArchive;
		return false;
	}

	sz = 0;
	numfiles = 0;

	unsigned int nItems = 0;
	bool ret = true;

	if ( !pArchive->GetItemCount(&nItems) )
		ret = false;
	else
	{
		C7ZipArchiveItem *pArchiveItem;
		unsigned __int64 n;

		for (unsigned int i=0; i<nItems; i++)
			if (pArchive->GetItemInfo(i, &pArchiveItem)
				&& pArchiveItem->GetUInt64Property(lib7zip::kpidSize, n))
			{
				sz += n;
				numfiles++;
			}
	}

	CloseArchive(pArchive);

	if (nocache)
	{
		delete g_pArchive;
		g_pArchive = pCachedArchive;
	}

	return ret;
}

int ListFilesInArchiveRoot(const char *archive, std::vector<std::string> &list, std::vector<time_t> *timestamps)
#ifdef T3_SUPPORT
{
	return ListFilesInArchivePruned(archive, 0, list, timestamps);
}

int ListFilesInArchivePruned(const char *archive, unsigned int maxdepth, std::vector<std::string> &list, std::vector<time_t> *timestamps)
#endif
{
	if ( !InitArchiveLib() )
		return -2;

	BUSY_CURSOR();

	time_t arch_ftime;

	C7ZipArchive *pArchive = OpenArchive(archive, &arch_ftime);
	if (!pArchive)
		return -1;

	unsigned int nItems = 0;
	pArchive->GetItemCount(&nItems);

	if (!nItems)
	{
		CloseArchive(pArchive);
		return 0;
	}

#ifdef _WIN32
	// convert FILETIME to time_t
	arch_ftime = (arch_ftime / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
#endif

	std::string itempath;

	for (unsigned int i=0; i<nItems; i++)
	{
		C7ZipArchiveItem *pArchiveItem = NULL;

		if ( !pArchive->GetItemInfo(i, &pArchiveItem) )
			continue;

		if (pArchiveItem->IsDir() || pArchiveItem->IsEncrypted())
			continue;

		itempath = NarrowStrOS(pArchiveItem->GetFullPath().c_str());

#ifdef T3_SUPPORT
		// skip files not within maxdepth
		size_t pos = itempath.find_first_of("/\\");
		unsigned int depth = 0;
		while (pos != std::string::npos) // assumes itempath has no trailing (back)slash
		{
			++depth;
			pos = itempath.find_first_of("/\\", pos + 1);
		}
		if (depth > maxdepth)
#else
		// skip files not in root
		if (itempath.find_first_of("/\\") != std::string::npos)
#endif
			continue;

		list.push_back(itempath);

		if (timestamps)
		{
			time_t tm = 0;
			unsigned __int64 val = 0;

			if (pArchiveItem->GetFileTimeProperty(lib7zip::kpidMTime, val)
				|| pArchiveItem->GetFileTimeProperty(lib7zip::kpidCTime, val))
				// convert FILETIME to time_t
				tm = (val / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
			else
				tm = arch_ftime;

			timestamps->push_back(tm);
		}
	}

	CloseArchive(pArchive);

	return list.size();
}

bool IsFileInArchive(const char *archive, const char *fname)
{
	if ( !InitArchiveLib() )
		return false;

	BUSY_CURSOR();

	C7ZipArchive *pArchive = OpenArchive(archive);
	if (!pArchive)
		return false;

	const bool ret = (FindArchiveItem(pArchive, fname) != NULL);

	CloseArchive(pArchive);

	return ret;
}

bool ExtractFileFromArchive(const char *archive, const char *fname, const char *destfile, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	BUSY_CURSOR();

	time_t arch_ftime;

	C7ZipArchive *pArchive = OpenArchive(archive, &arch_ftime);
	if (!pArchive)
	{
		ERR_OPENARCH();
		return false;
	}

	C7ZipArchiveItem *pFile = FindArchiveItem(pArchive, fname);
	if (!pFile)
	{
		CloseArchive(pArchive);
		ERR_NOFILE();
		return false;
	}

	FileOutStream outFile(destfile, pFile, arch_ftime);
	if ( !outFile.IsValid() )
	{
		ASSERT(FALSE);
		CloseArchive(pArchive);
		ERR_FCREATE();
		return false;
	}

	const bool ret = pArchive->Extract(pFile, &outFile);

	if (!ret && ppErrMsg)
		GetExtractErrorString(pArchive->GetExtractError(), *ppErrMsg);

	CloseArchive(pArchive);

	if ( outFile.WriteError() )
	{
		ERR_WRITE();
		return false;
	}

	return ret;
}

bool ExtractFileFromArchive(const char *archive, const char *fname, void *&pFileData, int &nFileSize, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	BUSY_CURSOR();

	C7ZipArchive *pArchive = OpenArchive(archive);
	if (!pArchive)
	{
		ERR_OPENARCH();
		return false;
	}

	C7ZipArchiveItem *pFile = FindArchiveItem(pArchive, fname);
	if (!pFile)
	{
		CloseArchive(pArchive);
		ERR_NOFILE();
		return false;
	}

	int n = (int) pFile->GetSize();
	char *data = new char[n+2];

	MemOutStream memOutFile(data, n);

	const bool ret = pArchive->Extract(pFile, &memOutFile);
	if (ret)
	{
		// add null terminator and extra terminator as safety padding to simplify parser code, in case data is text
		data[n] = 0;
		data[n+1] = 0;

		pFileData = data;
		nFileSize = n;
	}
	else
		delete[] data;

	if (!ret && ppErrMsg)
		GetExtractErrorString(pArchive->GetExtractError(), *ppErrMsg);

	CloseArchive(pArchive);

	return ret;
}

static void* ExtractFullThread(void *p)
{
	FileOutStreamFactory &factory = *(FileOutStreamFactory*)p;
	if ( factory.GetArchive()->ExtractAll(&factory) )
		EndProgress(1);
	else
		EndProgress(0);

	return 0;
}

int ExtractFullArchive(const char *archive, const char *dest, const char *progress_label, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return 0;
	}

	BUSY_CURSOR();

	time_t arch_ftime;

	C7ZipArchive *pArchive = OpenArchive(archive, &arch_ftime);
	if (!pArchive)
	{
		ERR_OPENARCH();
		return 0;
	}

	// make sure the leaf dir exists
	if (fl_mkdir(dest, DEF_DIR_MODE) && errno != EEXIST)
	{
		// dir doesn't exist and we couldn't create it
		CloseArchive(pArchive);
		ERR_WRITE();
		return 0;
	}

	unsigned int nItems = 0;

	pArchive->GetItemCount(&nItems);

	if (!nItems)
	{
		// there's nothing to see here
		CloseArchive(pArchive);
		return 1;
	}

	std::string itempath;
	std::string sdest = dest;
	std::string stmp;

	sdest.append(DIRSEP_STR);

	int ret;

	FileOutStreamFactory factory(pArchive, sdest, stmp, arch_ftime);

	if (progress_label)
	{
		InitProgress(nItems, progress_label);

		if ( !CreateThreadOS(ExtractFullThread, &factory) )
		{
			// if thread creation fails then do non-threaded extraction (without progress bar), shouldn't normally happen
			TermProgress();
			goto unthreaded_install;
		}
		else
			ret = RunProgress() && !factory.Failed();
	}
	else
	{
		// no progress dialog
unthreaded_install:
		ret = pArchive->ExtractAll(&factory) && !factory.Failed();
	}

	if (!ret && ppErrMsg)
		GetExtractErrorString(pArchive->GetExtractError(), *ppErrMsg);

	// check for partial failure (not all files failed)
	if (ret && pArchive->GetExtractError() != lib7zip::kxerrNone)
	{
		if (ppErrMsg)
			GetExtractErrorString(pArchive->GetExtractError(), *ppErrMsg);
		ret = 2;
	}

	CloseArchive(pArchive);

	if ( factory.WriteError() )
	{
		ERR_WRITE();
		return 0;
	}

	return ret;
}

bool EnumFullArchive(const char *archive, bool (*pEnumCallback)(const char*,void*), void *pCallbackData, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	C7ZipArchive *pArchive = OpenArchive(archive);
	if (!pArchive)
	{
		ERR_OPENARCH();
		return false;
	}

	unsigned int nItems = 0;

	pArchive->GetItemCount(&nItems);

	if (!nItems)
	{
		// there's nothing to see here
		CloseArchive(pArchive);
		return true;
	}

	std::string itempath;

	for (unsigned int i=0; i<nItems; i++)
	{
		C7ZipArchiveItem *pArchiveItem = NULL;

		if ( !pArchive->GetItemInfo(i, &pArchiveItem) )
			continue;

		if (pArchiveItem->IsDir() || pArchiveItem->IsEncrypted())
			continue;

		itempath = NarrowStrOS(pArchiveItem->GetFullPath().c_str());

		if ( !(*pEnumCallback)(itempath.c_str(), pCallbackData) )
			break;
	}

	CloseArchive(pArchive);

	return true;
}

bool EnumFullArchiveEx(const char *archive, bool (*pEnumCallback)(const char*,unsigned __int64,time_t,void*), void *pCallbackData, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	time_t arch_ftime;

	C7ZipArchive *pArchive = OpenArchive(archive, &arch_ftime);
	if (!pArchive)
	{
		ERR_OPENARCH();
		return false;
	}

	unsigned int nItems = 0;

	pArchive->GetItemCount(&nItems);

	if (!nItems)
	{
		// there's nothing to see here
		CloseArchive(pArchive);
		return true;
	}

#ifdef _WIN32
	// convert FILETIME to time_t
	arch_ftime = (arch_ftime / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
#endif

	std::string itempath;

	for (unsigned int i=0; i<nItems; i++)
	{
		C7ZipArchiveItem *pArchiveItem = NULL;

		if ( !pArchive->GetItemInfo(i, &pArchiveItem) )
			continue;

		if (pArchiveItem->IsDir() || pArchiveItem->IsEncrypted())
			continue;

		itempath = NarrowStrOS(pArchiveItem->GetFullPath().c_str());

		// get file unpacked size and mtime
		unsigned __int64 val;

		time_t tm;
		if (pArchiveItem->GetFileTimeProperty(lib7zip::kpidMTime, val)
			|| pArchiveItem->GetFileTimeProperty(lib7zip::kpidCTime, val))
			// convert FILETIME to time_t
			tm = (val / (unsigned __int64)10000000) - (unsigned __int64)11644473600;
		else
			tm = arch_ftime;

		if ( !pArchiveItem->GetUInt64Property(lib7zip::kpidSize, val) )
			val = ~(unsigned __int64)0;

		if ( !(*pEnumCallback)(itempath.c_str(), val, tm, pCallbackData) )
			break;
	}

	CloseArchive(pArchive);

	return true;
}

bool BeginCreateArchive(const char *archive)
{
	if (g_pZipOutContext)
	{
		ASSERT(FALSE);
		return false;
	}

	if ( !BeginCreateZipFile(archive) )
		return false;

	return true;
}

bool EndCreateArchive(bool bAbort)
{
	if (!g_pZipOutContext)
	{
		ASSERT(FALSE);
		return false;
	}

	bool ret = EndCreateZipFile(bAbort);

	return ret;
}

bool AddFileToArchive(const char *fname, const char *zipfname)
{
	if (!fname || !*fname)
	{
		ASSERT(FALSE);
		return false;
	}

	if ( !AddFileToZip(fname, zipfname) )
		return false;

	return true;
}
