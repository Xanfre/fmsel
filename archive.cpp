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
#include <FL/fl_ask.H>
#include <FL/fl_utf8.h>
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <fstream>

#include <bit7z/bitarchiveitem.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitarchivewriter.hpp>
#include <bit7z/biterror.hpp>

#include "dbgutil.h"


#define ERR_7ZINIT()	if (ppErrMsg) *ppErrMsg = $("failed to init 7z")
#define ERR_NOFILE()	if (ppErrMsg) *ppErrMsg = $("file not found")
#define ERR_FWRITE()	if (ppErrMsg) *ppErrMsg = $("failed to write file")


void ShowBusyCursor(BOOL bShow);

void InitProgress(int nSteps, const char *label);
void TermProgress();
int RunProgress();
void SetProgress(int nStep);
void EndProgress(int result);


static bit7z::Bit7zLibrary *g_p7zLib = NULL;
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

struct ArchiveReadContext
{
	ArchiveReadContext(const char *name) : archname(name), archive(*g_p7zLib,name), userdata(NULL)
	{
	}

	std::string archname;
	bit7z::BitArchiveReader archive;
	const void *userdata;
};

struct ArchiveWriteContext
{
	ArchiveWriteContext(const char *name) : archname(name), archive(*g_p7zLib,bit7z::BitFormat::Zip), userdata(NULL)
	{
		archive.setOverwriteMode(bit7z::OverwriteMode::Overwrite);
		archive.setUpdateMode(bit7z::UpdateMode::Update);
	}

	std::string archname;
	bit7z::BitArchiveWriter archive;
	const void *userdata;
};

class ProgressCallbackHandler
{
public:
	ProgressCallbackHandler(bit7z::BitAbstractArchiveHandler& h) : handler(h), totalbytes(0)
	{
		handler.setTotalCallback(std::bind(&ProgressCallbackHandler::OnTotal, this, std::placeholders::_1));
		handler.setProgressCallback(std::bind(&ProgressCallbackHandler::OnProgress, this, std::placeholders::_1));

	}
	~ProgressCallbackHandler()
	{
		handler.setTotalCallback(std::function<void(uint64_t)>());
		handler.setProgressCallback(std::function<bool(uint64_t)>());
	}

private:
	void OnTotal(uint64_t totalbytes)
	{
		this->totalbytes = totalbytes;
	}

	bool OnProgress(uint64_t curbytes)
	{
		if (totalbytes)
			SetProgress(static_cast<int>(static_cast<double>(curbytes) / static_cast<double>(totalbytes) * 1000.0));

		return true;
	}

	bit7z::BitAbstractArchiveHandler& handler;
	uint64_t totalbytes;
};

static void GetExtractErrorString(const std::error_code &err, const char *&pErrMsg)
{
	if (err == bit7z::BitFailureSource::CRCError)
		pErrMsg = $("checksum error");
	else if (err == bit7z::BitFailureSource::DataError
		|| err == bit7z::BitFailureSource::DataAfterEnd
		|| err == bit7z::BitFailureSource::UnavailableData
		|| err == bit7z::BitFailureSource::UnexpectedEnd)
		pErrMsg = $("corrupted archive data");
	else if (err == bit7z::BitFailureSource::InvalidArchive
		|| err == bit7z::BitFailureSource::InvalidArgument
		|| err == bit7z::BitFailureSource::FormatDetectionError
		|| err == bit7z::BitFailureSource::HeadersError)
		pErrMsg = $("failed to open archive");
	else if (err == bit7z::BitFailureSource::NoSuchItem)
		pErrMsg = $("file not found");
	else if (err == bit7z::BitFailureSource::OperationNotSupported)
		pErrMsg = $("unsupported compression type");
	else if (err == bit7z::BitFailureSource::OperationNotPermitted)
		pErrMsg = $("failed to write file");
	else if (err == bit7z::BitFailureSource::WrongPassword)
		pErrMsg = $("archive is encrypted or requires password");
	else
		pErrMsg = $("unknown error");
}


/////////////////////////////////////////////////////////////////////
// INIT/TERM

static ArchiveReadContext *g_pReadArchive = NULL;
static ArchiveWriteContext *g_pWriteArchive = NULL;

static bool InitArchiveLib(BOOL bSilent = FALSE)
{
	if (!g_p7zLib && !g_bFailed7z)
	{
		BUSY_CURSOR();

		try
		{
#ifdef _WIN32
			g_p7zLib = new bit7z::Bit7zLibrary;
#else
			g_p7zLib = new bit7z::Bit7zLibrary("/usr/lib/7zip/7z.so");
#endif
		}
		catch (const bit7z::BitException& e)
		{
			g_bFailed7z = 1;
			if (!bSilent)
				fl_alert($("Failed to initialize 7-zip library, archive support will be disabled."));

			return false;
		}
	}

	return g_p7zLib != NULL;
}

void TermArchiveSystem()
{
	if (g_pReadArchive)
	{
		delete g_pReadArchive;
		g_pReadArchive = NULL;
	}
	if (g_pWriteArchive)
	{
		delete g_pWriteArchive;
		g_pWriteArchive = NULL;
	}
	if (g_p7zLib)
	{
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

bool GetUnpackedArchiveSize(const char *archname, unsigned __int64 &sz, unsigned int &numfiles, bool nocache)
{
	if ( !InitArchiveLib() )
		return false;

	BUSY_CURSOR();

	try
	{
		if (nocache || !g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		sz = g_pReadArchive->archive.size();
		numfiles = g_pReadArchive->archive.filesCount();
	}
	catch (const bit7z::BitException& e)
	{
		return false;
	}

	return true;
}

int ListFilesInArchiveRoot(const char *archname, std::vector<std::string> &list, std::vector<time_t> *timestamps)
{
	return ListFilesInArchivePruned(archname, 0, list, timestamps);
}

int ListFilesInArchivePruned(const char *archname, unsigned int maxdepth, std::vector<std::string> &list, std::vector<time_t> *timestamps)
{
	if ( !InitArchiveLib() )
		return -2;

	BUSY_CURSOR();

	time_t arch_ftime = time(NULL);
	GetFileMTimeOS(archname, arch_ftime);

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}
	}
	catch (const bit7z::BitException& e)
	{
		return -1;
	}

	try
	{
		for (const bit7z::BitArchiveItem& item : g_pReadArchive->archive.items())
		{
			if (item.isDir() || item.isEncrypted())
				continue;

			const std::string path = item.path();

			int depth = std::count(path.begin(), path.end(), '/')
				+ std::count(path.begin(), path.end(), '\\');

			if (depth > maxdepth)
				continue;

			list.push_back(path);

			if (timestamps)
			{
				time_t tm = arch_ftime;

				bit7z::BitPropVariant val = item.itemProperty(bit7z::BitProperty::MTime);
				if (val.isFileTime())
				{
					tm = std::chrono::system_clock::to_time_t(val.getTimePoint());
				}
				else
				{
					val = item.itemProperty(bit7z::BitProperty::CTime);
					if (val.isFileTime())
						tm = std::chrono::system_clock::to_time_t(val.getTimePoint());
				}

				timestamps->push_back(tm);
			}
		}
	}
	catch (const bit7z::BitException& e)
	{
	}

	return (int) list.size();
}

bool IsFileInArchive(const char *archname, const char *fname)
{
	if ( !InitArchiveLib() )
		return false;

	BUSY_CURSOR();

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		return g_pReadArchive->archive.contains(fname);
	}
	catch (const bit7z::BitException& e)
	{
		return false;
	}
}

bool ExtractFileFromArchive(const char *archname, const char *fname, const char *destfile, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	BUSY_CURSOR();

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		const bit7z::BitInputArchive::ConstIterator it = g_pReadArchive->archive.find(fname);
		if (it != g_pReadArchive->archive.cend())
		{
			bit7z::buffer_t buffer;
			g_pReadArchive->archive.extractTo(buffer, it->index());

			FILE *f = fl_fopen(destfile, "wb");
			if (!f || (buffer.size() > 0 && fwrite(buffer.data(), 1, buffer.size(), f) != buffer.size()))
			{
				if (f)
					fclose(f);

				ERR_FWRITE();
				return false;
			}

			fclose(f);
		}
		else
		{
			ERR_NOFILE();
			return false;
		}
	}
	catch (const bit7z::BitException& e)
	{
		if (ppErrMsg)
			GetExtractErrorString(e.code(), *ppErrMsg);

		return false;
	}

	return true;
}

bool ExtractFileFromArchive(const char *archname, const char *fname, void *&pFileData, int &nFileSize, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	BUSY_CURSOR();

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		const bit7z::BitInputArchive::ConstIterator it = g_pReadArchive->archive.find(fname);
		if (it != g_pReadArchive->archive.cend())
		{
			bit7z::buffer_t buffer;
			g_pReadArchive->archive.extractTo(buffer, it->index());

			size_t n = buffer.size();
			char *data = new char[n+2];

			memcpy(data, reinterpret_cast<const char*>(buffer.data()), n);

			data[n] = 0;
			data[n+1] = 0;

			pFileData = data;
			nFileSize = n;
		}
		else
		{
			ERR_NOFILE();
			return false;
		}
	}
	catch (const bit7z::BitException& e)
	{
		if (ppErrMsg)
			GetExtractErrorString(e.code(), *ppErrMsg);

		return false;
	}

	return true;
}

static void* ExtractFullThread(void *p)
{
	try
	{
		ArchiveReadContext &context = *(ArchiveReadContext*)p;

		context.archive.extractTo(static_cast<const char *>(context.userdata));

		EndProgress(1);
	}
	catch (const bit7z::BitException& e)
	{
		EndProgress(0);
	}

	return 0;
}

int ExtractFullArchive(const char *archname, const char *dest, const char *progress_label, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	BUSY_CURSOR();

	int ret = 1;

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		// make sure the leaf dir exists
		if (fl_mkdir(dest, DEF_DIR_MODE) && errno != EEXIST)
		{
			// dir doesn't exist and we couldn't create it
			ERR_FWRITE();
			return 0;
		}
	
		if (progress_label)
		{
			ProgressCallbackHandler callbackHandler(g_pReadArchive->archive);

			InitProgress(1000 /* percentage with tenths */, progress_label);

			g_pReadArchive->userdata = dest;

			if ( !CreateThreadOS(ExtractFullThread, g_pReadArchive) )
			{
				// if thread creation fails then do non-threaded extraction (without progress bar), shouldn't normally happen
				TermProgress();
				goto unthreaded_install;
			}
			else
			{
				ret = RunProgress();
			}

			g_pReadArchive->userdata = NULL;
		}
		else
		{
		// no progress dialog
unthreaded_install:
			g_pReadArchive->archive.extractTo(dest);
		}
	}
	catch (const bit7z::BitException& e)
	{
		if (ppErrMsg)
			GetExtractErrorString(e.code(), *ppErrMsg);

		return e.failedFiles().empty() ? 0 : 2;
	}

	return ret;
}

bool EnumFullArchive(const char *archname, bool (*pEnumCallback)(const char*,void*), void *pCallbackData, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		for (const bit7z::BitArchiveItem& item : g_pReadArchive->archive.items())
		{
			if (item.isDir() || item.isEncrypted())
				continue;

			if ( !(*pEnumCallback)(item.path().c_str(), pCallbackData) )
				break;
		}
	}
	catch (const bit7z::BitException& e)
	{
		if (ppErrMsg)
			GetExtractErrorString(e.code(), *ppErrMsg);

		return false;
	}

	return true;
}

bool EnumFullArchiveEx(const char *archname, bool (*pEnumCallback)(const char*,unsigned __int64,time_t,void*), void *pCallbackData, const char **ppErrMsg)
{
	if ( !InitArchiveLib() )
	{
		ERR_7ZINIT();
		return false;
	}

	time_t arch_ftime = time(NULL);
	GetFileMTimeOS(archname, arch_ftime);

	try
	{
		if (!g_pReadArchive || g_pReadArchive->archname != archname)
		{
			delete g_pReadArchive;
			g_pReadArchive = NULL;

			g_pReadArchive = new ArchiveReadContext(archname);
		}

		for (const bit7z::BitArchiveItem& item : g_pReadArchive->archive.items())
		{
			if (item.isDir() || item.isEncrypted())
				continue;

			time_t tm = arch_ftime;

			bit7z::BitPropVariant val = item.itemProperty(bit7z::BitProperty::MTime);
			if (val.isFileTime())
			{
				tm = std::chrono::system_clock::to_time_t(val.getTimePoint());
			}
			else
			{
				val = item.itemProperty(bit7z::BitProperty::CTime);
				if (val.isFileTime())
					tm = std::chrono::system_clock::to_time_t(val.getTimePoint());
			}

			if ( !(*pEnumCallback)(item.path().c_str(), item.size(), tm, pCallbackData) )
				break;
		}

		return true;
	}
	catch (const bit7z::BitException& e)
	{
		if (ppErrMsg)
			GetExtractErrorString(e.code(), *ppErrMsg);

		return false;
	}
}

bool BeginCreateArchive(const char *archname)
{
	if ( !InitArchiveLib() )
		return false;

	if (g_pWriteArchive)
	{
		ASSERT(FALSE);
		delete g_pWriteArchive;
		g_pWriteArchive = NULL;
	}

	try
	{
		g_pWriteArchive = new ArchiveWriteContext(archname);
	}
	catch (const bit7z::BitException& e)
	{
		return false;
	}

	return true;
}

bool EndCreateArchive(bool bAbort)
{
	if (!g_pWriteArchive)
	{
		ASSERT(FALSE);
		return false;
	}

	bool ret = !bAbort;

	if (!bAbort)
	{
		try
		{
			g_pWriteArchive->archive.compressTo(g_pWriteArchive->archname.c_str());
		}
		catch (const bit7z::BitException& e)
		{
			fprintf(stderr, "%s\n", e.what());
			ret = false;
		}
	}
	
	delete g_pWriteArchive;
	g_pWriteArchive = NULL;

	return ret;
}

bool AddFileToArchive(const char *fname, const char *zipfname)
{
	if (!fname || !*fname || !zipfname || !*zipfname || !g_pWriteArchive)
	{
		ASSERT(FALSE);
		return false;
	}

	try
	{
		g_pWriteArchive->archive.addFile(fname, zipfname);
	}
	catch (const bit7z::BitException& e)
	{
		return false;
	}

	return true;
}
