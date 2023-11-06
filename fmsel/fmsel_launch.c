/*
 * FMSel Launcher
 * Launch FMSel with the specified data parameters without a game executable.
 *
 * This serves as an example of how to work with the library, notably preparing
 * the FMSel data structure, invoking FMSel itself, and processing the modified
 * data. However, it also serves as a utility to allow outside programs (e.g.
 * Wine) to interact with FMSel transparently, even being able to receive
 * the modified data contents through a pipe.
 */

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#define NOSERVICE
#define NOMCX
#define NOIMM
#include <windows.h>
#define getcwd _getcwd
#else
#define _POSIX_C_SOURCE 1
#include <limits.h>
#include <unistd.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fmsel.h"

// error status
#define FMSEL_L_ERR_WARN 0
#define FMSEL_L_ERR_FATAL 1

// return codes
#define FMSEL_L_RET_ERR -2 // must not be in eFMSelReturn

// prototypes
int StartFMSel(const char *game, const char *rootPath, const char *lang,
	const char *exited, const int rootLen, const int nameLen,
	const int modExcludeLen, const int langLen,
#ifdef _WIN32
	const unsigned long pipeFD
#else
	const int pipeFD
#endif
);
void ShowError(const char *message, const int fatal);
#ifdef _WIN32
void WriteDataToPipe(const sFMSelectorData *data, const HANDLE pipeFD);
#else
void WriteDataToPipe(const sFMSelectorData *data, const int pipeFD);
#endif

/*
 * main():
 * Entry point.
 * Returns FMSel's exit code on success and FMSEL_L_RET_ERR on failure.
 */
int main(const int argc, const char **argv)
{
	// Check parameter count.
	if ((1 != argc && 4 != argc && 5 != argc && 9 != argc && 10 != argc)
		|| (argc >= 5 && strcmp(argv[4], "true") && strcmp(argv[4], "false")))
	{
		printf("Usage:\n\t%s [GameVersion RootPath Language ["
			" ExitedGame(true,false) [ MaxRootLen MaxNameLen MaxModExcludeLen"
			" LanguageLen [ PipePID ] ] ] ]\n\n", argv[0]);
		return FMSEL_L_RET_ERR;
	}
	// Establish defaults and start FMSel.
	const int strsSet = argc >= 4;
	const int sizesSet = argc >= 9;
	return StartFMSel(strsSet ? argv[1] : "(No Game Running)",
		strsSet ? argv[2] : NULL, strsSet ? argv[3] : "english",
		argc >= 5 ? argv[4] : "false", sizesSet ? atoi(argv[5]) : PATH_MAX,
		sizesSet ? atoi(argv[6]) : 1 << 7, sizesSet ? atoi(argv[7]) : PATH_MAX,
		sizesSet ? atoi(argv[8]) : 1 << 6,
#ifdef _WIN32
		10 == argc ? strtoul(argv[9], NULL, 0) : 0
#else
		10 == argc ? atoi(argv[9]) : -1
#endif
	);
}

/*
 * StartFMSel:
 * Prepares the FM data structure and starts the FM selector.
 * Returns FMSel's exit code on success and FMSEL_L_RET_ERR on failure.
 */
int StartFMSel(const char *game, const char *rootPath, const char *lang,
	const char *exited, const int rootLen, const int nameLen,
	const int modExcludeLen, const int langLen,
#ifdef _WIN32
	const unsigned long pipeFD
#else
	const int pipeFD
#endif
)
{
	// Check size lengths.
	if (rootLen <= 0 || nameLen <= 0 || modExcludeLen <= 0 || langLen <= 0)
	{
		ShowError("Provided string sizes are invalid or too small.",
			FMSEL_L_ERR_FATAL);
		return FMSEL_L_RET_ERR;
	}
	// Construct FMSel data.
	sFMSelectorData data;
	data.nStructSize = sizeof(sFMSelectorData);
	data.sGameVersion = game;
	data.sRootPath = calloc(rootLen, sizeof(char));
	data.nMaxRootLen = rootLen;
	data.sName = calloc(nameLen, sizeof(char));
	data.nMaxNameLen = nameLen;
	data.bExitedGame = strcmp(exited, "true") ? 0 : 1;
	data.bRunAfterGame = 0;
	data.sModExcludePaths = calloc(modExcludeLen, sizeof(char));
	data.nMaxModExcludeLen = modExcludeLen;
	data.sLanguage = calloc(langLen, sizeof(char));
	data.nLanguageLen = langLen;
	data.bForceLanguage = 0;
	// Check allocated buffers.
	if (NULL == data.sRootPath || NULL == data.sName
		|| NULL == data.sModExcludePaths || NULL == data.sLanguage)
	{
		ShowError("Could not allocate memory for FMSel state.",
			FMSEL_L_ERR_FATAL);
		if (NULL != data.sRootPath)
			free(data.sRootPath);
		if (NULL != data.sName)
			free(data.sName);
		if (NULL != data.sModExcludePaths)
			free(data.sModExcludePaths);
		if (NULL != data.sLanguage)
			free(data.sLanguage);
		return FMSEL_L_RET_ERR;
	}
	// Copy strings to buffers.
	if (NULL != rootPath)
		strncpy(data.sRootPath, rootPath, data.nMaxRootLen - 1);
	else
		getcwd(data.sRootPath, data.nMaxRootLen);
	strncpy(data.sLanguage, lang, data.nLanguageLen - 1);
	// Start FMSel and retrieve the error code.
	const int ret = SelectFM(&data);
	// Pipe resulting configuration back to specified PID.
#ifdef _WIN32
	if (0 != pipeFD)
		WriteDataToPipe(&data, (HANDLE) pipeFD);
#else
	if (pipePID >= 0)
		WriteDataToPipe(&data, pipeFD);
#endif
	// Free buffers.
	free(data.sRootPath);
	free(data.sName);
	free(data.sModExcludePaths);
	free(data.sLanguage);
	// Return with FMSel's error code.
	return ret;
}

/*
 * ShowError:
 * Show a message either as a warning or as a fatal error.
 */
void ShowError(const char *message, const int fatal)
{
#ifdef _WIN32
	if (fatal)
		MessageBoxA(NULL, message, "Fatal Error", MB_ICONERROR | MB_OK);
	else
		MessageBoxA(NULL, message, "Warning", MB_ICONWARNING | MB_OK);
#else
	if (fatal)
		printf("\033[0;31mFatal Error:");
	else
		printf("\033[0;33mWarning:");
	printf("\033[0m %s\n", message);
#endif
}

/*
 * WriteDataToPipe:
 * Write the relevant (non-constant) data in the sFMSelectorData structure
 * to the write end of a pipe.
 * The write end of the pipe is specified by pipeFD and the read end should be
 * closed prior to program invocation.
 */
#ifdef _WIN32
void WriteDataToPipe(const sFMSelectorData *data, const HANDLE pipeFD)
#else
void WriteDataToPipe(const sFMSelectorData *data, const int pipeFD)
#endif
{
	const size_t bufSize = data->nMaxRootLen + data->nMaxNameLen
		+ data->nMaxModExcludeLen + data->nLanguageLen + (3 * sizeof(int));
	uint8_t *buf, *bufPtr;
	if (NULL != (buf = calloc(bufSize, 1)))
	{
		bufPtr = buf;
		strcpy((char *) bufPtr, data->sRootPath);
		bufPtr += data->nMaxRootLen * sizeof(char);
		strcpy((char *) bufPtr, data->sName);
		bufPtr += data->nMaxNameLen * sizeof(char);
		strcpy((char *) bufPtr, data->sModExcludePaths);
		bufPtr += data->nMaxModExcludeLen * sizeof(char);
		strcpy((char *) bufPtr, data->sLanguage);
		bufPtr += data->nLanguageLen * sizeof(char);
		*((int *) bufPtr) = data->bExitedGame;
		bufPtr += sizeof(int);
		*((int *) bufPtr) = data->bRunAfterGame;
		bufPtr += sizeof(int);
		*((int *) bufPtr) = data->bForceLanguage;
#ifdef _WIN32
		DWORD bytesWritten;
		if (!WriteFile(pipeFD, buf, bufSize, &bytesWritten, NULL)
			|| bufSize != bytesWritten)
#else
		if (bufSize != (size_t) write(pipeFD, buf, bufSize))
#endif
			ShowError("Full data not written to pipe.", FMSEL_L_ERR_WARN);
		free(buf);
	}
	else
		ShowError("Could not allocate output buffer.", FMSEL_L_ERR_WARN);
#ifdef _WIN32
	if (!CloseHandle(pipeFD))
#else
	if (-1 == close(pipeFD))
#endif
		ShowError("Could not close write end of pipe.", FMSEL_L_ERR_WARN);
}

