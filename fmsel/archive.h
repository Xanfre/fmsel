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

#ifndef _ARCHIVE_H_
#define _ARCHIVE_H_

#include <string>
#include <vector>
#include <time.h>


void TermArchiveSystem();

// check if file type is a supported archive format ('ext' is "ZIP" etc.)
bool IsArchiveFormatSupported(const char *ext);

// get total size of unpacked files
bool GetUnpackedArchiveSize(const char *archive, unsigned __int64 &sz, unsigned int &numfiles, bool nocache = false);

// get a list of files, fl_filename_list counterpart for archives, only looks for files in the archive root since
// that's all we care about, optionally returns list with file timestamps (1:1 indexing with 'list')
int ListFilesInArchiveRoot(const char *archive, std::vector<std::string> &list, std::vector<time_t> *timestamps = NULL);

#ifdef SUPPORT_T3
// generalized version of ListFilesInArchiveRoot, which looks for files while pruning at the specified depth (root being depth 0)
int ListFilesInArchivePruned(const char *archive, unsigned int maxdepth, std::vector<std::string> &list, std::vector<time_t> *timestamps = NULL);
#endif

// check if sepcified file exists in archive (if 'fname' contains dirs then it's assumed to use OS specific separators)
bool IsFileInArchive(const char *archive, const char *fname);

// extract a single file from archive and save it as 'destfile'
bool ExtractFileFromArchive(const char *archive, const char *fname, const char *destfile, const char **ppErrMsg = NULL);

// extract a single file from archive to memory buffer, caller must 'delete' it
bool ExtractFileFromArchive(const char *archive, const char *fname, void *&pFileData, int &nFileSize, const char **ppErrMsg = NULL);

// extract archive to destination path, if the leaf dir in the dest path doesn't exist then it attempts to create it
// if progress_label is NULL then no progress dialog will be shown
// returns 0 on failure, 1 on success and 2 if some files failed to extract correctly
int ExtractFullArchive(const char *archive, const char *dest, const char *progress_label = "Extracting...", const char **ppErrMsg = NULL);

// enumerate all files in archive (callback returns 'false' when it wants to stop enumeration)
bool EnumFullArchive(const char *archive, bool (*pEnumCallback)(const char*,void*), void *pCallbackData, const char **ppErrMsg = NULL);
bool EnumFullArchiveEx(const char *archive, bool (*pEnumCallback)(const char*,unsigned __int64,time_t,void*), void *pCallbackData, const char **ppErrMsg = NULL);

// start (zip) archive creation
bool BeginCreateArchive(const char *archive);
// end/close created archive (call this after a successful BeginCreateArchive)
bool EndCreateArchive(bool bAbort = false);
// add file (with path relative to FM root) to archive currently being created (called between Begin/EndCreateArchive)
bool AddFileToArchive(const char *fname, const char *zipfname);

#endif // _ARCHIVE_H_
