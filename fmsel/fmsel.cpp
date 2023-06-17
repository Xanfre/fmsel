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

// UTF-8 USAGE
// -----------
// All file and directory names (including the raw FM name since that boils down to a directory) are NOT in UTF-8
// format, they're in "raw" unconverted format to keep things simple, all other strings (nice names, tags, notes
// etc. etc.) are in UTF-8 format

#include "fmsel.h"
#include "os.h"
#include "archive.h"
#include "lang.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#define rsize_t size_t
#define _S_IREAD S_IREAD
#define _S_IWRITE S_IWRITE
#define _mkgmtime timegm
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strdup strdup
#define _strtoui64 strtoull
#define _snprintf_s(a,b,c,...) snprintf(a,b,__VA_ARGS__)
#define strcat_s(a,b,c) strncat(a,c,b-strlen(a)-1)
#define _chmod chmod
#define _mkdir(a) mkdir(a,0755)
#define _rmdir rmdir
#endif
#ifdef _MSC_VER
#include <stddef.h>
#else
#include <cstdint>
#endif
#include <errno.h>
#include <FL/Fl.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Menu.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Round_Button.H>
#include <FL/filename.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/Fl_Browser_.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include "Fl_Html_View.H"
#include "Fl_Table/Fl_Table_Row.H"
#include <FL/fl_draw.H>
#undef min
#undef max
#include "dbgutil.h"
#if !defined(_WIN32) && defined(__int64)
#undef __int64
#endif
#include "lib7zip.h"
#include "mp3.h"

#include "icons.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
using std::vector;
using std::string;
#ifdef _MSC_VER
using std::tr1::unordered_map;
using std::tr1::hash;
using std::tr1::unordered_multimap;
#else
#include <list>
using std::unordered_map;
using std::hash;
using std::unordered_multimap;
#endif


// don't localize these, they are used as directory names
static const char *g_languages[] =
{
	"english", // must be first
	"czech",
	"dutch",
	"french",
	"german",
	"hungarian",
	"italian",
	"japanese",
	"polish",
	"russian",
	"spanish"
};
const int NUM_LANGUAGES = sizeof(g_languages)/sizeof(g_languages[0]);

static const char *TAGS_LABEL = "tags:";
static const char *RATING_COL_LABEL = "Rating";


#define USER_CLR		96
#define TOGBTN_DN_CLR		(Fl_Color)USER_CLR
#define HTML_POPUP_BOX		FL_FREE_BOXTYPE
#define COMPLETION_LIST_BOX	((Fl_Boxtype)(FL_FREE_BOXTYPE+1))
#define FLAT_MARGIN_BOX		((Fl_Boxtype)(FL_FREE_BOXTYPE+2))
#define IMAGETEXT_LABEL		FL_FREE_LABELTYPE
#define RADIO_EMPTY_LABEL	((Fl_Labeltype)(FL_FREE_LABELTYPE+1))
#define RADIO_SET_LABEL		((Fl_Labeltype)(FL_FREE_LABELTYPE+2))

// list of characters that are illegal in tags and are replaced with underscores
#define ILLEGAL_TAG_CHARS ":*;,/\"\\<>[]{}"

// max text length for FM description (should be kept relatively short as it's meant for a web search result
// style of brief description, not full blown release notes like an info file)
#define MAX_DESCR_LEN 1500


#ifndef NULL
#define NULL 0
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define MAX_PATH_BUF (MAX_PATH+1)

#ifndef FALSE
#define FALSE 0
#define TRUE 1
typedef int BOOL;
#endif

#ifdef _WIN32
#define DIRSEP "\\"
#else
#define DIRSEP "/"
#endif

#if defined(WIN32) || defined(__EMX__) && !defined(__CYGWIN__)
static inline int isdirsep(char c) {return c=='/' || c=='\\';}
#else
#define isdirsep(c) ((c)=='/')
#endif

#define isspace_(_ch) isspace((unsigned char)(_ch))

#ifdef CUSTOM_FLTK
#define NO_COMP_UTFCONV NULL,TRUE
#define ALIGN_NO_DRAW (Fl_Align)(FL_ALIGN_TOP_LEFT|FL_ALIGN_CLIP|FL_ALIGN_WRAP),0
#else
#define NO_COMP_UTFCONV NULL
#define ALIGN_NO_DRAW 0
#define fl_filename_absolute_ex(a, b, c, d) fl_filename_absolute(a, b, c)
#define fl_filename_isdir_ex(a, b) fl_filename_isdir(a)
#endif

#ifdef _MSC_VER
#define DATFMT "I64X"
#else
#define DATFMT "lX"
#endif

static int tolower_utf(const char *str, int len, char *buf)
{
	if (!str || !*str)
	{
		*buf = 0;
		return 0;
	}

	const int n = strlen(str) + 1;
	ASSERT(n < len);
	const int r = fl_utf_tolower((const unsigned char *)str, std::min(n, len), buf);
	buf[len-1] = 0;
	return r;
}

// same as fl_utf_strncasecmp except 'n' specifies the lengths in raw number of bytes and not in unicode characters
int strncasecmp_utf_b(const char *s1, const char *s2, int n)
{
	for (int i=0; n>0; i++)
	{
		int l1, l2;

		if (!*s1 && !*s2)
			return 0;

		const unsigned int u1 = fl_utf8decode(s1, NULL, &l1);
		const unsigned int u2 = fl_utf8decode(s2, NULL, &l2);
		const int res = fl_tolower(u1) - fl_tolower(u2);
		if (res)
			return res;

		n -= l2;
		s1 += l1;
		s2 += l2;
	}
	return 0;
}

#ifndef __APPLE__
static string ToUTF(const string &s)
{
	if ( !s.length() )
		return s;

	if (s.length() < 1024)
	{
		char buf[1024*3];
		fl_utf8from_mb(buf, sizeof(buf), s.c_str(), s.length());
		return string(buf);
	}
	else
	{
		char *buf = new char[s.length()*3+1];
		fl_utf8from_mb(buf, s.length()*3+1, s.c_str(), s.length());
		string ret(buf);
		delete[] buf;
		return ret;
	}
}
#else
#define ToUTF(_x) _x
#endif

#ifndef __APPLE__
static string FromUTF(const string &s)
{
	if ( !s.length() )
		return s;

	if (s.length() < 1024)
	{
		char buf[1024*3];
		fl_utf8to_mb(s.c_str(), s.length(), buf, sizeof(buf));
		return string(buf);
	}
	else
	{
		char *buf = new char[s.length()+1];
		fl_utf8to_mb(s.c_str(), s.length(), buf, s.length()+1);
		string ret(buf);
		delete[] buf;
		return ret;
	}
}
#else
#define FromUTF(_x) _x
#endif

#define SCALE(_x) _x * scale / 4
#define SCALECFG(_x) _x * g_cfg.uiscale / 4
#define REGCLR(_r,_g,_b) Fl::set_color((Fl_Color)c++,_r,_g,_b);
#define SWPCLR(_x,_y) color = Fl::get_color((Fl_Color)_x); Fl::set_color((Fl_Color)_x,dark_cmap[_y]); dark_cmap[_y++] = color;
#define DARK() g_cfg.uitheme == 1


enum
{
	TABPAGE_MISC,
	TABPAGE_TAGS,
	TABPAGE_DESCR,
	TABPAGE_NOTES,
};


struct FMEntry;
class Fl_FM_Return_Button;
class Fl_FM_Filter_Input;
class Fl_FM_Tag_Input;
class Fl_FM_List;
class Fl_FM_Tag_Group;
class Fl_FM_TagEd_Window;
class Fl_FM_Filter_Button;


static BOOL g_bDbModified = FALSE;
static BOOL g_bRunningEditor = FALSE;
static BOOL g_bRunningShock = FALSE;

static eFMSelReturn g_appReturn = kSelFMRet_ExitGame;
static sFMSelectorData *g_pFMSelData = NULL;

// temp dir used to extract archived file for temporary use, empty if no temp dir is available (slash-terminated)
static string g_sTempDir;
static string g_sTempDirAbs;


static void InvalidateTagDb();
static void RefreshFilteredDb(BOOL bUpdateListControl = TRUE, BOOL bReSortOnly = FALSE);
static void InvalidateTagFilterHash();
static void AddTagFilter(const char *tagfilter, int op, BOOL bLoading = FALSE);
static BOOL CleanTag(char *tag, BOOL bFilter);
const char* $tag(const char *tag);

static BOOL FmFileExists(const FMEntry *fm, const char *fname);
static BOOL FmReadFileToBuffer(const FMEntry *fm, const char *fname, char *&data, int &len);

static string Trimmed(const char *s, int leftright = 3);
#ifdef _WIN32
static void TrimTrailingSlashFromPath(string &path, BOOL bTrimRoot = FALSE);
#else
static void TrimTrailingSlashFromPath(string &path, BOOL bTrimRoot = TRUE);
#endif
static string EscapeString(const char *s);
static const char* stristr(const char *_Str, const char *_LowerCaseSubStr);
static char* strcpy_safe(char *_Dst, rsize_t _SizeInBytes, const char *_Src);
char* skip_ws(char *s);
static char* next_line(char *s);
static int remove_forced(const char *s);
static void CleanDirSlashes(string &s);
static void CleanDirSlashes(char *s);
static BOOL AutoScanReleaseDate(FMEntry *fm, BOOL bSkipFmIni);
static void AutoScanReleaseDates();
static BOOL ApplyFmIni(FMEntry *fm, BOOL bFallbackModIni = TRUE);
static BOOL FmDelTree(FMEntry *fm);
static BOOL InstallFM(FMEntry *fm);
static BOOL UninstallFM(FMEntry *fm);
static void ConfigArchivePath(BOOL bStartupConfig = FALSE);

static void UpdateFilterControls();
static FMEntry* GetCurSelFM();
static void RefreshListControl(FMEntry *pPrevSel, BOOL bOnlyChangedOrder);
static void RefreshListControlRowSize();
static void RedrawListControl(BOOL bUpdateButtons = FALSE);
static void SelectNeighborFM();
static void ViewSummary(FMEntry *fm);
static void ViewArchiveContents(FMEntry *fm);
static const char* GenericHtmlTextPopup(const char *caption, const char *text, int W = 600, int maxH = 800);
static void ViewAbout();
static string TextToHtml(const char *text);

static int DoImportBatchFmIniDialog();

void InitProgress(int nSteps, const char *label);
void TermProgress();
int RunProgress();
void StepProgress(int nSteps);
void EndProgress(int result);

static void DoTagEditor(FMEntry *fm, int page = TABPAGE_TAGS);
static void OnTagEdAddTag(Fl_Button *o, void *p);
static void OnAddCustomTag(Fl_FM_Tag_Input *o, void *p);
static void OnTagPreset(Fl_Choice *o, void *p);
static void OnTagEdViewInfoFile(Fl_Button *o, void *p);
static void SetAddTagInputText(const char *s);
static void OnCancelTagEditor(Fl_Button*, void*);
static void OnOkTagEditor(Fl_Button*, void*);

static void OnClose(Fl_Widget *o, void *p);
static void OnResetFilters(Fl_Button *o, void *p);
static void OnRefreshFilters(Fl_Button *o, void *p);
static void OnFilterName(Fl_FM_Filter_Input *o, void *p);
static void OnFilterToggle(Fl_FM_Filter_Button *o, void *p);
static void OnFilterRating(Fl_Choice *o, void *p);
static void OnFilterPriority(Fl_Choice *o, void *p);
static void OnFilterRelFrom(Fl_FM_Filter_Input *o, void *p);
static void OnFilterRelTo(Fl_FM_Filter_Input *o, void *p);
static void OnPlayOM(Fl_Button *o, void *p);
static void OnPlayFM(Fl_FM_Return_Button *o, void *p);
static void OnStartFM(Fl_Button *o, void *p);
static void OnExit(Fl_Button *o, void *p);
static void OnListSelChange(FMEntry *fm);

static void InvokeAddTagFilter(const char *tag);

#include "fl_mainwnd.h"
#include "fl_taged.h"

static unsigned int dark_cmap[75] = {
#include "dark_cmap.h"
};

// DoImportBatchFmIniDialog mode flags
enum ImportMode
{
	IMP_Name		= 1<<0,
	IMP_RelDate		= 1<<1,
	IMP_Descr		= 1<<2,
	IMP_InfoFile	= 1<<3,
	IMP_Tags		= 1<<4,

	IMP_ModeFill		= 1<<16,
	IMP_ModeFillAddTags	= 1<<17,
	IMP_ModeOverwrite	= 1<<18,
	IMP_ModeFiltOny		= 1<<19,
};


/////////////////////////////////////////////////////////////////////
// Menu construction helpers

#define MENU_SET(...) { ASSERT(menu_items < MAX_MENU_ITEMS); Fl_Menu_Item __mi = __VA_ARGS__; menu[menu_items++] = __mi; }

#define MENU_ITEM_EX(_name, _id, _flgs) MENU_SET( { (_name), 0, NULL, (void*)(intptr_t)(_id), (_flgs), 0, FL_HELVETICA, FL_NORMAL_SIZE } )
#define MENU_RITEM(_name, _id, _selected) if (1) { MENU_SET( { (_name), 0, NULL, (void*)(intptr_t)(_id), (_selected)?FL_MENU_INACTIVE:0, 0, (_selected)?FL_HELVETICA_BOLD:FL_HELVETICA, FL_NORMAL_SIZE } ); menu[menu_items-1].labeltype((_selected)?RADIO_SET_LABEL:RADIO_EMPTY_LABEL); }
#define MENU_TITEM(_name, _id, _selected) MENU_ITEM_EX(_name, _id, FL_MENU_TOGGLE|((_selected)?FL_MENU_VALUE:0));
#define MENU_SUB(_name) MENU_SET( { (_name), 0, NULL, NULL, FL_SUBMENU, 0, FL_HELVETICA, FL_NORMAL_SIZE } )
#define MENU_END() MENU_SET( { 0 } )

#define MENU_MOD_DIV() menu[menu_items-1].flags |= FL_MENU_DIVIDER
#define MENU_MOD_DISABLE(_cond) if (_cond) menu[menu_items-1].flags |= FL_MENU_INACTIVE

#define MENU_ITEM(_name, _id) MENU_ITEM_EX(_name, _id, 0)


/////////////////////////////////////////////////////////////////////
// Config

#define POS_UNDEF -99999

enum FMPriority
{
	PRIO_Normal = 0,
	PRIO_High,
	PRIO_Urgent,
	PRIO_Critical
};

enum ListColumns
{
	COL_Name,
	COL_Priority,
	COL_Status,
	COL_LastPlayed,
	COL_ReleaseDate,

	COL_DirName,
	COL_Archive,

	COL_NUM_COLS
};

// negative sort mode for reversed order
enum SortMode
{
	SORT_None,

	SORT_Name,
	SORT_Rating,
	SORT_Priority,
	SORT_Status,
	SORT_LastPlayed,
	SORT_ReleaseDate,
	SORT_DirName,
	SORT_Archive,

	SORT_NUM_MODES
};

enum FilterOp
{
	FOP_OR,		// search result must contain at least one of all ORed tag filters
	FOP_AND,	// search result must contain this tag
	FOP_NOT,	// search result must NOT contain this tag

	FOP_NUM_OPS
};

static const char *FilterOpNames[] = { "OR", "AND", "NOT" };

enum FilterShow
{
	FSHOW_NotPlayed		= 1,
	FSHOW_Completed		= 2,
	FSHOW_InProgress	= 4,
	FSHOW_NotAvail		= 8,	// show mission that aren't available at all (not installed and not archive)
	FSHOW_ArchivedOnly	= 16,	// show missions that aren't installed but have archive
};

#define FSHOW_Default	(FSHOW_NotPlayed | FSHOW_InProgress | FSHOW_ArchivedOnly)
#define FSHOW_All		(FSHOW_NotPlayed | FSHOW_Completed | FSHOW_InProgress | FSHOW_NotAvail | FSHOW_ArchivedOnly)

enum ReturnMode
{
	RET_NEVER,	// never return to fmsel
	RET_FM,		// return only after playing FM
	RET_ALWAYS	// always return
};

enum
{
	DATEFMT_CurLocale,

	DATEFMT_YYMMDD,
	DATEFMT_YYYYMMDD,
	DATEFMT_MMDDYY,
	DATEFMT_MMDDYYYY,
	DATEFMT_DDMMYY,
	DATEFMT_DDMMYYYY,

	DATEFMT_YYMMDD_Slash,
	DATEFMT_YYYYMMDD_Slash,
	DATEFMT_MMDDYY_Slash,
	DATEFMT_MMDDYYYY_Slash,
	DATEFMT_DDMMYY_Slash,
	DATEFMT_DDMMYYYY_Slash,

	DATEFMT_MonthYYYY,
	DATEFMT_MonthDDYYYY,
	DATEFMT_DDMonthYYYY,

	DATEFMT_NUM_FORMATS
};

static const char *g_DateFmt[DATEFMT_NUM_FORMATS] =
{
	"%x",

	"%y-%m-%d",
	"%Y-%m-%d",
	"%m-%d-%y",
	"%m-%d-%Y",
	"%d-%m-%y",
	"%d-%m-%Y",

	"%y/%m/%d",
	"%Y/%m/%d",
	"%m/%d/%y",
	"%m/%d/%Y",
	"%d/%m/%y",
	"%d/%m/%Y",

	"%b %Y",
	"%b %d %Y",
	"%d %b %Y",
};


struct FMSelConfig
{
	string lastfm;
	int datefmt;

	BOOL bWindowMax;
	int windowpos[2];
	int windowsize[2];
	int colsizes[COL_NUM_COLS];

	int windowsize_taged[2];

	int sortmode;
	int tagrows;
	BOOL bVarSizeList;
	int colvis;

	// filter settings
	string filtName;
	int filtMinRating;
	int filtMinPrio;
	int filtShow;
	int filtShowDefault;
	int filtReleaseFrom;
	int filtReleaseTo;
	vector<const char*> tagFilterList[FOP_NUM_OPS];

	// 0 - game exe, 1 - editor
	int returnAfterGame[2];

	int uitheme;
	BOOL bLargeFont;
	int uiscale;

	// do differential backups (ie. backup all files that have been added or modified instead of only savegames and shots)
	BOOL bDiffBackups;
	// review diffs before doing a differential backup
	BOOL bReviewDiffBackup;

	BOOL bDecompressOGG;
	BOOL bGenerateMissFlags;

	unsigned int dwLastProcessID;

	// when a new FM is encountered, that contains an fm.ini(/mod.ini) then mark it as modified so it gets saved
	// (doing this avoids reparsing the fm.ini each time fmsel starts up, which for many 7z archives can be very slow)
	BOOL bSaveNewDbEntriesWithFmIni;

	// automatically refresh filtered db after editing FM properties
	BOOL bAutoRefreshFilteredDb;

	// word wrapping for description and notes text edit controls
	BOOL bWrapDescrEditor;
	BOOL bWrapNotesEditor;

	// optional directory for archive repository (if none is specified then archive support is disabled)
	string archiveRepo;

	// optional custom app to use opening hyperlinks (if the default browser isn't desired)
	string browserApp;

	// filtRelease* converted to time_t ready for filter comparisons
	time_t filtReleaseMinTime;
	time_t filtReleaseMaxTime;

	// run-time var to indicated that archive repo was found
	BOOL bRepoOK;

protected:
	void DestroyTagFilters()
	{
		for (int i=0; i<FOP_NUM_OPS; i++)
			if ( !tagFilterList[i].empty() )
			{
				for (int j=0; j<(int)tagFilterList[i].size(); j++)
					free((void*)tagFilterList[i][j]);

				tagFilterList[i].clear();
			}
	}

public:
	FMSelConfig()
	{
		datefmt = DATEFMT_CurLocale;
		bWindowMax = FALSE;
		windowpos[0] = windowpos[1] = POS_UNDEF;
		windowsize[0] = windowsize[1] = 0;
		windowsize_taged[0] = windowsize_taged[1] = 0;
		memset(colsizes, 0xff, sizeof(colsizes));
		sortmode = SORT_Name;
		tagrows = 2;
		bVarSizeList = TRUE;
		colvis = COL_Name|COL_Priority|COL_Status|COL_LastPlayed|COL_ReleaseDate;
		filtMinRating = -1;
		filtMinPrio = 0;
		filtShow = FSHOW_Default;
		filtShowDefault = FSHOW_Default;
		filtReleaseFrom = -1;
		filtReleaseTo = -1;
		filtReleaseMinTime = 0;
#ifdef _MSC_VER
		filtReleaseMaxTime = 0x7fffffffffffffffLL;
#else
		filtReleaseMaxTime = 0x7fffffffL;
#endif
		returnAfterGame[0] = RET_NEVER;
		returnAfterGame[1] = RET_NEVER;
		uitheme = 0;
		bLargeFont = FALSE;
		uiscale = 4;
		bDiffBackups = FALSE;
		bReviewDiffBackup = FALSE;
		bDecompressOGG = FALSE;
		bGenerateMissFlags = TRUE;
		dwLastProcessID = 0;
		bAutoRefreshFilteredDb = TRUE;
		bWrapDescrEditor = TRUE;
		bWrapNotesEditor = TRUE;
		bSaveNewDbEntriesWithFmIni = TRUE;
		bRepoOK = FALSE;
	}

	~FMSelConfig()
	{
		DestroyTagFilters();
	}

	void OnModified()
	{
		g_bDbModified = TRUE;
	}

	const BOOL HasFilters() const
	{
		if (filtMinRating != -1 || filtMinPrio || filtShow != FSHOW_All || filtReleaseFrom != -1
			|| filtReleaseTo != -1 || !filtName.empty())
			return TRUE;

		return HasTagFilters();
	}

	const BOOL HasTagFilters() const
	{
		for (int i=0; i<FOP_NUM_OPS; i++)
			if ( !tagFilterList[i].empty() )
				return TRUE;

		return FALSE;
	}

	void SetLastFM(const char *name)
	{
		OnModified();

		if (name && *name)
			lastfm = name;
		else
			lastfm.clear();
	}

	void SetDatFmt(int n)
	{
		OnModified();

		datefmt = n;
	}

	void SetSortMode(int n)
	{
		if (sortmode != n)
		{
			sortmode = n;
			OnModified();
			RefreshFilteredDb(TRUE, TRUE);
		}
	}

	void ResetFilters()
	{
		BOOL bModified = FALSE;

		if ( !filtName.empty() )
		{
			filtName.clear();
			OnModified();
			bModified = TRUE;
		}

		if (filtMinRating != -1 || filtMinPrio || filtShow != filtShowDefault)
		{
			filtMinRating = -1;
			filtMinPrio = 0;
			filtShow = filtShowDefault;
			OnModified();
			bModified = TRUE;
		}

		if (filtReleaseFrom != -1 || filtReleaseTo != -1)
		{
			filtReleaseFrom = -1;
			filtReleaseTo = -1;
			OnModified();
			OnFilterReleaseChanged();
			bModified = TRUE;
		}

		for (int i=0; i<FOP_NUM_OPS; i++)
			if ( !tagFilterList[i].empty() )
			{
				OnModified();
				bModified = TRUE;
				InvalidateTagFilterHash();
				DestroyTagFilters();
				break;
			}

		if (bModified)
		{
			UpdateFilterControls();
			RefreshFilteredDb();
		}
	}

	void ResetTagFilters()
	{
		BOOL bModified = FALSE;

		for (int i=0; i<FOP_NUM_OPS; i++)
			if ( !tagFilterList[i].empty() )
			{
				OnModified();
				bModified = TRUE;
				InvalidateTagFilterHash();
				DestroyTagFilters();
				break;
			}

		if (bModified)
		{
			UpdateFilterControls();
			RefreshFilteredDb();
		}
	}

	void SetFilterName(const char *s)
	{
		if (s && *s)
		{
			if (filtName != s)
			{
				filtName = s;
				OnModified();
				RefreshFilteredDb();
			}
		}
		else
		{
			if ( !filtName.empty() )
			{
				filtName.clear();
				OnModified();
				RefreshFilteredDb();
			}
		}
	}

	void SetFilterRating(int n)
	{
		if (filtMinRating != n)
		{
			filtMinRating = n;
			OnModified();
			RefreshFilteredDb();
		}
	}

	void SetFilterPriority(int n)
	{
		if (filtMinPrio != n)
		{
			filtMinPrio = n;
			OnModified();
			RefreshFilteredDb();
		}
	}

	void SetFilterShow(int showflag, int set)
	{
		ASSERT(showflag && !((showflag-1)&showflag));

		if ((!(filtShow & showflag)) != (!set))
		{
			if (set)
				filtShow |= showflag;
			else
				filtShow &= ~showflag;

			OnModified();
			RefreshFilteredDb();
		}
	}

	void SetFilterRelease(int from, int to)
	{
		if (from < -1)
			from = -1;
		if (to < -1)
			to = -1;

		if (from != filtReleaseFrom || to != filtReleaseTo)
		{
			filtReleaseFrom = from;
			filtReleaseTo = to;

			OnFilterReleaseChanged();
			OnModified();
			RefreshFilteredDb();
		}
	}

	void OnFilterReleaseChanged()
	{
		filtReleaseMinTime = 0;
#ifdef _MSC_VER
		filtReleaseMaxTime = 0x7fffffffffffffffLL;
#else
		filtReleaseMaxTime = 0x7fffffffL;
#endif

		if (filtReleaseFrom != -1)
		{
			struct tm t = {0};
			t.tm_year = (filtReleaseFrom>=80 ? 0 : 100) + filtReleaseFrom;
			filtReleaseMinTime = _mkgmtime(&t);
		}

		if (filtReleaseTo != -1)
		{
			struct tm t = {0};
			t.tm_year = (filtReleaseTo>=80 ? 0 : 100) + filtReleaseTo;
			t.tm_mon = 11;
			t.tm_mday = 31;
			t.tm_yday = 365;
			t.tm_hour = 23;
			t.tm_min = 59;
			t.tm_sec = 59;
			filtReleaseMaxTime = _mkgmtime(&t);
		}
	}

	BOOL Write(FILE *f)
	{
		int i;
		if ( !lastfm.empty() ) fprintf(f, "LastFM=%s\n", lastfm.c_str());
		if (returnAfterGame[0] != RET_NEVER) fprintf(f, "DebriefFM=%d\n", returnAfterGame[0]);
		if (returnAfterGame[1] != RET_NEVER) fprintf(f, "DebriefFMEd=%d\n", returnAfterGame[1]);
		if (datefmt) fprintf(f, "DateFormat=%d\n", datefmt);
		if (bWindowMax) fprintf(f, "WindowMaximized=%d\n", 1);
		if (windowpos[0] != POS_UNDEF && windowpos[1] != POS_UNDEF) fprintf(f, "WindowPos=%d,%d\n", windowpos[0], windowpos[1]);
		if (windowsize[0] && windowsize[1]) fprintf(f, "WindowSize=%d,%d\n", windowsize[0], windowsize[1]);
		for (i=0; i<COL_NUM_COLS; i++)
			if (colsizes[i] != -1)
			{
				fprintf(f, "ColumnSizes=%d", colsizes[i]);
				for (i=1; i<COL_NUM_COLS; i++)
					fprintf(f, ",%d", colsizes[i]);
				fprintf(f, "\n");
				break;
			}
		if (colvis == ~0) fprintf(f, "ShowExtColumns=%d\n", 1);
		if (windowsize_taged[0] && windowsize_taged[1]) fprintf(f, "TagEdSize=%d,%d\n", windowsize_taged[0], windowsize_taged[1]);
		if (sortmode != SORT_Name) fprintf(f, "SortMode=%d\n", sortmode);
		if (tagrows != 2) fprintf(f, "TagRows=%d\n", tagrows);
		if (!bVarSizeList) fprintf(f, "VarSizeRows=%d\n", !!bVarSizeList);
		if ( !filtName.empty() ) fprintf(f, "FilterName=%s\n", filtName.c_str());
		if (filtMinRating != -1) fprintf(f, "FilterRating=%d\n", filtMinRating);
		if (filtMinPrio) fprintf(f, "FilterPrio=%d\n", filtMinPrio);
		if (filtShow != FSHOW_Default) fprintf(f, "FilterShow=%X\n", filtShow);
		if (filtShowDefault != FSHOW_Default) fprintf(f, "FilterShowDefault=%X\n", filtShowDefault);
		if (filtReleaseFrom != -1) fprintf(f, "FilterReleaseFrom=%d\n", filtReleaseFrom);
		if (filtReleaseTo != -1) fprintf(f, "FilterReleaseTo=%d\n", filtReleaseTo);
		for (i=0; i<FOP_NUM_OPS; i++)
			if ( !tagFilterList[i].empty() )
			{
				string str = tagFilterList[i][0];
				for (int j=1; j<(int)tagFilterList[i].size(); j++)
				{
					str.append(",");
					str.append(tagFilterList[i][j]);
				}
				fprintf(f, "FilterTags%s=%s\n", FilterOpNames[i], str.c_str());
			}
		if (uitheme) fprintf(f, "Theme=%d\n", uitheme);
		if (bLargeFont) fprintf(f, "FontSize=%d\n", bLargeFont);
		if (uiscale > 4) fprintf(f, "Scale=%d\n", uiscale-4);
		if (bDiffBackups) fprintf(f, "BackupType=%d\n", !!bDiffBackups);
		if (bReviewDiffBackup) fprintf(f, "ReviewDiffBackup=%d\n", bReviewDiffBackup);
		if (bDecompressOGG) fprintf(f, "ConvertOGG=%d\n", bDecompressOGG);
		if (!bGenerateMissFlags) fprintf(f, "GenerateMissFlags=%d\n", bGenerateMissFlags);
		if ( !archiveRepo.empty() ) fprintf(f, "ArchiveRoot=%s\n", archiveRepo.c_str());
		if ( !browserApp.empty() ) fprintf(f, "Browser=%s\n", browserApp.c_str());
		if (!bSaveNewDbEntriesWithFmIni) fprintf(f, "SaveNewWithIni=%d\n", bSaveNewDbEntriesWithFmIni);
		if (!bAutoRefreshFilteredDb) fprintf(f, "AutoRefreshList=%d\n", bAutoRefreshFilteredDb);
		if (!bWrapDescrEditor) fprintf(f, "WrapDescrEdit=%d\n", bWrapDescrEditor);
		if (!bWrapNotesEditor) fprintf(f, "WrapNotesEdit=%d\n", bWrapNotesEditor);
		if (dwLastProcessID) fprintf(f, "LastPID=%d\n", dwLastProcessID);

		return !ferror(f);
	}

	BOOL ParseVal(const char *valname, char *val)
	{
		int i;
		if ( !_stricmp(valname, "LastFM") )
			lastfm = val;
		else if ( !_stricmp(valname, "DebriefFM") )
		{
			returnAfterGame[0] = atoi(val);
			if (returnAfterGame[0] < 0) returnAfterGame[0] = 0;
			else if (returnAfterGame[0] > RET_ALWAYS) returnAfterGame[0] = RET_ALWAYS;
		}
		else if ( !_stricmp(valname, "DebriefFMEd") )
		{
			returnAfterGame[1] = atoi(val);
			if (returnAfterGame[1] < 0) returnAfterGame[1] = 0;
			else if (returnAfterGame[1] > RET_ALWAYS) returnAfterGame[1] = RET_ALWAYS;
		}
		else if ( !_stricmp(valname, "DateFormat") )
		{
			datefmt = atoi(val);
			if (datefmt < 0) datefmt = 0;
			else if (datefmt >= DATEFMT_NUM_FORMATS) datefmt = DATEFMT_NUM_FORMATS-1;
		}
		else if ( !_stricmp(valname, "WindowMaximized") )
			bWindowMax = (atoi(val) == 1);
		else if ( !_stricmp(valname, "WindowPos") )
			sscanf(val, "%d,%d", &windowpos[0], &windowpos[1]);
		else if ( !_stricmp(valname, "WindowSize") )
			sscanf(val, "%d,%d", &windowsize[0], &windowsize[1]);
		else if ( !_stricmp(valname, "ColumnSizes") )
		{
			for (i=0; i<COL_NUM_COLS; i++)
			{
				colsizes[i] = atoi(val);
				val = strchr(val, ',');
				if (!val)
					break;
				val++;
			}
		}
		else if ( !_stricmp(valname, "ShowExtColumns") )
		{
			if ( atoi(val) )
				colvis |= ~0;
		}
		else if ( !_stricmp(valname, "TagEdSize") )
			sscanf(val, "%d,%d", &windowsize_taged[0], &windowsize_taged[1]);
		else if ( !_stricmp(valname, "SortMode") )
		{
			sortmode = atoi(val);
			if (abs(sortmode) <= SORT_None || abs(sortmode) >= SORT_NUM_MODES)
				sortmode = SORT_None+1;
		}
		else if ( !_stricmp(valname, "TagRows") )
		{
			tagrows = atoi(val);
			if (tagrows < 0) tagrows = 0;
			else if (tagrows > 4) tagrows = 4;
		}
		else if ( !_stricmp(valname, "VarSizeRows") )
			bVarSizeList = (atoi(val) > 0);
		else if ( !_stricmp(valname, "FilterName") )
			filtName = val;
		else if ( !_stricmp(valname, "FilterRating") )
		{
			filtMinRating = atoi(val);
			if (filtMinRating < -1) filtMinRating = -1;
			else if (filtMinRating > 10) filtMinRating = 10;
		}
		else if ( !_stricmp(valname, "FilterPrio") )
		{
			filtMinPrio = atoi(val);
			if (filtMinPrio < 0) filtMinPrio = 0;
			else if (filtMinPrio > PRIO_Critical) filtMinPrio = PRIO_Critical;
		}
		else if ( !_stricmp(valname, "FilterShow") )
		{
			filtShow = strtoul(val, NULL, 16);
			filtShow &= FSHOW_All;
		}
		else if ( !_stricmp(valname, "FilterShowDefault") )
		{
			filtShowDefault = strtoul(val, NULL, 16);
			filtShowDefault &= FSHOW_All;
		}
		else if ( !_stricmp(valname, "FilterReleaseFrom") )
		{
			filtReleaseFrom = atoi(val);
			if (filtReleaseFrom < -1) filtReleaseFrom = -1;
			else if (filtReleaseFrom > 99) filtReleaseFrom = 99;
			OnFilterReleaseChanged();
		}
		else if ( !_stricmp(valname, "FilterReleaseTo") )
		{
			filtReleaseTo = atoi(val);
			if (filtReleaseTo < -1) filtReleaseTo = -1;
			else if (filtReleaseTo > 99) filtReleaseTo = 99;
			OnFilterReleaseChanged();
		}
		else if ( !_strnicmp(valname, "FilterTags", 10) )
		{
			int op = -1;
			for (i=0; i<FOP_NUM_OPS; i++)
				if ( !_stricmp(valname+10, FilterOpNames[i]) )
				{
					op = i;
					break;
				}
			if (op == -1)
				goto parse_err;

			while (val && *val)
			{
				char *next = strchr(val, ',');
				if (next)
					*next++ = 0;

				if ( CleanTag(val, TRUE) )
					AddTagFilter(val, op, TRUE);

				val = next;
			}
		}
		else if ( !_stricmp(valname, "Theme") )
			uitheme = atoi(val);
		else if ( !_stricmp(valname, "FontSize") )
			bLargeFont = !!atoi(val);
		else if ( !_stricmp(valname, "Scale") )
			uiscale = atoi(val)+4;
		else if ( !_stricmp(valname, "BackupType") )
			bDiffBackups = !!atoi(val);
		else if ( !_stricmp(valname, "ReviewDiffBackup") )
			bReviewDiffBackup = !!atoi(val);
		else if ( !_stricmp(valname, "ConvertOGG") )
			bDecompressOGG = !!atoi(val);
		else if ( !_stricmp(valname, "GenerateMissFlags") )
			bGenerateMissFlags = !!atoi(val);
		else if ( !_stricmp(valname, "ArchiveRoot") )
		{
			archiveRepo = Trimmed(val);
			TrimTrailingSlashFromPath(archiveRepo);
		}
		else if ( !_stricmp(valname, "Browser") )
			browserApp = val;
		else if ( !_stricmp(valname, "SaveNewWithIni") )
			bSaveNewDbEntriesWithFmIni = !!atoi(val);
		else if ( !_stricmp(valname, "AutoRefreshList") )
			bAutoRefreshFilteredDb = !!atoi(val);
		else if ( !_stricmp(valname, "WrapDescrEdit") )
			bWrapDescrEditor = !!atoi(val);
		else if ( !_stricmp(valname, "WrapNotesEdit") )
			bWrapNotesEditor = !!atoi(val);
		else if ( !_stricmp(valname, "LastPID") )
			dwLastProcessID = atoi(val);
		else
		{
parse_err:
			TRACE("ParseVal: unknown val name: %s", valname);
			ASSERT(FALSE);
			return FALSE;
		}

		return TRUE;
	}
};


static FMSelConfig g_cfg;


/////////////////////////////////////////////////////////////////////
// DB

static __inline bool compare_tags(const char *a, const char *b)
{
	// category tags first
	if ( strchr(a, ':') )
	{
		if ( !strchr(b, ':') )
			return true;
	}
	else if ( strchr(b, ':') )
		return false;

	return fl_utf_strcasecmp(a, b) < 0;
}


struct FMEntry
{
	enum
	{
		FLAG_UnverifiedRelDate	= (1<<0),	// automatically determined release date (not quite accurate but better than nothing)
		FLAG_NoInfoFile			= (1<<1),	// set if we have scanned this FM for info file but found none

		FLAG_CachedInfoFiles	= (1<<26),	// infoFilesCache is valid
		FLAG_ArchiveUnverified	= (1<<27),	// temp flag during loading/init that indicates that 'archive' hasn't been verified yet
		FLAG_PendingInfoFile	= (1<<28),	// info file has been soft-changed (changed without changing modified status)
		FLAG_Archived			= (1<<29),	// archived FM is available
		FLAG_Installed			= (1<<30),	// FM dir is present
		FLAG_UnmodifiedNew		= (1<<31),	// entry was automatically created and has not been modified (don't save)

		// flags that aren't saved
		FLAG_NoSaveMask			= FLAG_Installed|FLAG_UnmodifiedNew|FLAG_Archived|FLAG_PendingInfoFile
									|FLAG_ArchiveUnverified|FLAG_CachedInfoFiles,
	};

	enum
	{
		STATUS_NotPlayed,
		STATUS_Completed,	// a completed mission can change to InProgress when replayed
		STATUS_InProgress,
	};

public:
	char name[32];			// dir name (30 is max storage name len supported by resource manager in dark)
	string name_utf;		// 'name' converted to UTF
	string nicename;		// optional nicely formatted name displayed in list
	string archive;			// optional archive filename
	unsigned int flags;
	int status;
	time_t tmReleaseDate;
	time_t tmLastStarted;
	time_t tmLastCompleted;
	int nCompleteCount;
	int rating;				// 0 to 10, -1 for unrated
	int priority;			// priority level can be assigned to maintain a ranked todo list
	string tags;			// comma separated tag list (no space after comma)
	string notes;
	string modexclude;		// mod/ubermod exclude paths (+ separated like the mod_path config var)
	string infofile;		// filename of FMs info/readme
	string descr;			// mission description/summary (unlike the info file this is handled and displayed natively)

	string filtername;		// lower case version of GetFriendlyName used for filtering and sorting
	vector<const char*> taglist;// individual tags extracted from 'tags' and alphabetically sorted
	string tagsUI;			// 'tags' pre-formatted for list control drawing

	vector<string> infoFilesCache;// cached info file list for archived FM

protected:
	void DestroyTaglist()
	{
		for (int i=0; i<(int)taglist.size(); i++)
			free((void*)taglist[i]);
		taglist.clear();
	}

public:
	FMEntry()
	{
		memset(name, 0, sizeof(name));
		flags = FLAG_UnmodifiedNew;
		status = STATUS_NotPlayed;
		tmReleaseDate = 0;
		tmLastStarted = 0;
		tmLastCompleted = 0;
		nCompleteCount = 0;
		rating = -1;
		priority = 0;
	}

	~FMEntry()
	{
		DestroyTaglist();
	}

	void InitName(const char *s)
	{
		strcpy_safe(name, sizeof(name), s);

		name_utf = ToUTF(s);
	}

	const char* GetFriendlyName() const
	{
		return nicename.empty() ? name_utf.c_str() : nicename.c_str();
	}

	const char* GetFilterName() const
	{
		return filtername.c_str();
	}

	string GetArchiveFilePath() const
	{
		ASSERT(!g_cfg.archiveRepo.empty() && !archive.empty());

		string s = g_cfg.archiveRepo + DIRSEP + archive;
		CleanDirSlashes(s);
		return s;
	}

	string GetBakArchiveFilePath() const
	{
		string s = GetArchiveFilePath();
		string::size_type next = s.find_last_of('.');
		if (next != string::npos)
			s = s.substr(0, next);
		s += ".FMSelBak.zip";
		return s;
	}

	// return archive name without sub-dir
	const char* GetArchiveNameOnly() const
	{
		// strip subdir from archive name
		const char *s = archive.c_str();
		const char *name = strrchr(s, *DIRSEP);
		return name ? name : s;
	}

	BOOL IsAvail() const { return flags & (FLAG_Installed|FLAG_Archived); }
	BOOL IsInstalled() const { return flags & FLAG_Installed; }
	BOOL IsArchived() const { return flags & FLAG_Archived; }

	void OnModified()
	{
		flags &= ~(FLAG_UnmodifiedNew|FLAG_PendingInfoFile);
		g_bDbModified = TRUE;
	}

	void OnStart(BOOL bSetInProgress = FALSE)
	{
		OnModified();

		if (bSetInProgress)
			SetStatus(STATUS_InProgress);
		time(&tmLastStarted);

		g_cfg.SetLastFM(name);
	}

	void OnCompleted()
	{
		OnModified();

		status = STATUS_Completed;
		nCompleteCount++;
		time(&tmLastCompleted);
	}

	void SetStatus(int n)
	{
		if (status != n)
		{
			OnModified();
			status = n;
		}
	}

	void SetRating(int n)
	{
		if (rating != n)
		{
			OnModified();
			rating = n;
		}
	}

	void SetPriority(int n)
	{
		if (priority != n)
		{
			OnModified();
			priority = n;
		}
	}

	void SetNiceName(const char *s)
	{
		if ((!s && !nicename.empty()) || nicename != s)
		{
			OnModified();

			if (!s || !*s)
				nicename.clear();
			else
				nicename = s;

			OnUpdateName();
		}
	}

	void SetTags(const char *s)
	{
		if ((!s && !tags.empty()) || tags != s)
		{
			OnModified();

			if (!s || !*s)
				tags.clear();
			else
				tags = s;

			OnUpdatedTags();

			InvalidateTagDb();
		}
	}

	void SetNotes(const char *s)
	{
		if ((!s && !notes.empty()) || notes != s)
		{
			OnModified();

			if (!s || !*s)
				notes.clear();
			else
				notes = s;
		}
	}

	void SetDescr(const char *s)
	{
		if ((!s && !descr.empty()) || descr != s)
		{
			OnModified();

			if (!s || !*s)
				descr.clear();
			else
				descr = s;
		}
	}

	void SetModExclude(const char *s)
	{
		if ((!s && !modexclude.empty()) || modexclude != s)
		{
			OnModified();

			if (!s || !*s)
				modexclude.clear();
			else
				modexclude = s;
		}
	}

	void OnUpdateName()
	{
		char s[512*3];
		tolower_utf(GetFriendlyName(), sizeof(s), s);
		filtername = s;
	}

	void OnUpdatedTags(BOOL bOnLoad = FALSE)
	{
		DestroyTaglist();
		tagsUI.clear();

		if ( !tags.empty() )
		{
			const char *s = tags.c_str();
			for (;;)
			{
				const char *next = strchr(s, ',');
				if (next)
				{
					*(char*)next = 0;
					char *tag = _strdup(s);
					*(char*)next = ',';
					s = next + 1;

					if (!bOnLoad || CleanTag(tag, FALSE))
						taglist.push_back(tag);
				}
				else
				{
					char *tag = _strdup(s);

					if (!bOnLoad || CleanTag(tag, FALSE))
						taglist.push_back(tag);
					break;
				}
			}

			// pre-build tag string used for drawing FM list (sorting is knowingly/intentionally still based
			// on the non-localized names here for now, as it would require changes in other places as well
			// in order to be consistent across the board)
			if ( !taglist.empty() )
			{
				std::sort(taglist.begin(), taglist.end(), compare_tags);

				tagsUI.reserve(tags.size() + taglist.size() + 6);
				tagsUI = TAGS_LABEL;
				tagsUI += " ";
				for (int i=0; i<(int)taglist.size(); i++)
				{
					if (i)
						tagsUI.append(", ");
#ifdef LOCALIZATION_SUPPORT
					// tags with category need special localization handling
					if (localization_supported && strchr(taglist[i], ':'))
					{
						ASSERT(localization_initialized);

						// split category and tag into two strings and localize each
						char loctag[2048];
						const char *colon = strchr(taglist[i], ':');
						*(char*)colon = 0;
						if (colon[1])
							sprintf(loctag, "%s:%s", $tag(taglist[i]), $tag(colon+1));
						else
							sprintf(loctag, "%s:", $tag(taglist[i]));
						*(char*)colon = ':';

						tagsUI.append(loctag);
					}
					else
#endif
					tagsUI.append( $tag(taglist[i]) );
				}
			}
			else
				tagsUI.clear();
		}
	}

	BOOL HasTag(const char *tag)
	{
		for (int i=0; i<(int)taglist.size(); i++)
			if ( !fl_utf_strcasecmp(tag, taglist[i]) )
				return TRUE;

		return FALSE;
	}

	// write subset of vals relevant for FM.INI (enforce DOS linebreaks for fm.ini, file opened in binary mode)
	BOOL WriteIni(FILE *f)
	{
		if ( !nicename.empty() ) fprintf(f, "NiceName=%s\r\n", nicename.c_str());
		if (tmReleaseDate) fprintf(f, "ReleaseDate=%" DATFMT "\r\n", tmReleaseDate);
		if ( !infofile.empty() ) fprintf(f, "InfoFile=%s\r\n", infofile.c_str());
		if ( !tags.empty() ) fprintf(f, "Tags=%s\r\n", tags.c_str());
		if ( !descr.empty() ) fprintf(f, "Descr=%s\r\n", descr.c_str());

		return !ferror(f);
	}

	BOOL Write(FILE *f)
	{
		if ( !nicename.empty() ) fprintf(f, "NiceName=%s\n", nicename.c_str());
		if ( !archive.empty() ) fprintf(f, "Archive=%s\n", archive.c_str());
		if (flags & ~FLAG_NoSaveMask) fprintf(f, "Flags=%X\n", flags & ~FLAG_NoSaveMask);
		if (status != STATUS_NotPlayed) fprintf(f, "Status=%d\n", status);
		if (tmReleaseDate) fprintf(f, "ReleaseDate=%" DATFMT "\n", tmReleaseDate);
		if (tmLastStarted) fprintf(f, "LastStarted=%" DATFMT "\n", tmLastStarted);
		if (tmLastCompleted) fprintf(f, "LastCompleted=%" DATFMT "\n", tmLastCompleted);
		if (nCompleteCount) fprintf(f, "Completed=%d\n", nCompleteCount);
		if (rating != -1) fprintf(f, "Rating=%d\n", rating);
		if (priority) fprintf(f, "Priority=%d\n", priority);
		if ( !infofile.empty() ) fprintf(f, "InfoFile=%s\n", infofile.c_str());
		if ( !modexclude.empty() ) fprintf(f, "ModExclude=%s\n", modexclude.c_str());
		if ( !tags.empty() ) fprintf(f, "Tags=%s\n", tags.c_str());
		if ( !descr.empty() ) fprintf(f, "Descr=%s\n", descr.c_str());
		if ( !notes.empty() ) fprintf(f, "Notes=%s\n", notes.c_str());

		return !ferror(f);
	}

	BOOL ParseVal(const char *valname, char *val)
	{
		if ( !_stricmp(valname, "NiceName") )
		{
			nicename = val;
			OnUpdateName();
		}
		else if ( !_stricmp(valname, "Archive") )
		{
			archive = val;
			CleanDirSlashes(archive);
			flags |= FLAG_ArchiveUnverified;
		}
		else if ( !_stricmp(valname, "Flags") )
			flags = (strtoul(val, NULL, 16) & ~FLAG_NoSaveMask) | (flags & FLAG_NoSaveMask);
		else if ( !_stricmp(valname, "Status") )
		{
			status = atoi(val);
			if (status < 0) status = 0;
			else if (status > STATUS_InProgress) status = STATUS_InProgress;
		}
		else if ( !_stricmp(valname, "ReleaseDate") )
			tmReleaseDate = _strtoui64(val, NULL, 16);
		else if ( !_stricmp(valname, "LastStarted") )
			tmLastStarted = _strtoui64(val, NULL, 16);
		else if ( !_stricmp(valname, "LastCompleted") )
			tmLastCompleted = _strtoui64(val, NULL, 16);
		else if ( !_stricmp(valname, "Completed") )
			nCompleteCount = atoi(val);
		else if ( !_stricmp(valname, "Rating") )
		{
			rating = atoi(val);
			if (rating < -1) rating = -1;
			else if (rating > 10) rating = 10;
		}
		else if ( !_stricmp(valname, "Priority") )
		{
			priority = atoi(val);
			if (priority < 0) priority = 0;
			else if (priority > PRIO_Critical) priority = PRIO_Critical;
		}
		else if ( !_stricmp(valname, "InfoFile") )
			infofile = val;
		else if ( !_stricmp(valname, "Tags") )
		{
			tags = val;
			OnUpdatedTags(TRUE);
		}
		else if ( !_stricmp(valname, "Notes") )
			notes = val;
		else if ( !_stricmp(valname, "Descr") )
			descr = val;
		else if ( !_stricmp(valname, "ModExclude") )
			modexclude = val;
		else
		{
			TRACE("ParseVal: unknown val name: %s (FM %s)", valname, name);
			ASSERT(FALSE);
			return FALSE;
		}

		return TRUE;
	}
};

// case insensitive string based hash key
typedef string tIStrHashKey;

static __inline tIStrHashKey KEY(const char *s)
{
	tIStrHashKey k(s);
	std::transform(k.begin(), k.end(), k.begin(), ::tolower);
	return k;
}

static __inline tIStrHashKey KEY_UTF(const char *s)
{
	char buf[1024*3];
	tolower_utf(s, sizeof(buf), buf);

	return tIStrHashKey(buf);
}

// filname based key (case insensitive on Windows, otherwise case sensitive)
#ifdef _WIN32
#define FKEY(_x) KEY(_x)
#else
#define FKEY(_x) _x
#endif

// override default hash calc for better hash search performance (original algo doesn't include all characters
// in hash calc if string is longer than 10 chars, which reduces uniqueness)
#if __cplusplus >= 201103L
struct KeyHash
#else
struct KeyHash : public std::unary_function<tIStrHashKey, size_t>
#endif
{
	size_t operator()(const tIStrHashKey& _Keyval) const
	{
		size_t _Val = 2166136261U;
		size_t _First = 0;
		size_t _Last = _Keyval.size();
		for(; _First < _Last; _First++)
			_Val = 16777619U * _Val ^ (size_t)_Keyval[_First];
		return _Val;
	}
};

typedef unordered_map<tIStrHashKey, FMEntry*, KeyHash> tFMHash;

static vector<FMEntry*> g_db;
static tFMHash g_dbHash;

// lookup db based on archive names for installed FMs prior to existing archives being scanned
// (db is cleared once scan is completed)
static tFMHash g_dbUnverifiedArchiveHash;

static vector<FMEntry*> g_dbFiltered;

// list of FM dirs that were ignored becuase they had names that dark couldn't handle (ie. too long)
// may contain entries after calling ScanFmDir
static vector<string> g_invalidDirs;


static FMEntry* GetFM(const tIStrHashKey &key)
{
	tFMHash::iterator dataIter = g_dbHash.find(key);
	if (dataIter != g_dbHash.end())
		return dataIter->second;

	return NULL;
}

static __inline FMEntry* GetFM(const char *name)
{
	return GetFM( KEY(name) );
}

static int GetDbIndex(FMEntry *fm)
{
	for (int i=0; i<(int)g_db.size(); i++)
		if (g_db[i] == fm)
			return i;

	return -1;
}


static FMEntry* GetFMByUnverifiedArchive(const tIStrHashKey &key)
{
	tFMHash::iterator dataIter = g_dbUnverifiedArchiveHash.find(key);
	if (dataIter != g_dbUnverifiedArchiveHash.end())
		return dataIter->second;

	return NULL;
}


static FMEntry* FindFmFromArchiveNameOnly(const char *archname)
{
	for (int i=0; i<(int)g_db.size(); i++)
	{
		FMEntry *fm = g_db[i];

		if (fm->IsArchived() && !_stricmp(fm->GetArchiveNameOnly(), archname))
			return fm;
	}

	return NULL;
}


static BOOL SaveInstallInfo(FMEntry *fm, const char *path = NULL)
{
	char fname[MAX_PATH_BUF];
	int r;
	if (path)
		r = _snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "fmsel.inf", path);
	else
		r = _snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "fmsel.inf", g_pFMSelData->sRootPath, fm->name);

	if (r == -1)
		return FALSE;

	remove_forced(fname);

	FILE *f = fopen(fname, "wb");
	if (!f)
	{
		TRACE("failed to open fmsel.inf for writing %d", errno);
		return FALSE;
	}

	fprintf(f, "Name=%s\r\n", fm->name);
	fprintf(f, "Archive=%s\r\n", fm->archive.c_str());

	fclose(f);

	return TRUE;
}

// returns TRUE if file was found and fm name matched
static BOOL LoadInstallInfo(FMEntry *fm)
{
	// the install info file contains the source archive from this FM was installed, even though that information
	// would also be available in the db, this method was chosen to ensure a persistent way to store this info even
	// if the db was deleted or lost

	char fname[MAX_PATH_BUF];
	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "fmsel.inf", g_pFMSelData->sRootPath, fm->name) == -1)
		return FALSE;

	FILE *f = fopen(fname, "rb");
	if (!f)
		return FALSE;

	// read file into buffer

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
		return FALSE;
	}

	char *data = new char[n+2];
	data[n] = 0;
	data[n+1] = 0;

	fread(data, 1, n, f);
	fclose(f);

	// parse file

	BOOL bRet = FALSE;

	char *s = data;
	while (*s)
	{
		char *nextline = next_line(s);

		char *val = strchr(s, '=');
		if (!val)
		{
			TRACE("parse error, unknown token: %s", s);
			ASSERT(FALSE);
			s = nextline;
			continue;
		}

		*val++ = 0;
		val = skip_ws(val);
		if (!*val)
		{
			s = nextline;
			continue;
		}

		if ( !_stricmp(s, "Archive") )
		{
			// archive from which this FM was installed from

			fm->archive = val;
			CleanDirSlashes(fm->archive);
			fm->flags |= FMEntry::FLAG_ArchiveUnverified;
		}
		else if ( !_stricmp(s, "Name") )
		{
			// dir name which this FM was installed in, should always be the first param (used as verification to prevent
			// an FM dir, that was copied containing an fmsel.inf, as being identified as a "genuine" install)
			if ( strcmp(val, fm->name) )
			{
				// this install info is bogus, remove it and abort
				remove_forced(fname);
				break;
			}
			else
				bRet = TRUE;
		}

		s = nextline;
	}

	//

	delete[] data;

	return bRet;
}

static BOOL LoadRemoveFileInfo(FMEntry *fm)
{
	// this loads and processes the special install info file that differential backups may contain
	// to list files that should be removed from install

	char installdir[MAX_PATH_BUF];
	if (_snprintf_s(installdir, sizeof(installdir), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
		return FALSE;

	char fname[MAX_PATH_BUF];
	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "fmsel.inf", installdir) == -1)
		return FALSE;

	FILE *f = fopen(fname, "rb");
	if (!f)
		return FALSE;

	// read file into buffer

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
		return FALSE;
	}

	char *data = new char[n+2];
	data[n] = 0;
	data[n+1] = 0;

	fread(data, 1, n, f);
	fclose(f);

	// parse file

	BOOL bRet = FALSE;

	char *s = data;
	while (*s)
	{
		char *nextline = next_line(s);

		char *val = strchr(s, '=');
		if (!val)
		{
			TRACE("parse error, unknown token: %s", s);
			ASSERT(FALSE);
			s = nextline;
			continue;
		}

		*val++ = 0;
		val = skip_ws(val);
		if (!*val)
		{
			s = nextline;
			continue;
		}

		if ( !_stricmp(s, "RemoveFile") )
		{
			// remove file from install

			CleanDirSlashes(val);

			// ensure that filename does not contain anything malicious (absolute paths, dot-dot paths etc.)
			if (*val && val[0] != *DIRSEP && val[strlen(val)-1] != *DIRSEP
				&& !strchr(val, ':')
				&& !strstr(val, DIRSEP"..") && strncmp(val, "..", 2))
			{
				if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s", installdir, val) == -1)
				{
					TRACE("remove list failed to remove file (path too long): %s", fname);
				}
				else if ( remove_forced(fname) )
				{
					TRACE("remove list failed to remove file: %s", fname);
				}
			}
		}

		s = nextline;
	}

	//

	delete[] data;

	return bRet;
}


static void GenerateArchiveInstallName(const char *name, FMEntry *fm)
{
	// generate install dir name from archive name, must be unique and not exceed 30 characters

	// extract file title
	char ftitle[MAX_PATH_BUF];
	const char *namestart = strrchr(name, *DIRSEP);
	namestart = namestart ? namestart+1 : name;
	strcpy_safe(ftitle, sizeof(ftitle), namestart);
	*strrchr(ftitle, '.') = 0;

	// replace undesired chars with underscore (replace aggressively for cross-platform safety)
	const char *illegal_chars = "+;:.,<>?*~| ";
	char *s = ftitle;
	for (; *s; s++)
		if ( strchr(illegal_chars, *s) )
			*s = '_';

	int n = strlen(ftitle);
	if (n > 30)
		ftitle[n=30] = 0;

	// check of uniqueness
	if ( GetFM(ftitle) )
	{
		// not unique, append "(<number>)" with increasing numbers until unique is found
		char altname[32];
		char seq[8];

#ifdef _WIN32
		for (int i=0; ; i++)
		{
			sprintf(seq, "(%d)", i+2);
#else
		for (unsigned short i=0; ; i++)
		{
			sprintf(seq, "(%u)", i+2);
#endif
			int l = strlen(seq);
			if (n+l > 30)
			{
				strcpy(altname, ftitle);
				altname[30-l] = 0;
				strcat(altname, seq);
			}
			else
				sprintf(altname, "%s%s", ftitle, seq);

			if ( !GetFM(altname) )
			{
				fm->InitName(altname);
				return;
			}
		}
	}

	fm->InitName(ftitle);
}

static void EnumFmArchive(const char *name_raw)
{
	string name = name_raw;
	CleanDirSlashes(name);

	FMEntry *fm = GetFMByUnverifiedArchive(KEY( name.c_str() ));
	if (!fm)
	{
		// new found entry (or rather not previously saved in db)

		fm = new FMEntry;

		GenerateArchiveInstallName(name.c_str(), fm);
		fm->flags |= FMEntry::FLAG_Archived;
		fm->archive = name;

		ApplyFmIni(fm, g_bRunningShock);
		AutoScanReleaseDate(fm, TRUE);

		fm->OnUpdateName();

		g_db.push_back(fm);
		g_dbHash[KEY(fm->name)] = fm;
	}
	else
	{
		fm->flags |= FMEntry::FLAG_Archived;
		fm->archive = name;
	}

	fm->flags &= ~FMEntry::FLAG_ArchiveUnverified;
}

static void ScanArchiveRepo(const char *subdirname = NULL, int depth = 0)
{
	// 'depth' is a dumb infinite recursion stopper, in case file system contains cyclic hard-links
	if (!g_cfg.bRepoOK || depth > 99)
		return;

	// recursively scan repo dir

	string sdir;
	if (subdirname)
	{
		sdir = g_cfg.archiveRepo;
		sdir.append("/");
		sdir.append(subdirname);
	}

	dirent **files;
	int nFiles = fl_filename_list(subdirname ? sdir.c_str() : g_cfg.archiveRepo.c_str(), &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		if (f->d_name[0] != '.')
		{
			int len = strlen(f->d_name);
			if ( isdirsep(f->d_name[len-1]) )
			{
				// recurse into subdir
				if (subdirname)
				{
					sdir = subdirname;
					sdir.append(f->d_name);

					ScanArchiveRepo(sdir.c_str(), depth+1);
				}
				else
					ScanArchiveRepo(f->d_name, depth+1);
			}
			else
			{
				// check for supported archive format (and that archive isn't a savegame backup)
				const char *ext = strrchr(f->d_name, '.');
				if (ext && IsArchiveFormatSupported(ext+1) && !stristr(f->d_name, ".fmselbak."))
				{
					int k = len - 1;
					while (k >= 0 && !isdirsep(f->d_name[k])) k--;
					k++;
					const char *name = f->d_name+k;

					// prepend subdir name if inside one
					if (!subdirname)
						EnumFmArchive(name);
					else
					{
						sdir = subdirname;
						sdir.append(f->d_name);

						EnumFmArchive( sdir.c_str() );
					}
				}
			}
		}
	}

	fl_filename_free_list(&files, nFiles);
}

static void EnumFmDir(const char *name)
{
	if (strlen(name) > 30)
	{
		TRACE("skipping dir with too long name: %s", name);
		g_invalidDirs.push_back(name);
		return;
	}

	const tIStrHashKey key = KEY(name);
	FMEntry *fm = GetFM(key);
	if (!fm)
	{
		// new found entry (or rather not previously saved in db)

		fm = new FMEntry;

		fm->InitName(name);
		fm->flags |= FMEntry::FLAG_Installed;

		ApplyFmIni(fm, g_bRunningShock);
		AutoScanReleaseDate(fm, TRUE);

		fm->OnUpdateName();

		g_db.push_back(fm);
		g_dbHash[key] = fm;
	}
	else
		fm->flags |= FMEntry::FLAG_Installed;

	LoadInstallInfo(fm);

	if (fm->flags & FMEntry::FLAG_ArchiveUnverified)
	{
		const tIStrHashKey key = KEY( fm->archive.c_str() );
		// if multiple FMs reference the same archive then we're screwed, the last one encountered will be the chosen one
		g_dbUnverifiedArchiveHash[key] = fm;
	}
}

static void ScanFmDir()
{
	g_invalidDirs.clear();

	// scan for installed FMs

	dirent **files;
	int nFiles = fl_filename_list(g_pFMSelData->sRootPath, &files, NO_COMP_UTFCONV);
	if (nFiles > 0)
	{
		for (int i=0; i<nFiles; i++)
		{
			dirent *f = files[i];

			int len = strlen(f->d_name);
			if (isdirsep(f->d_name[len-1]) && f->d_name[0] != '.')
			{
				// extract FM dir name
				f->d_name[len-1] = 0;
				int k = len - 2;
				while (k >= 0 && !isdirsep(f->d_name[k])) k--;
				k++;
				const char *name = f->d_name+k;

				EnumFmDir(name);
			}
		}

		fl_filename_free_list(&files, nFiles);
	}

	// add all non-installed db entries with archive defined to archive hash (so ScanArchiveRepo can find them)
	for (int i=0; i<(int)g_db.size(); i++)
	{
		FMEntry *fm = g_db[i];
		if ((fm->flags & FMEntry::FLAG_ArchiveUnverified) && !fm->IsInstalled())
			g_dbUnverifiedArchiveHash[KEY( fm->archive.c_str() )] = fm;
	}

	// scan for archived FMs
	ScanArchiveRepo();

	g_dbUnverifiedArchiveHash.clear();
}


static BOOL SaveDb()
{
	if (!g_bDbModified)
		return TRUE;

	char fname[MAX_PATH_BUF];
	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "fmsel.ini", g_pFMSelData->sRootPath) == -1)
		return FALSE;

	char bakfname[MAX_PATH_BUF];
	_snprintf_s(bakfname, sizeof(bakfname), _TRUNCATE, "%s" DIRSEP "fmsel.bak", g_pFMSelData->sRootPath);

	remove_forced(bakfname);
	rename(fname, bakfname);

	FILE *f = fopen(fname, "w");
	if (!f)
	{
		ASSERT(FALSE);
		rename(bakfname, fname);
		return FALSE;
	}

	// save config
	fprintf(f, "[Config]\n");
	if ( !g_cfg.Write(f) )
	{
save_failed:
		fclose(f);

		ASSERT(FALSE);

		// remove faulty file and restore backup
		remove_forced(fname);
		rename(bakfname, fname);

		return FALSE;
	}
	fprintf(f, "\n");

	// save db
	for (unsigned int i=0; i<g_db.size(); i++)
	{
		FMEntry *fm = g_db[i];

		if (fm->flags & FMEntry::FLAG_UnmodifiedNew)
			continue;

		fprintf(f, "[FM=%s]\n", fm->name);

		if ( !fm->Write(f) )
			goto save_failed;

		if (fprintf(f, "\n") <= 0)
			goto save_failed;
	}

	fclose(f);

	g_bDbModified = FALSE;

	return TRUE;
}

static BOOL LoadDb()
{
	g_db.reserve(2048);

	char fname[MAX_PATH_BUF];
	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "fmsel.ini", g_pFMSelData->sRootPath) == -1)
		return FALSE;

	FILE *f = fopen(fname, "r");
	if (!f)
		return 2;

	char line[8192];
	int state = 0;
	FMEntry *fm = NULL;

	for (;;)
	{
		if ( !fgets(line, sizeof(line)-1, f) )
			break;

		line[sizeof(line)-1] = 0;
		int len = strlen(line);
		while (len && isspace_(line[len-1]))
		{
			len--;
			line[len] = 0;
		}

		if (!len)
			continue;

		char *s = line;
		while ( isspace_(*s) )
		{
			s++;
			len--;
		}

		ASSERT(len > 0);

		if (*s == ';')
			continue;

		if (state)
		{
			if (*s == '[')
			{
				fm = NULL;
				goto new_block;
			}

			char *q = strchr(s, '=');
			if (!q)
			{
				TRACE("parse error, unknown token: %s", s);
				ASSERT(FALSE);
				continue;
			}

			if (!q[1])
				continue;

			*q = 0;
			char *val = q + 1;

			if (state == 1)
			{
				// [Config]
				g_cfg.ParseVal(s, val);
			}
			else //if (state == 2)
			{
				// [FM=...]
				ASSERT(fm != NULL);
				fm->ParseVal(s, val);
			}
		}
		else
		{
new_block:
			if ( !_stricmp(s, "[Config]") )
			{
				state = 1;
			}
			else if ( !_strnicmp(s, "[FM=", 4) )
			{
				if (s[len-1] != ']')
				{
					TRACE("parse error, unknown token: %s", s);
					ASSERT(FALSE);
					state = 0;
					continue;
				}

				s[len-1] = 0;
				len -= 4+1;
				if (len <= 0)
				{
					TRACE("FM name missing");
					ASSERT(FALSE);
					state = 0;
					continue;
				}
				else if (len > 30)
				{
					TRACE("FM name too long: %s", s);
					ASSERT(FALSE);
					state = 0;
					continue;
				}

				fm = new FMEntry;
				fm->InitName(s+4);
				fm->flags &= ~FMEntry::FLAG_UnmodifiedNew;
				g_db.push_back(fm);
				g_dbHash[KEY(fm->name)] = fm;

				state = 2;
			}
			else
			{
				TRACE("parse error, unknown token: %s", s);
				ASSERT(FALSE);
				state = 0;
			}
		}
	}

	fclose(f);

	// post-load processing
	for (int i=0; i<(int)g_db.size(); i++)
		g_db[i]->OnUpdateName();

	return TRUE;
}


static void FormatDbDate(time_t t, char *s, int bufsize, BOOL bLocal, int fmt)
{
	struct tm *tm = bLocal ? localtime(&t) : gmtime(&t);
	strftime(s, bufsize, g_DateFmt[fmt], tm);
}

static void FormatDbDate(time_t t, char *s, int bufsize, BOOL bLocal)
{
	FormatDbDate(t, s, bufsize, bLocal, g_cfg.datefmt);
}


/////////////////////////////////////////////////////////////////////
// DB Filtering and Sorting

// FALSE when tag DBs need to be rebuilt
static BOOL g_bTagDbValid = FALSE;

// db that contains counts of how many FMs have a particular tag assigned (also countains countains for entire categories
// where the key is in "cat:" format, so for example "author:")
typedef unordered_map<tIStrHashKey, int, KeyHash> tTagCountHash;
static tTagCountHash g_dbTagCountHash;

#ifdef USE_TAG_FM_XREF_DB
// tag -> fm db
typedef unordered_multimap<tIStrHashKey, FMEntry*, KeyHash> tTagHash;
static tTagHash g_dbTagHash;
#endif

// alphabetically sorted lists of tags use to display global tag lists and do auto-completion

// list of tag names with categories (ie. "cat:val" format) and the stand alone category names too
static vector<const char*> g_tagNameList;
// list of tag names without categories
static vector<const char*> g_tagNameListNoCat;

// db that contains tag filters that are currently added to filter list (for quick lookups if a tag filter is added or not)
typedef unordered_map<tIStrHashKey, char, KeyHash> tTagFilterHash;
static tTagFilterHash g_dbTagFilterHash;

//

static void DestroyTagNamelists()
{
	for (int i=0; i<(int)g_tagNameList.size(); i++)
		free((void*)g_tagNameList[i]);
	g_tagNameList.clear();

	for (int i=0; i<(int)g_tagNameListNoCat.size(); i++)
		free((void*)g_tagNameListNoCat[i]);
	g_tagNameListNoCat.clear();
}


static __inline bool sort_name(FMEntry *a, FMEntry *b)
{
	return strcmp(a->GetFilterName(), b->GetFilterName()) < 0;
}
static __inline bool sort_rating(FMEntry *a, FMEntry *b)
{
	return (a->rating == b->rating) ? sort_name(a, b) : (a->rating < b->rating);
}
static __inline bool sort_prio(FMEntry *a, FMEntry *b)
{
	return (a->priority == b->priority) ? sort_name(a, b) : (a->priority < b->priority);
}
static __inline bool sort_status(FMEntry *a, FMEntry *b)
{
	return (a->status == b->status)
		? ((a->nCompleteCount == b->nCompleteCount) ? sort_name(a, b) : (a->nCompleteCount < b->nCompleteCount))
		: (a->status < b->status);
}
static __inline bool sort_lastplayed(FMEntry *a, FMEntry *b)
{
	return (a->tmLastStarted == b->tmLastStarted) ? sort_name(a, b) : (a->tmLastStarted < b->tmLastStarted);
}
static __inline bool sort_released(FMEntry *a, FMEntry *b)
{
	return (a->tmReleaseDate == b->tmReleaseDate) ? sort_name(a, b) : (a->tmReleaseDate < b->tmReleaseDate);
}
static __inline bool sort_dirname(FMEntry *a, FMEntry *b)
{
	return _stricmp(a->name, b->name) < 0;
}
static __inline bool sort_archive(FMEntry *a, FMEntry *b)
{
	if ( !a->IsArchived() )
	{
		if ( !b->IsArchived() )
			return sort_name(a, b);
		else
			return false;
	}
	else if ( !b->IsArchived() )
			return true;

	return _stricmp(a->archive.c_str(), b->archive.c_str()) < 0;
}

static __inline bool sort_rev_name(FMEntry *a, FMEntry *b)
{
	return sort_name(b, a);
}
static __inline bool sort_rev_rating(FMEntry *a, FMEntry *b)
{
	return (a->rating == b->rating) ? sort_name(a, b) : (b->rating < a->rating);
}
static __inline bool sort_rev_prio(FMEntry *a, FMEntry *b)
{
	return (a->priority == b->priority) ? sort_name(a, b) : (b->priority < a->priority);
}
static __inline bool sort_rev_status(FMEntry *a, FMEntry *b)
{
	return (a->status == b->status)
		? ((a->nCompleteCount == b->nCompleteCount) ? sort_name(a, b) : (b->nCompleteCount < a->nCompleteCount))
		: (b->status < a->status);
}
static __inline bool sort_rev_lastplayed(FMEntry *a, FMEntry *b)
{
	return (a->tmLastStarted == b->tmLastStarted) ? sort_name(a, b) : (b->tmLastStarted < a->tmLastStarted);
}
static __inline bool sort_rev_released(FMEntry *a, FMEntry *b)
{
	return (a->tmReleaseDate == b->tmReleaseDate) ? sort_name(a, b) : (b->tmReleaseDate < a->tmReleaseDate);
}
static __inline bool sort_rev_dirname(FMEntry *a, FMEntry *b)
{
	return sort_dirname(b, a);
}
static __inline bool sort_rev_archive(FMEntry *a, FMEntry *b)
{
	return sort_archive(b, a);
}

static __inline BOOL tagfltcmp(const char *tag, const char *filter)
{
	/*for (;;)
	{
		if (tolower(*tag) != tolower(*filter))
			return *filter == '*';
		else if (!*tag)
			return TRUE;
		tag++;
		filter++;
	}*/

	for (;;)
	{
		int l1, l2;
		unsigned int u1, u2;

		u1 = fl_utf8decode(tag, 0, &l1);
		u2 = fl_utf8decode(filter, 0, &l2);

		if (fl_tolower(u1) != fl_tolower(u2))
			return *filter == '*';
		else if (!*tag)
			return TRUE;

		tag += l1;
		filter += l2;
	}

	return FALSE;
}

struct FilterContext
{
	const char *filterName;
};

// return TRUE if fm is visible
static BOOL DoFilter(FMEntry *fm, const FilterContext &ctxt)
{
	int i, j;

	if (g_cfg.filtShow & FSHOW_NotAvail)
	{
		if (!fm->IsInstalled()
			&& !(g_cfg.filtShow & FSHOW_ArchivedOnly) && fm->IsArchived())
			return FALSE;
	}
	else if (g_cfg.filtShow & FSHOW_ArchivedOnly)
	{
		if ( !fm->IsAvail() )
			return FALSE;
	}
	else if ( !fm->IsInstalled() )
		return FALSE;

	if (!(g_cfg.filtShow & FSHOW_NotPlayed) && fm->status == FMEntry::STATUS_NotPlayed)
		return FALSE;
	if (!(g_cfg.filtShow & FSHOW_Completed) && fm->status == FMEntry::STATUS_Completed)
		return FALSE;
	// when showing completed also show completed ones that are in progress
	if (!(g_cfg.filtShow & FSHOW_InProgress) && fm->status == FMEntry::STATUS_InProgress
		&& (!(g_cfg.filtShow & FSHOW_Completed) || !fm->nCompleteCount))
		return FALSE;

	if (fm->rating < g_cfg.filtMinRating || fm->priority < g_cfg.filtMinPrio)
		return FALSE;

	if (ctxt.filterName && !strstr(fm->GetFilterName(), ctxt.filterName))
		return FALSE;

	if (fm->tmReleaseDate < g_cfg.filtReleaseMinTime || fm->tmReleaseDate > g_cfg.filtReleaseMaxTime)
		return FALSE;

	// apply FOP_OR tag filters (fm must only match on of the filters)
	if ( !g_cfg.tagFilterList[FOP_OR].empty() )
	{
		for (i=0; i<(int)g_cfg.tagFilterList[FOP_OR].size(); i++)
		{
			const char *filter = g_cfg.tagFilterList[FOP_OR][i];
			for (j=0; j<(int)fm->taglist.size(); j++)
				if ( tagfltcmp(fm->taglist[j], filter) )
					goto found_or_match;
		}
		// found no match
		return FALSE;
found_or_match:;
	}

	// apply FOP_AND tag filters (fm must match all of the filters)
	for (i=0; i<(int)g_cfg.tagFilterList[FOP_AND].size(); i++)
	{
		const char *filter = g_cfg.tagFilterList[FOP_AND][i];
		for (j=0; j<(int)fm->taglist.size(); j++)
			if ( tagfltcmp(fm->taglist[j], filter) )
				goto found_and_match;
		// found no match
		return FALSE;
found_and_match:;
	}

	// apply FOP_NOT tag filters (fm must match none of the filters)
	for (i=0; i<(int)g_cfg.tagFilterList[FOP_NOT].size(); i++)
	{
		const char *filter = g_cfg.tagFilterList[FOP_NOT][i];
		for (j=0; j<(int)fm->taglist.size(); j++)
			if ( tagfltcmp(fm->taglist[j], filter) )
				return FALSE;
	}

	return TRUE;
}


static int GetFilteredDbIndex(FMEntry *fm)
{
	for (int i=0; i<(int)g_dbFiltered.size(); i++)
		if (g_dbFiltered[i] == fm)
			return i;
	return -1;
}

static void RefreshFilteredDb(BOOL bUpdateListControl, BOOL bReSortOnly)
{
	static BOOL bRefreshing = FALSE;

	if (bRefreshing)
		return;

	FMEntry *pCurSel = GetCurSelFM();

	if (!bReSortOnly)
	{
		g_dbFiltered.clear();
		g_dbFiltered.reserve( g_db.size() );

		// check if nothing is filtered
		if ( !g_cfg.HasFilters() )
		{
			g_dbFiltered = g_db;
		}
		else
		{
			FilterContext ctxt;

			// lower case name filter string
			string s;
			ctxt.filterName = NULL;
			if ( !g_cfg.filtName.empty() )
			{
				s = KEY_UTF( g_cfg.filtName.c_str() );
				ctxt.filterName = s.c_str();
			}

			for (int i=0; i<(int)g_db.size(); i++)
			{
				FMEntry *fm = g_db[i];
				if ( DoFilter(fm, ctxt) )
					g_dbFiltered.push_back(fm);
			}
		}
	}

	if ( !g_dbFiltered.empty() )
	{
		switch (g_cfg.sortmode)
		{
		case SORT_Rating: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rating); break;
		case SORT_Priority: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_prio); break;
		case SORT_Status: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_status); break;
		case SORT_LastPlayed: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_lastplayed); break;
		case SORT_ReleaseDate: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_released); break;
		case SORT_DirName: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_dirname); break;
		case SORT_Archive: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_archive); break;

		case -SORT_Name: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_name); break;
		case -SORT_Rating: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_rating); break;
		case -SORT_Priority: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_prio); break;
		case -SORT_Status: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_status); break;
		case -SORT_LastPlayed: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_lastplayed); break;
		case -SORT_ReleaseDate: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_released); break;
		case -SORT_DirName: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_dirname); break;
		case -SORT_Archive: std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_rev_archive); break;

		default:
			std::sort(g_dbFiltered.begin(), g_dbFiltered.end(), sort_name);
		}
	}

	if (bUpdateListControl)
	{
		bRefreshing = TRUE;

		RefreshListControl(pCurSel, bReSortOnly);

		bRefreshing = FALSE;
	}
}

//

static void InvalidateTagDb()
{
	g_bTagDbValid = FALSE;
}

static void RefreshTagDb()
{
	if (g_bTagDbValid)
		return;

	g_bTagDbValid = TRUE;

	g_dbTagCountHash.clear();
#ifdef USE_TAG_FM_XREF_DB
	g_dbTagHash.clear();
#endif

	DestroyTagNamelists();
	g_tagNameList.reserve(1024);
	g_tagNameListNoCat.reserve(1024);

	for (int i=0; i<(int)g_db.size(); i++)
	{
		FMEntry *fm = g_db[i];

		for (int i=0; i<(int)fm->taglist.size(); i++)
		{
			const char *tag = fm->taglist[i];

			tIStrHashKey key = KEY_UTF(tag);
			const char *cat = strchr(tag, ':');

#ifdef USE_TAG_FM_XREF_DB
			g_dbTagHash.insert( std::make_pair<tIStrHashKey,FMEntry*>(key,fm) );
#endif

			if (cat)
			{
				char c = cat[1];
				(char&)cat[1] = 0;
				tIStrHashKey ckey = KEY_UTF(tag);
				(char&)cat[1] = c;

#ifdef USE_TAG_FM_XREF_DB
				g_dbTagHash.insert( std::make_pair<tIStrHashKey,FMEntry*>(ckey,fm) );
#endif

				tTagCountHash::iterator dataIter = g_dbTagCountHash.find(key);
				if (dataIter != g_dbTagCountHash.end())
				{
					// tag already in db
					dataIter->second++;
					g_dbTagCountHash[ckey]++;
					continue;
				}

				// new tag entry
				g_dbTagCountHash[key] = 1;
				g_dbTagCountHash[ckey]++;
				g_tagNameList.push_back( _strdup(tag) );
			}
			else
			{
				tTagCountHash::iterator dataIter = g_dbTagCountHash.find(key);
				if (dataIter != g_dbTagCountHash.end())
				{
					// tag already in db
					dataIter->second++;
					continue;
				}

				// new tag entry
				g_dbTagCountHash[key] = 1;
				g_tagNameListNoCat.push_back( _strdup(tag) );
			}
		}
	}

	std::sort(g_tagNameList.begin(), g_tagNameList.end(), compare_tags);
	std::sort(g_tagNameListNoCat.begin(), g_tagNameListNoCat.end(), compare_tags);
}

static BOOL IsTagInFilterList(const char *tagfilter)
{
	ASSERT(tagfilter && *tagfilter);

	RefreshTagDb();

	tIStrHashKey key = KEY_UTF(tagfilter);

	tTagFilterHash::iterator dataIter = g_dbTagFilterHash.find(key);
	if (dataIter != g_dbTagFilterHash.end())
		return TRUE;

	return FALSE;
}

static void AddTagFilter(const char *tagfilter, int op, BOOL bLoading)
{
	ASSERT(tagfilter && *tagfilter);

	tIStrHashKey key = KEY_UTF(tagfilter);

	// make sure it's not already added
	tTagFilterHash::iterator dataIter = g_dbTagFilterHash.find(key);
	if (dataIter != g_dbTagFilterHash.end() && dataIter->second)
		return;

	g_dbTagFilterHash[key] = 1 + op;

	g_cfg.tagFilterList[op].push_back( _strdup(tagfilter) );

	if (!bLoading)
	{
		g_cfg.OnModified();
		RefreshFilteredDb();
	}

	TRACE("AddTagFilter: \"%s\" %d", tagfilter, op);
}

static void InvalidateTagFilterHash()
{
	g_dbTagFilterHash.clear();
}

static void RemoveTagFilter(const char *tagfilter, BOOL bRefresh = TRUE)
{
	ASSERT(tagfilter && *tagfilter);

	RefreshTagDb();

	tIStrHashKey key = KEY_UTF(tagfilter);

	// make sure it's added
	tTagFilterHash::iterator dataIter = g_dbTagFilterHash.find(key);
	if (dataIter == g_dbTagFilterHash.end())
		return;

	ASSERT(dataIter->second >= 1);
	const int op = dataIter->second - 1;

	g_dbTagFilterHash.erase(key);

	TRACE("RemoveTagFilter: \"%s\" %d", tagfilter, op);

	for (int i=0; i<(int)g_cfg.tagFilterList[op].size(); i++)
		if ( !fl_utf_strcasecmp(g_cfg.tagFilterList[op][i], tagfilter) )
		{
			const char *val = g_cfg.tagFilterList[op][i];
			g_cfg.tagFilterList[op].erase(g_cfg.tagFilterList[op].begin() + i);
			free((void*)val);
			break;
		}
	g_cfg.OnModified();

	if (bRefresh)
		RefreshFilteredDb();
}

static void RemoveDeadTagFilters(BOOL bRefreshList = TRUE)
{
	// remove tags from tag filters that are no longer in the tag db (can happen after FM entry is deleted from db)

	RefreshTagDb();

	BOOL bModified = FALSE;

	string tmp;

	for (int op=0; op<FOP_NUM_OPS; op++)
		for (int i=0; i<(int)g_cfg.tagFilterList[op].size(); i++)
			{
				const char *val = g_cfg.tagFilterList[op][i];

				const char *key = val;
				// handle category filters (strip asterisk)
				const int l = strlen(val);
				if (l > 2 && !strcmp(val+l-2, ":*"))
				{
					((char*)val)[l-1] = 0;
					tmp = val;
					((char*)val)[l-1] = '*';

					key = tmp.c_str();
				}

				tTagCountHash::iterator dataIter = g_dbTagCountHash.find(KEY_UTF(key));
				if (dataIter == g_dbTagCountHash.end())
				{
					// tag no longer in db, remove from filter list
					g_cfg.tagFilterList[op].erase(g_cfg.tagFilterList[op].begin() + i);
					free((void*)val);
					i--;
					bModified = TRUE;
				}
			}

	if (bModified)
	{
		g_cfg.OnModified();
		if (bRefreshList)
			RefreshFilteredDb();
		UpdateFilterControls();
	}
}

static void TrimLeft(char *s)
{
	const char *q = s;
	int i = 0;
	for (; isspace_(*q); q++);
	if (q != s)
		memmove(s, q, strlen(q)+1);
}

// assumes that there are non whitespace chares at the beginning of the string
static void TrimRight(char *s)
{
	int len = strlen(s);
	while ( isspace_(s[len-1]) )
	{
		ASSERT(len > 1);
		s[--len] = 0;
	}
}

static string Trimmed(const char *s, int leftright)
{
	if (!s || !*s)
		return "";

	// trim left
	if (leftright & 1)
	{
		while ( isspace_(*s) ) s++;
		if (!*s)
			return "";
	}

	// trim right
	if (leftright & 2)
	{
		int len = strlen(s);
		if ( isspace_(s[len-1]) )
		{
			while ( isspace_(s[len-1]) ) len--;

			char ch = s[len];
			(char&)s[len] = 0;
			string d = s;
			(char&)s[len] = ch;

			return d;
		}
	}

	return s;
}

static BOOL CleanTag(char *tag, BOOL bFilter)
{
	ASSERT(tag != NULL);

	const char *org = tag;

	// makes sure initial string is trimmed

	TrimLeft(tag);
	if (!*org)
		return FALSE;

	TrimRight(tag);

	// if a tag starts with a colon then ignore it and disallow it contain a category
	if (*tag == ':')
	{
		// remove colon
		memmove(tag, tag+1, strlen(tag));

		// trim again in case there were spaces after the colon
		TrimLeft(tag);
		if (!*org)
			return FALSE;
	}
	else
	{
		// a tag may only contain a single colon, to separate between tag and category
		char *s = strchr(tag, ':');
		if (s)
		{
			// replace illegal chars in category name (and enforce lower case)
			for (; tag<s; tag++)
			{
				if ( strchr(ILLEGAL_TAG_CHARS, *tag) )
					*tag = '_';
				else
					*tag = tolower(*tag);
			}

			ASSERT(*tag == ':');

			// trim spaces right before and after the colon
			int len = strlen(tag) + 1;
			char *q = tag - 1;
			while ( isspace_(*q) ) q--;
			if (q != tag-1)
			{
				memmove(q+1, tag, len);
				tag = q+2;
			}
			else
				tag++;
			TrimLeft(tag);

			if (!*tag)
			{
				// a tag cannot consist of only a category, strip trailing colon
				tag[-1] = 0;
				return *org != 0;
			}

			// allow "cat:*" style tag filters
			if (bFilter && !strcmp(tag, "*"))
				return *org != 0;
		}
	}

	for (; *tag; tag++)
		if ( strchr(ILLEGAL_TAG_CHARS, *tag) )
			*tag = '_';

	return *org != 0;
}


static void TrimTrailingSlashFromPath(string &path, BOOL bTrimRoot)
{
	if ( path.empty() )
		return;

	if (!bTrimRoot)
	{
		if (path == "/" || path == "\\")
			return;
	}

#ifdef _WIN32
	if ((path.length() == 2 || path.length() == 3) && path[1] == ':')
	{
		if (bTrimRoot)
			path.clear();
		else if (path.length() == 2)
			path += "\\";
		return;
	}
#endif

	int n = path.size() - 1;
	if ( !isdirsep(path[n]) )
		return;

	do
	{
		n--;
	}
	while ( isdirsep(path[n]) );

	if (n < 0)
		path.clear();
	else
		path = path.substr(0, n+1);
}


//

static void TermDb()
{
	for (unsigned int i=0; i<g_db.size(); i++)
	{
		FMEntry *fm = g_db[i];
		delete fm;
	}

	DestroyTagNamelists();
	g_dbTagFilterHash.clear();
	g_dbTagCountHash.clear();
#ifdef USE_TAG_FM_XREF_DB
	g_dbTagHash.clear();
#endif

	g_db.clear();
	g_db.resize(0);
	g_dbHash.clear();
	g_dbUnverifiedArchiveHash.clear();
	g_dbFiltered.clear();
	g_dbFiltered.resize(0);

	g_invalidDirs.clear();
}


/////////////////////////////////////////////////////////////////////
// MOD.INI Import (support for importing SS2 Mod Manager mod.ini)

char* skip_ws(char *s)
{
	while ( isspace_(*s) ) s++;
	return s;
}

char* next_line(char *s)
{
	if (!*s)
		return s;

	char *t = s;

	// find end of line
	while (*s && *s != '\n' && *s != '\r') s++;

	char *q = *s ? s-1 : s;
	const char nl = *s;

	// null-terminate end of line
	if (*s)
		*s++ = 0;

	// skip past \r\n combo
	if (nl == '\r' && *s == '\n')
		s++;

	// trim trailing spaces
	while (q >= t && isspace_(*q)) *q-- = 0;

	return s;
}

static BOOL ReadModIni(FMEntry *fm, const FMEntry *realfm)
{
	// sections (of interest):
	//   [authors]
	//   [description]
	//   [modHomepage]
	//   [modName]
	//   [readMe]

	char *data;
	int n;

	if ( !FmReadFileToBuffer(realfm, "mod.ini", data, n) )
		return FALSE;

	string homepage;

	// parse file
	char *s = data;
	while (*s)
	{
		char *nextline = next_line(s);

		if (*s != '[' || *nextline == '[')
		{
			s = nextline;
			continue;
		}

		if ( !_stricmp(s, "[authors]") )
		{
			char *val = nextline;
			s = nextline;
			nextline = next_line(s);

			// aggressivle strip author lines that appear to be in the format "nick (real name)"
			// this can very well strip too much if there are multiple authors, but those aren't
			// suitable for tags anyway
			if (*val && *val != '(')
			{
				char *q = strchr(val, '(');
				if (q)
				{
					q--;
					while (q > val && isspace_(*q)) *q-- = 0;
				}
			}

			if (*val && CleanTag(val, FALSE))
			{
				string str = "author:";
				str.append(val);
				fm->SetTags( str.c_str() );
			}
		}
		else if ( !_stricmp(s, "[description]") )
		{
			do
			{
				char *val = nextline;
				s = nextline;
				nextline = next_line(s);

				if (*val != ';')
				{
					if (*val)
						fm->descr.append(val);
					fm->descr.append("\n");
				}
			}
			while (*nextline && *nextline != '[');

			fm->descr = Trimmed(fm->descr.c_str(), 2);
		}
		else if ( !_stricmp(s, "[modHomepage]") )
		{
			char *val = nextline;
			s = nextline;
			nextline = next_line(s);

			if (*val)
			{
				homepage = Trimmed(val);

				// make sure it has a http prefix
				if (!homepage.empty() && _strnicmp(homepage.c_str(), "http", 4))
				{
					if ( _strnicmp(homepage.c_str(), "https", 5) )
						homepage = "https://" + homepage;
					else
						homepage = "http://" + homepage;
				}
			}
		}
		else if ( !_stricmp(s, "[modName]") )
		{
			char *val = nextline;
			s = nextline;
			nextline = next_line(s);

			if (*val)
				fm->SetNiceName(val);
		}
		else if ( !_stricmp(s, "[readMe]") )
		{
			char *val = nextline;
			s = nextline;
			nextline = next_line(s);

			if (*val)
				fm->infofile = val;
		}

		s = nextline;
	}

	// append homepage to description
	if ( !homepage.empty() )
	{
		string str = "\n\nHomepage: ";
		str.append(homepage);

		if (fm->descr.size()+str.length() > MAX_DESCR_LEN)
		{
			fm->descr = fm->descr.substr(0, MAX_DESCR_LEN - str.length() - 3);
			fm->descr.append("...");
		}

		fm->descr.append(str);
	}
	else if (fm->descr.size() > MAX_DESCR_LEN)
	{
		fm->descr = fm->descr.substr(0, MAX_DESCR_LEN - 3);
		fm->descr.append("...");
	}
	fm->descr = EscapeString( fm->descr.c_str() );

	delete[] data;

	//fm->OnUpdateName();

	return TRUE;
}


/////////////////////////////////////////////////////////////////////
// MISC TASKS

static const char *g_doctypes[] = { "rtf", "wri", "txt", "html", "htm", "doc", "pdf" };

struct FileDiffInfo
{
	unsigned __int64 fsize;
	time_t ftime;
	char *fname;
	char *fname_rel;

	FileDiffInfo() { fname = NULL; }
	~FileDiffInfo() { if (fname) free(fname); }
};
typedef unordered_map<tIStrHashKey, FileDiffInfo, KeyHash> tFileDiffInfoHash;

static tFileDiffInfoHash g_fileDiffInfoMap;
static std::list<string> g_removedFiles;

struct FileQ
{
	char *fname;
	char *fname_rel;

	FileQ() { fname = NULL; }
	~FileQ() { if (fname) free(fname); }
};
static std::list<FileQ> g_backupList;

static int g_nBusyCursorCount = 0;


void ShowBusyCursor(BOOL bShow)
{
	if (bShow)
	{
		if (!g_nBusyCursorCount)
			fl_cursor(FL_CURSOR_WAIT);
		g_nBusyCursorCount++;
	}
	else
	{
		ASSERT(g_nBusyCursorCount > 0);
		g_nBusyCursorCount--;
		if (!g_nBusyCursorCount)
			fl_cursor(FL_CURSOR_DEFAULT);
	}
}


// remove() variant that removes read-only files too
static int remove_forced(const char *s)
{
	int res = remove(s);
	if (res && errno == EACCES)
	{
		// read-only, try to set read-write
		if ( !_chmod(s, _S_IREAD | _S_IWRITE) )
			// worked, now delete it
			res = remove(s);
	}

	return res;
}


static inline char dirslashify(char ch)
{
	return isdirsep(ch) ? *DIRSEP : ch;
}

static void CleanDirSlashes(string &s)
{
	std::transform(s.begin(), s.end(), s.begin(), dirslashify);
}

static void CleanDirSlashes(char *s)
{
	if (!s || !*s)
		return;

	for (; *s; s++)
		if ( isdirsep(*s) )
			*s = *DIRSEP;
}

static void RegDefColors()
{
	// register custom colors
	unsigned int c = 96;
	REGCLR(207,227,227);
	REGCLR(180,180,200);
	REGCLR(245,245,255);
	REGCLR(192,239,250);
	REGCLR(232,232,240);
	REGCLR(240,240,240);
	REGCLR(206,238,250);
	REGCLR(219,219,225);
	REGCLR(225,225,225);
	REGCLR(70,70,250);
	REGCLR(243,135,75);
	REGCLR(255,92,3);
	REGCLR(220,220,220);
	REGCLR(50,90,200);
	REGCLR(30,50,80);
	REGCLR(150,185,220);
	REGCLR(60,120,180);
	REGCLR(145,200,245);
	REGCLR(130,170,200);
	REGCLR(105,135,160);
	REGCLR(95,95,100);
	REGCLR(205,220,205);
	REGCLR(220,205,205);
	REGCLR(205,210,220);
	REGCLR(240,240,230);
	REGCLR(100,170,225);
	REGCLR(210,210,210);
	REGCLR(230,230,230);
	REGCLR(226,226,217);
	REGCLR(170,238,255);
}

static void ChangeScheme()
{
	// swap the necessary parts of the color map
	unsigned int color, c, i = 0;
	for (c = FL_FOREGROUND_COLOR; c < FL_FOREGROUND_COLOR+16; c++) { SWPCLR(c,i); } // 3-bit colormap
	for (c = FL_GRAY0; c <= FL_BLACK; c++) { SWPCLR(c,i); } // grayscale ramp and FL_BLACK
	SWPCLR(FL_RED,i); // FL_RED
	for (c = USER_CLR; c < USER_CLR+30; c++) { SWPCLR(c,i); } // custom colors
	SWPCLR(215,i); // Tooltip background
	SWPCLR(FL_BLUE,i); // FL_BLUE
	SWPCLR(FL_WHITE,i); // FL_WHITE
}


// generate a temp filename from a filename and make sure no file already exists
// usable temp filename is returned in 'tmpfile'
// if bReturnWithPath is 1 the path returned may be relative, if it's 2 the returned path is absolute
// otherwise only the filename without path is returned
static BOOL GetTempFile(const char *fname, string &tmpfile, int bReturnWithPath)
{
	// generate a temp filename (optionally with path) from a filename and make sure no file already exists

	if ( g_sTempDir.empty() )
		return FALSE;

	tmpfile = (bReturnWithPath>1 ? g_sTempDirAbs : g_sTempDir) + fname;

	// make sure file doesn't exist, if we failed to remove an existing file due to no write access
	// then we fall back to an alternative name
	if (remove_forced( tmpfile.c_str() ) && errno == EACCES)
	{
		char buf[16];
		string s;

		for (int i=2; ;i++)
		{
			sprintf(buf, " (%d)", i);
			s = tmpfile + buf;

			if (!remove_forced( s.c_str() ) || errno == ENOENT)
			{
				if (!bReturnWithPath)
				{
					tmpfile = fname;
					tmpfile += buf;
				}
				else
					tmpfile = s;

				return TRUE;
			}
		}
	}

	if (!bReturnWithPath)
		tmpfile = fname;

	return TRUE;
}


static BOOL FmFileExists(const FMEntry *fm, const char *fname)
{
	if (!fname || !*fname)
		return FALSE;

	if ( !fm->IsInstalled() )
	{
		if ( fm->IsArchived() )
			return IsFileInArchive(fm->GetArchiveFilePath().c_str(), fname);

		return FALSE;
	}

	char pathname[MAX_PATH_BUF];
	if (_snprintf_s(pathname, sizeof(pathname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name, fname) == -1)
		return FALSE;

	struct stat st = {0};
	return !stat(pathname, &st);
}

static BOOL FmReadFileToBuffer(const FMEntry *fm, const char *fname, char *&data, int &len)
{
	data = NULL;
	len = 0;

	if ( !fm->IsInstalled() )
	{
		if ( fm->IsArchived() )
			if ( ExtractFileFromArchive(fm->GetArchiveFilePath().c_str(), fname, (void*&)data, len) )
				return TRUE;

		return FALSE;
	}

	char fpath[MAX_PATH_BUF];

	if (_snprintf_s(fpath, sizeof(fpath), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name, fname) == -1)
		return FALSE;

	FILE *f = fopen(fpath, "rb");
	if (!f)
		return FALSE;

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
		return FALSE;
	}

	len = n;

	// add null terminator and extra terminator as safety padding to simplify parser code, in case data is text
	data = new char[n+2];
	data[n] = 0;
	data[n+1] = 0;

	fread(data, 1, n, f);
	fclose(f);

	return TRUE;
}

// get a file from archive as a real (temp) file on disk, the real file's name is returned in 'outfile'
// if bReturnWithPath is 1 the path returned may be relative, if it's 2 the returned path is absolute
// otherwise only the filename without path is returned
static BOOL FmGetPhysicalFileFromArchive(FMEntry *fm, const char *filename, string &outfile, int bReturnWithPath)
{
	string s;
	if ( !GetTempFile(filename, s, FALSE) )
		return FALSE;

	string t = g_sTempDir + s;
	const char *pErrMsg = NULL;

	if ( !ExtractFileFromArchive(fm->GetArchiveFilePath().c_str(), filename, t.c_str(), &pErrMsg) )
	{
		fl_alert($("Failed to extract \"%s\" to \"%s\".\n\nError: %s"), filename, t.c_str(), pErrMsg ? pErrMsg : "unknown error");
		return FALSE;
	}

	if (bReturnWithPath)
	{
		if (bReturnWithPath > 1)
			outfile = g_sTempDirAbs + s;
		else
			outfile = t;
	}
	else
		outfile = s;

	return TRUE;
}

static BOOL FmOpenFileWithAssociatedApp(FMEntry *fm, const char *filename)
{
	if ( !fm->IsAvail() )
		return FALSE;

	char pathname[MAX_PATH_BUF];

	if ( !fm->IsInstalled() )
	{
		// extract file from archive to temp dir

		string s;
		if ( !FmGetPhysicalFileFromArchive(fm, filename, s, FALSE)
#ifndef _WIN32
			|| snprintf(pathname, sizeof(pathname), "%s" DIRSEP "%s", g_sTempDirAbs.c_str(), filename) == -1
#endif
			)
			return FALSE;

#ifdef _WIN32
		return OpenFileWithAssociatedApp(filename, g_sTempDirAbs.c_str());
#else
		return OpenFileWithAssociatedApp(pathname);
#endif
	}
	else
	{
#ifdef _WIN32
		if (_snprintf_s(pathname, sizeof(pathname), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
#else
		if (snprintf(pathname, sizeof(pathname), "%s" DIRSEP "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name, filename) == -1)
#endif
			return FALSE;

#ifdef _WIN32
		return OpenFileWithAssociatedApp(filename, pathname);
#else
		return OpenFileWithAssociatedApp(pathname);
#endif
	}
}


static BOOL ExportFmIni(FMEntry *fm, BOOL bSaveTitle = FALSE)
{
	char fname[MAX_PATH_BUF];
	if (*fm->name)
	{
		if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "fm.ini", g_pFMSelData->sRootPath, fm->name) == -1)
		{
			fl_alert($("Failed to open file \"%s\" for writing, path name too long."), fname);
			return FALSE;
		}
	}
	else
		strcpy(fname, "fm.ini");

	const char *pattern[] =
	{
		$("FM Ini File"), "fm.ini",
		NULL,
	};

	const char *title = bSaveTitle ? $("Save FM.INI") : $("Export FM.INI");

	if ( !FileDialog(pMainWnd, TRUE, title, pattern, "fm.ini", fname, fname, sizeof(fname)) )
		return FALSE;

	FILE *f = fopen(fname, "wb");
	if (!f)
	{
		fl_alert($("Failed to open file \"%s\" for writing."), fname);
		return FALSE;
	}

	fm->WriteIni(f);

	fclose(f);

	return TRUE;
}

static BOOL ExportBatchFmIni()
{
	char fname[MAX_PATH_BUF];
	strcpy(fname, "fm_batch.ini");

	const char *pattern[] =
	{
		$("Batch FM Ini File"), "fm_batch.ini",
		NULL,
	};

	if ( !FileDialog(pMainWnd, TRUE, $("Export FM_BATCH.INI"), pattern, "ini", fname, fname, sizeof(fname)) )
		return FALSE;

	FILE *f = fopen(fname, "wb");
	if (!f)
	{
		fl_alert($("Failed to open file \"%s\" for writing."), fname);
		return FALSE;
	}

	int count = 0;

	// export all archived FMs in filtered list
	for (int i=0; i<(int)g_dbFiltered.size(); i++)
	{
		FMEntry *fm = g_dbFiltered[i];

		if ( !fm->IsArchived() )
			continue;

		if (count)
			fprintf(f, "\r\n");

		const char *name = fm->GetArchiveNameOnly();

		fprintf(f, "[FM=%s]\r\n", name);

		fm->WriteIni(f);

		count++;
	}

	fclose(f);

	if (!count)
	{
		remove(fname);
		fl_message($("The current list contained no archived FMs to export a batched fm.ini for"));
	}

	return TRUE;
}

static BOOL ApplyImportedData(FMEntry *fm, const FMEntry *data, int mode)
{
	ASSERT(fm && data);

	if (mode & IMP_ModeFiltOny)
		// skip if fm is not in filtered db
		if (GetFilteredDbIndex(fm) == -1)
			return FALSE;

	BOOL bModified = FALSE;

	if (mode & IMP_ModeOverwrite)
	{
		if ((mode & IMP_Name) && !data->nicename.empty())
		{
			if (fm->nicename.empty() || fm->nicename != data->nicename)
			{
				fm->SetNiceName( data->nicename.c_str() );
				bModified = TRUE;
			}
		}
		if ((mode & IMP_RelDate) && data->tmReleaseDate)
		{
			if (!fm->tmReleaseDate || fm->tmReleaseDate != data->tmReleaseDate)
			{
				fm->tmReleaseDate = data->tmReleaseDate;
				fm->OnModified();
				bModified = TRUE;
			}
		}
		if ((mode & IMP_Descr) && !data->descr.empty())
		{
			if (fm->descr.empty() || fm->descr != data->descr)
			{
				fm->SetDescr( data->descr.c_str() );
				bModified = TRUE;
			}
		}
		if ((mode & IMP_InfoFile) && !data->infofile.empty())
		{
			if (fm->infofile.empty() || fm->infofile != data->infofile)
			{
				fm->infofile = data->infofile;
				fm->flags &= ~FMEntry::FLAG_NoInfoFile;
				fm->OnModified();
				bModified = TRUE;
			}
		}
		if ((mode & IMP_Tags) && !data->taglist.empty())
		{
			if (fm->tags.empty() || fm->tags != data->tags)
			{
				fm->SetTags( data->tags.c_str() );
				bModified = TRUE;
			}
		}
	}
	else if (mode & (IMP_ModeFill|IMP_ModeFillAddTags))
	{
		if ((mode & IMP_Name) && !data->nicename.empty())
		{
			if ( fm->nicename.empty() )
			{
				fm->SetNiceName( data->nicename.c_str() );
				bModified = TRUE;
			}
		}
		if ((mode & IMP_RelDate) && data->tmReleaseDate)
		{
			if (!fm->tmReleaseDate)
			{
				fm->tmReleaseDate = data->tmReleaseDate;
				fm->OnModified();
				bModified = TRUE;
			}
		}
		if ((mode & IMP_Descr) && !data->descr.empty())
		{
			if ( fm->descr.empty() )
			{
				fm->SetDescr( data->descr.c_str() );
				bModified = TRUE;
			}
		}
		if ((mode & IMP_InfoFile) && !data->infofile.empty())
		{
			if ( fm->infofile.empty() )
			{
				fm->infofile = data->infofile;
				fm->flags &= ~FMEntry::FLAG_NoInfoFile;
				fm->OnModified();
				bModified = TRUE;
			}
		}
		if ((mode & IMP_Tags) && !data->taglist.empty())
		{
			if (mode & IMP_ModeFillAddTags)
			{
				// add tags that aran't already added
				BOOL bAdded = FALSE;
				string tags = fm->tags;
				for (int i=0; i<(int)data->taglist.size(); i++)
				{
					if ( !fm->HasTag(data->taglist[i]) )
					{
						if ( !tags.empty() )
							tags += ",";
						tags += data->taglist[i];
						bAdded = TRUE;
					}
				}
				if (bAdded)
				{
					fm->SetTags( tags.c_str() );
					bModified = TRUE;
				}
			}
			else if ( fm->taglist.empty() )
			{
				fm->SetTags( data->tags.c_str() );
				bModified = TRUE;
			}
		}
	}
	else
		return FALSE;

	return bModified;
}

static BOOL ImportBatchFmIni()
{
	char fname[MAX_PATH_BUF];
	strcpy(fname, "fm_batch.ini");

	const char *pattern[] =
	{
		$("Batch FM Ini File"), "*.ini",
		NULL,
	};

	if ( !FileDialog(pMainWnd, FALSE, $("Import FM_BATCH.INI"), pattern, "ini", fname, fname, sizeof(fname)) )
		return FALSE;

	// show import dialog (user can select what to import, tags, release dates etc., and how to apply it,
	// like merge tags with existing, replace existing data, or only import of local data isn't specified)
	int mode = DoImportBatchFmIniDialog();
	if (!mode)
		return FALSE;

	FILE *f = fopen(fname, "rb");
	if (!f)
	{
		fl_alert($("Failed to open file \"%s\"."), fname);
		return FALSE;
	}

	char line[8192];
	int state = 0;
	FMEntry *fm = NULL;
	FMEntry *dummy = NULL;

	int count = 0;

	for (;;)
	{
		if ( !fgets(line, sizeof(line)-1, f) )
			break;

		line[sizeof(line)-1] = 0;
		int len = strlen(line);
		while (len && isspace_(line[len-1]))
		{
			len--;
			line[len] = 0;
		}

		if (!len)
			continue;

		char *s = line;
		while ( isspace_(*s) )
		{
			s++;
			len--;
		}

		ASSERT(len > 0);

		if (*s == ';')
			continue;

		if (state)
		{
			if (*s == '[')
				goto new_block;

			char *q = strchr(s, '=');
			if (!q)
			{
				TRACE("parse error, unknown token: %s", s);
				ASSERT(FALSE);
				continue;
			}

			if (!q[1])
				continue;

			*q = 0;
			char *val = q + 1;

			// [FM=...]
			ASSERT(fm && dummy);
			dummy->ParseVal(s, val);
		}
		else
		{
new_block:
			state = 0;

			if ( !_strnicmp(s, "[FM=", 4) )
			{
				if (s[len-1] != ']')
				{
					TRACE("parse error, unknown token: %s", s);
					ASSERT(FALSE);
					continue;
				}

				s[len-1] = 0;
				len -= 4+1;
				if (len <= 0)
				{
					TRACE("FM name missing");
					ASSERT(FALSE);
					continue;
				}

				if (dummy)
				{
					if ( ApplyImportedData(fm, dummy, mode) )
						count++;
					delete dummy;
					dummy = NULL;
				}

				// name is specified as an archive name without subdir

				fm = FindFmFromArchiveNameOnly(s+4);
				if (fm)
				{
					dummy = new FMEntry;
					state = 1;
				}
			}
		}
	}

	if (dummy)
	{
		if (fm && ApplyImportedData(fm, dummy, mode))
			count++;
		delete dummy;
	}

	fclose(f);

	if (count)
	{
		InvalidateTagDb();

		if (mode & IMP_ModeOverwrite)
			// tags may have been removed during this process
			RemoveDeadTagFilters();

		RefreshFilteredDb();

		fl_message($("Imported data for %d FMs."), count);
	}
	else
		fl_message($("No applicable data found, nothing imported."));

	return TRUE;
}

static void CreateStandaloneFmIni()
{
	FMEntry *fm = new FMEntry;

	time(&fm->tmReleaseDate);

	DoTagEditor(fm, TABPAGE_MISC);

	delete fm;
}


static BOOL ReadFmIni(FMEntry *fm, const FMEntry *realfm)
{
	char *data;
	int n;

	if ( !FmReadFileToBuffer(realfm, "fm.ini", data, n) )
		return FALSE;

	char *curs = data;
	while (curs < data+n)
	{
		char *line = curs;
		char *nextline = next_line(curs);
		curs = nextline;

		int len = strlen(line);
		if (!len)
			continue;

		char *s = line;
		while ( isspace_(*s) )
		{
			s++;
			len--;
		}

		ASSERT(len > 0);

		if (*s == ';')
			continue;

		char *val = strchr(s, '=');
		if (!val)
		{
			TRACE("parse error, unknown token: %s", s);
			ASSERT(FALSE);
			continue;
		}

		*val++ = 0;
		val = skip_ws(val);
		if (!*val)
			continue;

		fm->ParseVal(s, val);
	}

	delete data;

	//fm->OnUpdateName();

	return TRUE;
}

// read fm.ini into a dummy FMEntry that is returned, caller must delete it when done
// (use this function as a barrier so if an fm.ini contains things it isn't supposed, it doesn't get applied,
// like for exmaple if an ini contains rating, or status)
static FMEntry* GetIsolatedFmIni(FMEntry *fm, BOOL bFallbackModIni = FALSE)
{
	if ( !fm->IsAvail() )
		return NULL;

	FMEntry *p = new FMEntry;

	p->InitName(fm->name);

	if ( ReadFmIni(p, fm) )
		return p;
	else if (bFallbackModIni && ReadModIni(p, fm))
		return p;

	delete p;

	return NULL;
}


// get info from FM ini and replace existing info (this is generally intended for completely new entries)
static BOOL ApplyFmIni(FMEntry *fm, BOOL bFallbackModIni)
{
	FMEntry *ini = GetIsolatedFmIni(fm, bFallbackModIni);
	if (!ini)
		return FALSE;

	fm->nicename = ini->nicename;
	fm->tmReleaseDate = ini->tmReleaseDate;
	fm->SetTags( ini->tags.c_str() );
	fm->infofile = ini->infofile;
	fm->SetDescr( ini->descr.c_str() );

	if (g_cfg.bSaveNewDbEntriesWithFmIni)
		// save db entry permanently to avoid reading the fm.ini each time (which isn't a performance issue for zip/rar
		// archives, but it is for 7z)
		fm->OnModified();

	delete ini;

	return TRUE;
}


static BOOL SetReleaseDateFromFile(FMEntry *fm, const char *fname, time_t tmMin, time_t tmMax)
{
	char pathname[MAX_PATH_BUF];

	if (_snprintf_s(pathname, sizeof(pathname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name, fname) == -1)
		return FALSE;

	struct stat st = {0};

	if (!stat(pathname, &st) && st.st_mtime >= tmMin && st.st_mtime <= tmMax)
	{
		fm->tmReleaseDate = st.st_mtime;
		fm->flags |= FMEntry::FLAG_UnverifiedRelDate;
		fm->OnModified();

		return TRUE;
	}

	return FALSE;
}

static BOOL ScanForTypeAndSetReleaseDate(FMEntry *fm, vector<string> &files, vector<time_t> &ftimes, const int nFiles,
										 const char **filetypes, int nTypes, time_t tmMin, time_t tmMax)
{
	for (int i=0; i<nFiles; i++)
	{
		string &f = files[i];

		// skip dirs
		int len = f.length();
		if ( isdirsep(f[len-1]) )
			continue;

		// get extension
		while (len && f[len] != '.' && !isdirsep(f[len])) len--;
		if (f[len] != '.' || !f[len+1])
			continue;

		const char *ext = f.c_str() + len + 1;

		// check if extension matches any of the requested ones
		for (int j=0; j<nTypes; j++)
		{
			if (!_stricmp(ext, filetypes[j]) && ftimes[i] >= tmMin && ftimes[i] <= tmMax)
			{
				fm->tmReleaseDate = ftimes[i];
				fm->flags |= FMEntry::FLAG_UnverifiedRelDate;
				fm->OnModified();

				return TRUE;
			}
		}
	}

	return FALSE;
}

static BOOL ScanForTypeAndSetReleaseDate(FMEntry *fm, dirent **files, const int nFiles,
										 const char **filetypes, int nTypes, time_t tmMin, time_t tmMax)
{
	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		// skip dirs
		int len = strlen(f->d_name);
		if ( isdirsep(f->d_name[len-1]) )
			continue;

		// get extension
		while (len && f->d_name[len] != '.' && !isdirsep(f->d_name[len])) len--;
		if (f->d_name[len] != '.' || !f->d_name[len+1])
			continue;

		const char *ext = f->d_name + len + 1;

		// check if extension matches any of the requested ones
		for (int j=0; j<nTypes; j++)
		{
			if (!_stricmp(ext, filetypes[j]) && SetReleaseDateFromFile(fm, f->d_name, tmMin, tmMax))
				return TRUE;
		}
	}

	return FALSE;
}

static BOOL ScanReleaseDate(FMEntry *fm, time_t tmMin, time_t tmMax, BOOL bSkipFmIni = FALSE)
{
	char fname[MAX_PATH_BUF];

	if (!bSkipFmIni)
	{
		// first check for presence of an fm.ini and get the date from that if so
		FMEntry *ini = GetIsolatedFmIni(fm);
		if (ini)
		{
			// if ini didn't contain any release date then proceed with guesswork
			const time_t tmReleaseDate = ini->tmReleaseDate;
			delete ini;
			if (tmReleaseDate)
			{
				fm->tmReleaseDate = tmReleaseDate;
				fm->flags &= ~FMEntry::FLAG_UnverifiedRelDate;
				fm->OnModified();
				return TRUE;
			}
		}
	}

	const char *mistypes[] = { "mis" };

	BOOL bResult;

	if ( fm->IsInstalled() )
	{
		// couldn't get date from fm.ini, now try to get date from the modified timestamp of some appropiate files
		if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
			return FALSE;

		dirent **files;
		int nFiles = fl_filename_list(fname, &files, NO_COMP_UTFCONV);
		if (nFiles <= 0)
			return FALSE;

		// scan for documentation/readme files (txt/rtf/wri/doc/pdf) and use date from that
		bResult = ScanForTypeAndSetReleaseDate(fm, files, nFiles, g_doctypes, sizeof(g_doctypes)/sizeof(g_doctypes[0]), tmMin, tmMax);

		if (!bResult)
			// didn't find any documentation files, now scan for *.mis files and use the date from that
			bResult = ScanForTypeAndSetReleaseDate(fm, files, nFiles, mistypes, sizeof(mistypes)/sizeof(mistypes[0]), tmMin, tmMax);

		// free data
		fl_filename_free_list(&files, nFiles);
	}
	else
	{
		// scan files in archive

		vector<string> files;
		vector<time_t> ftimes;

		int nFiles = ListFilesInArchiveRoot(fm->GetArchiveFilePath().c_str(), files, &ftimes);
		if (nFiles <= 0)
			return FALSE;

		// scan for documentation/readme files (txt/rtf/wri/doc/pdf) and use date from that
		bResult = ScanForTypeAndSetReleaseDate(fm, files, ftimes, nFiles, g_doctypes, sizeof(g_doctypes)/sizeof(g_doctypes[0]), tmMin, tmMax);

		if (!bResult)
			// didn't find any documentation files, now scan for *.mis files and use the date from that
			bResult = ScanForTypeAndSetReleaseDate(fm, files, ftimes, nFiles, mistypes, sizeof(mistypes)/sizeof(mistypes[0]), tmMin, tmMax);
	}

	return bResult;
}


static void InitValidMinMaxDate(time_t &tmMin, time_t &tmMax)
{
	// determine min and max timestamps that constitute a valid date (to filter out screwed vals)
	// min is 1996 (when dark developement began), and max is today's date plus roughly a month

	struct tm t = {0};
	t.tm_year = 1996 - 1900;
	t.tm_mon = 4 - 1;
	t.tm_mday = 1;
	t.tm_hour = 12;
	tmMin = _mkgmtime(&t);
	if (tmMin == (time_t)-1)
		tmMin = 0;

	time(&tmMax);
	tmMax += 60 * 60 * 24 * 30;
}


// automatically try to determine release date of FM, if none is currently set
// returns TRUE if an auto-scanned date was set
static BOOL AutoScanReleaseDate(FMEntry *fm, BOOL bSkipFmIni)
{
	ASSERT(fm != NULL);

	if (fm->tmReleaseDate)
		return FALSE;

	time_t tmMin, tmMax;
	InitValidMinMaxDate(tmMin, tmMax);

	return ScanReleaseDate(fm, tmMin, tmMax, bSkipFmIni);
}

// automatically try to determine release dates of FMs that currently don't have any set
static void AutoScanReleaseDates()
{
	BOOL bModified = FALSE;

	time_t tmMin, tmMax;
	InitValidMinMaxDate(tmMin, tmMax);

	// use filtered db so thee user has a convenient way to limit which FMs to process
	for (int i=0; i<(int)g_dbFiltered.size(); i++)
	{
		FMEntry *fm = g_dbFiltered[i];

		if (fm->tmReleaseDate != 0 || !fm->IsAvail())
			continue;

		if ( ScanReleaseDate(fm, tmMin, tmMax) )
			bModified = TRUE;
	}

	if (bModified)
		RefreshFilteredDb();
}


static string EscapeString(const char *s)
{
	// blackslashify tabs and line breaks

	if (!s || !*s)
		return "";

	int len = strlen(s);
	string d;

	d.reserve(len+16);

	for (int i=0; i<len; i++)
	{
		switch (s[i])
		{
		case '\\': d += "\\\\"; break;
		case '\r': if (s[i+1] != '\n') d += "\\n"; break;
		case '\n': d += "\\n"; break;
		case '\t': d += "\\t"; break;
		case '\"': d += "\\\""; break;
		default:
			d += s[i];
		}
	}

	return d;
}

static string UnescapeString(const char *s)
{
	// un-blackslashify tabs and line breaks

	if (!s || !*s)
		return "";

	int len = strlen(s);
	string d;

	d.reserve(len+16);

	for (int i=0; i<len; i++)
	{
		if (s[i] == '\\')
		{
			i++;

			switch (s[i])
			{
			case 't': d += '\t'; break;
			case 'n': d += "\n"; break;
			case 'r': /*skip*/ break;
			case '\"': d += "\""; break;
			default:
				d += s[i];
			}
		}
		else
		{
			d += s[i];
		}
	}

	return d;
}


static __inline int GetTypeScore(const char *ext)
{
	const int nTypes = sizeof(g_doctypes)/sizeof(g_doctypes[0]);
	for (int i=0; i<nTypes; i++)
		if ( !_stricmp(ext, g_doctypes[i]) )
			return (nTypes-i-1) * 10;
	return 0;
}

static const char* stristr(const char *_Str, const char *_LowerCaseSubStr)
{
	string lower_str = _Str;
	std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
	return strstr(lower_str.c_str(), _LowerCaseSubStr);
}

/*static const char* stristr_utf(const char *_Str, const char *_LowerCaseSubStr)
{
	char buf[256*3];
	tolower_utf(_Str, sizeof(buf), buf);

	return strstr(buf, _LowerCaseSubStr);
}*/

static char* strcpy_safe(char *_Dst, rsize_t _SizeInBytes, const char *_Src)
{
	if (_SizeInBytes > 0)
	{
		rsize_t len = strlen(_Src);
		char *r = strncpy(_Dst, _Src, _SizeInBytes-1);
		_Dst[_SizeInBytes > len ? len : _SizeInBytes-1] = '\0';
		return r;
	}

	return _Dst;
}

static int GetFileScore(const char *s)
{
	// name is guaranteed to contain a g_doctypes extension, don't need to check return val of strrchr
	const char *ext = strrchr(s, '.') + 1;

	int score = GetTypeScore(ext);

	// starts with "fminfo"
	if ( !_strnicmp(s, "fminfo", 6) )
	{
		if (s[6] == '-' && _strnicmp(s+7, "en", 2))
			score += 7000;
		else
			score += 8000;
	}
	// starts with "readme"
	else if ( !_strnicmp(s, "readme", 6) )
	{
		if (s[6] == '-' && _strnicmp(s+7, "en", 2))
			score += 5000;
		else
			score += 6000;
	}
	else if ( !_strnicmp(s, "read me", 7) )
	{
		if (s[7] == '-' && _strnicmp(s+8, "en", 2))
			score += 5000;
		else
			score += 6000;
	}
	// contains "readme" anywhere
	else if ( stristr(s, "readme") )
	{
		if ( !stristr(s, "readme-en") )
			score += 3000;
		else
			score += 4000;
	}
	else if ( stristr(s, "read me") )
	{
		if ( !stristr(s, "read me-en") )
			score += 3000;
		else
			score += 4000;
	}

	return score;
}

static __inline bool compare_docfiles(const char *a, const char *b)
{
	const int ascore = GetFileScore(a);
	const int bscore = GetFileScore(b);
	return (ascore != bscore) ? (ascore > bscore) : (_stricmp(a, b) < 0);
}

static BOOL GetDocFilesFromArch(FMEntry *fm, vector<string> &list)
{
	if (fm->flags & FMEntry::FLAG_CachedInfoFiles)
	{
		list = fm->infoFilesCache;
		return !list.empty();
	}

	int i;
	vector<string> files;

	int nFiles = ListFilesInArchiveRoot(fm->GetArchiveFilePath().c_str(), files);
	if (nFiles <= 0)
		return FALSE;

	const int nTypes = sizeof(g_doctypes)/sizeof(g_doctypes[0]);

	vector<const char *> tmplist;
	tmplist.reserve(16);

	for (i=0; i<nFiles; i++)
	{
		const string &str = files[i];
		const char *s = str.c_str();
		int len = str.length();

		// get extension
		while (len && s[len] != '.' && !isdirsep(s[len])) len--;
		if (s[len] != '.' || !s[len+1])
			continue;

		const char *ext = s + len + 1;

		// check if extension matches any of the requested ones
		for (int j=0; j<nTypes; j++)
			if ( !_stricmp(ext, g_doctypes[j]) )
				tmplist.push_back(s);
	}

	if (tmplist.size() > 1)
		// sort files so most likely info file is first
		std::sort(tmplist.begin(), tmplist.end(), compare_docfiles);

	// copy temp list to 'list'
	list.reserve( tmplist.size() );
	for (i=0; i<(int)tmplist.size(); i++)
		list.push_back(tmplist[i]);

	fm->infoFilesCache = list;
	fm->flags |= FMEntry::FLAG_CachedInfoFiles;

	return !list.empty();
}

static BOOL GetDocFiles(FMEntry *fm, vector<string> &list)
{
	int i;
	char fname[MAX_PATH_BUF];

	if ( !fm->IsInstalled() )
	{
		if ( fm->IsArchived() )
			return GetDocFilesFromArch(fm, list);

		return FALSE;
	}

	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
		return FALSE;

	dirent **files;
	int nFiles = fl_filename_list(fname, &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return FALSE;

	const int nTypes = sizeof(g_doctypes)/sizeof(g_doctypes[0]);

	// build temp list with direct string pointers so sorting doesn't have to mess with 'string' copy/construction stuff
	vector<const char *> tmplist;
	tmplist.reserve(16);

	for (i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		// skip dirs
		int len = strlen(f->d_name);
		if ( isdirsep(f->d_name[len-1]) )
			continue;

		// get extension
		while (len && f->d_name[len] != '.' && !isdirsep(f->d_name[len])) len--;
		if (f->d_name[len] != '.' || !f->d_name[len+1])
			continue;

		const char *ext = f->d_name + len + 1;

		// check if extension matches any of the requested ones
		for (int j=0; j<nTypes; j++)
			if ( !_stricmp(ext, g_doctypes[j]) )
				tmplist.push_back(f->d_name);
	}

	if (tmplist.size() > 1)
		// sort files so most likely info file is first
		std::sort(tmplist.begin(), tmplist.end(), compare_docfiles);

	// copy temp list to 'list'
	list.reserve( tmplist.size() );
	for (i=0; i<(int)tmplist.size(); i++)
		list.push_back(tmplist[i]);

	// free data
	fl_filename_free_list(&files, nFiles);

	return !list.empty();
}

static BOOL AutoSelectInfoFile(FMEntry *fm, BOOL bSetModified = FALSE)
{
	vector<string> list;

	if ( !fm->IsAvail() )
		return FALSE;

	if ( GetDocFiles(fm, list) )
	{
		fm->infofile = list[0].c_str();
		fm->flags &= ~FMEntry::FLAG_NoInfoFile;
		if (bSetModified)
			fm->OnModified();
		else
			fm->flags |= FMEntry::FLAG_PendingInfoFile;
		return TRUE;
	}
	else
	{
		fm->infofile.clear();
		fm->flags |= FMEntry::FLAG_NoInfoFile;
		if (bSetModified)
			fm->OnModified();
		else
			fm->flags |= FMEntry::FLAG_PendingInfoFile;
		return FALSE;
	}
}


// delete dir recursively including the leaf dir, USE WITH CARE!
// ('path' must be have been cleaned with CleanDirSlashes and contain no trailing slash)
static BOOL DelTreeInternal(const char *path)
{
	// if the path length is already at max, then fail, because we can't possibly append file or subdir names
	if (strlen(path) >= MAX_PATH)
		return FALSE;

	dirent **files;
	int nFiles = fl_filename_list(path, &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
	{
		if ( !_rmdir(path) )
			return TRUE;
		ASSERT(errno == ENOENT);
		return errno == ENOENT;
	}

	BOOL bRet = TRUE;

	string s;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		int len = strlen(f->d_name);
		if ( isdirsep(f->d_name[len-1]) )
		{
			// recurse into subdir

			if (strcmp(f->d_name, "./") && strcmp(f->d_name, "../"))
			{
				f->d_name[len-1] = 0;

				s = path;
				s.append(DIRSEP);
				s.append(f->d_name);

				if ( !DelTreeInternal( s.c_str() ) )
					bRet = FALSE;
			}
		}
		else
		{
			// delete file

			s = path;
			s.append(DIRSEP);
			s.append(f->d_name);

			if (s.length() > MAX_PATH || remove_forced( s.c_str() ))
			{
				ASSERT(FALSE);
				bRet = FALSE;
			}
		}
	}

	fl_filename_free_list(&files, nFiles);

	if ( _rmdir(path) )
	{
		ASSERT(FALSE);
		bRet = FALSE;
	}

	return bRet;
}

static BOOL DelTree(const char *path)
{
	ShowBusyCursor(TRUE);

	BOOL ret = DelTreeInternal(path);

	ShowBusyCursor(FALSE);

	return ret;
}

static BOOL DelTree(const string &path)
{
	return DelTree( path.c_str() );
}


static BOOL IsSafeFmDir(FMEntry *fm)
{
	ASSERT(fm != NULL);

	// make sure the fm name produce a safe dir name for install and deltree operations
	// (ie. name is not empty and does not contain dir separators)

	string s = Trimmed(fm->name);

	if (!s.size() || s.find_first_of("/\\") != string::npos)
		return FALSE;

	return TRUE;
}

static BOOL FmDelTree(FMEntry *fm)
{
	if ( g_sTempDir.empty() )
		return FALSE;

	char installdir[MAX_PATH_BUF];
	if (_snprintf_s(installdir, sizeof(installdir), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	// double and triple check that we produce a proper FM dir, since we delete dir recursively and
	// don't want any disasters to happen
	if ( !IsSafeFmDir(fm) )
	{
		ASSERT(FALSE);
		return FALSE;
	}

	CleanDirSlashes(installdir);

	// first we move the FM dir to our trash can (our temp dir)
	string tmpdir;
	if ( !GetTempFile(fm->name, tmpdir, TRUE) )
	{
		// uninstall failed, could not get tmp dir name (should never happen)
		ASSERT(FALSE);
		return FALSE;
	}
	if ( rename(installdir, tmpdir.c_str()) )
	{
		const int err = errno;
		if (err != ENOENT)
		{
			// uninstall failed, could not move dir to trash
			TRACE("failed to move dir \"%s\" to \"%s\" (%d)", installdir, tmpdir.c_str(), err);
			return FALSE;
		}
	}

	fm->flags &= ~FMEntry::FLAG_Installed;

	// we don't care if delete fails, it's in the trash already
	DelTree( tmpdir.c_str() );

	return TRUE;
}

static BOOL BackupOptFileToArchive(FMEntry *fm, const char *fname, BOOL bSkipCheck = FALSE)
{
	// if file doesn't exist then return TRUE

	char pathname[MAX_PATH_BUF];
	if (_snprintf_s(pathname, sizeof(pathname), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name, fname) == -1)
		return FALSE;

	if (!bSkipCheck)
	{
		struct stat st = {0};
		if ( stat(pathname, &st) )
			return TRUE;
	}

	FileQ file;
	file.fname = _strdup(pathname);
	file.fname_rel = file.fname + (strlen(pathname) - strlen(fname));

	g_backupList.push_back(file);

	file.fname = NULL;

	return TRUE;
}

static BOOL BackupOptDirToArchive(FMEntry *fm, const char *dirname)
{
	// if dir doesn't exist then return TRUE

	string s = g_pFMSelData->sRootPath;
	s.append(DIRSEP);
	s.append(fm->name);
	s.append(DIRSEP);
	s.append(dirname);

	dirent **files;
	int nFiles = fl_filename_list(s.c_str(), &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return TRUE;

	BOOL bRet = TRUE;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		if (bRet)
		{
			int len = strlen(f->d_name);
			if ( isdirsep(f->d_name[len-1]) )
			{
				// recurse into subdir

				if (strcmp(f->d_name, "./") && strcmp(f->d_name, "../"))
				{
					f->d_name[len-1] = 0;

					s = dirname;
					s.append(DIRSEP);
					s.append(f->d_name);

					if ( !BackupOptDirToArchive(fm, s.c_str()) )
						bRet = FALSE;
				}
			}
			else
			{
				// file

				s = dirname;
				s.append(DIRSEP);
				s.append(f->d_name);

				if ( !BackupOptFileToArchive(fm, s.c_str(), TRUE) )
					bRet = FALSE;
			}
		}
	}

	fl_filename_free_list(&files, nFiles);

	return bRet;
}

static void* BackupSavesThread(void *p)
{
	for (std::list<FileQ>::iterator it=g_backupList.begin(); it!=g_backupList.end(); it++, StepProgress(1))
		if ( !AddFileToArchive(it->fname, it->fname_rel) )
		{
			EndProgress(0);
			return 0;
		}

	EndProgress(1);

	return (void*)1;
}

static BOOL BackupSavesToArchive(FMEntry *fm)
{
	BOOL bRes = FALSE;

	string bakarchive = fm->GetBakArchiveFilePath();

	ShowBusyCursor(TRUE);

	// TODO: a progress dialog for this might be nice (we'd have to enumerate all files in advances, so we know
	//       the filecount, and then do archiving in a thread

	if (!g_bRunningShock)
	{
		// not necessary, it's only an internal temp file for thief (it gets created each time a savegame is loaded
		// or a mission is started)
		/*// thief checkpoint save
		if ( !BackupOptFileToArchive(fm, "startmis.sav") )
			goto abort;*/

		// thief saves
		if ( !BackupOptDirToArchive(fm, "saves") )
			goto abort;
	}

	// shock saves "save_*" and also "saves_*" for potential future thief multiplayer saves
	{
	char fname[MAX_PATH_BUF];
	if (_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
		goto abort;
	dirent **files;
	int nFiles = fl_filename_list(fname, &files, NO_COMP_UTFCONV);
	if (nFiles > 0)
	{
		BOOL bFailed = FALSE;

		for (int i=0; i<nFiles; i++)
		{
			dirent *f = files[i];

			if (!bFailed)
			{
				int len = strlen(f->d_name);
				if (isdirsep(f->d_name[len-1]) && f->d_name[0] != '.')
				{
					f->d_name[len-1] = 0;
					if (!_strnicmp(f->d_name, "save_", 5) || !_strnicmp(f->d_name, "saves_", 6))
						if ( !BackupOptDirToArchive(fm, f->d_name) )
							bFailed = TRUE;
				}
			}
		}

		fl_filename_free_list(&files, nFiles);

		if (bFailed)
			goto abort;
	}
	}

	// screenshots
	if ( !BackupOptDirToArchive(fm, "screenshots") )
		goto abort;

	if ( g_backupList.empty() )
	{
		// nothing to backup
		bRes = TRUE;
		goto abort;
	}

	if ( !BeginCreateArchive( bakarchive.c_str() ) )
		goto abort;

	InitProgress(g_backupList.size(), $("Archiving backup..."));

	if ( !CreateThreadOS(BackupSavesThread, NULL) )
	{
		// if thread creation fails then run non-threaded (without progress bar), shouldn't normally happen
		TermProgress();
		bRes = (BackupSavesThread(NULL) != NULL);
	}
	else
		bRes = RunProgress();

	bRes = EndCreateArchive(!bRes);

abort:
	g_backupList.clear();

	ShowBusyCursor(FALSE);

	return bRes;
}

static __inline bool compare_diffinfo_relname(const FileDiffInfo *a, const FileDiffInfo *b)
{
	// root files last
	if ( !strchr(a->fname_rel, *DIRSEP) )
	{
		if ( strchr(b->fname_rel, *DIRSEP) )
			return false;
	}
	else if ( !strchr(b->fname_rel, *DIRSEP) )
		return true;

	return _stricmp(a->fname_rel, b->fname_rel) < 0;
}

struct BackupDiffSetContext
{
	FMEntry *fm;
	vector<const FileDiffInfo*> *changed;
};
static void* BackupDiffSetThread(void *p)
{
	BackupDiffSetContext *ctxt = (BackupDiffSetContext*)p;
	FMEntry *fm = ctxt->fm;
	vector<const FileDiffInfo*> &changed = *ctxt->changed;

	for (int i=0; i<(int)changed.size(); i++, StepProgress(1))
		if ( !AddFileToArchive(changed[i]->fname, changed[i]->fname_rel) )
		{
			EndProgress(0);
			return 0;
		}

	if ( !g_removedFiles.empty() )
	{
		// include list of deleted files in archive (we write those to fmsel.inf, since that file doesn't exist in
		// installs or backups otherwise)

		string fname;
		if ( GetTempFile("fmsel_remove.tmp", fname, TRUE) )
		{
			FILE *f = fopen(fname.c_str(), "wb");
			if (f)
			{
				for (std::list<string>::iterator it=g_removedFiles.begin(); it!=g_removedFiles.end(); it++)
					fprintf(f, "RemoveFile=%s\r\n", it->c_str());

				fclose(f);

				AddFileToArchive(fname.c_str(), "fmsel.inf");
				remove_forced( fname.c_str() );
			}
		}

		StepProgress(1);
	}

	EndProgress(1);

	return (void*)1;
}

static BOOL BackupDiffSet(FMEntry *fm)
{
	if (g_fileDiffInfoMap.empty() && g_removedFiles.empty())
		return TRUE;

	string bakarchive = fm->GetBakArchiveFilePath();

	if ( !BeginCreateArchive( bakarchive.c_str() ) )
		return FALSE;

	BOOL bRes = FALSE;

	int i;

	// build sorted list of backup files
	vector<const FileDiffInfo*> changed;
	changed.reserve( g_fileDiffInfoMap.size() );
	for (tFileDiffInfoHash::iterator it=g_fileDiffInfoMap.begin(); it!=g_fileDiffInfoMap.end(); it++)
		changed.push_back(&it->second);
	std::sort(changed.begin(), changed.end(), compare_diffinfo_relname);

	// optionally review changes
	if (g_cfg.bReviewDiffBackup)
	{
		string html;

		if ( !changed.empty() )
		{
			html.append("<br><b><u>");
			html.append($("Changed/Added Files"));
			html.append(":</u></b><br><br>");

			for (i=0; i<(int)changed.size(); i++)
			{
				html.append( TextToHtml(changed[i]->fname_rel) );
				html.append("<br>");
			}
		}

		if ( !g_removedFiles.empty() )
		{
			html.append("<br><b><u>");
			html.append($("Removed Files"));
			html.append(":</u></b><br><br>");

			for (std::list<string>::iterator it=g_removedFiles.begin(); it!=g_removedFiles.end(); it++)
			{
				html.append( TextToHtml(it->c_str()) );
				html.append("<br>");
			}
		}

		html.append("<br><br><b><a href=\"/cmd/accept\" color=\"");
		html.append(DARK() ? "#009A4F\">" : "#36DB8B\">");
		html.append($("Continue with Backup"));
		html.append("</a></b>");

		const char *ret = GenericHtmlTextPopup($("Queued Files for Differential Backup"), html.c_str());
		if (!ret || strcmp(ret, "/cmd/accept"))
			goto abort;
	}

	//

	InitProgress(changed.size() + (g_removedFiles.empty() ? 0 : 1), $("Archiving backup..."));

	BackupDiffSetContext ctxt;
	ctxt.fm = fm;
	ctxt.changed = &changed;

	if ( !CreateThreadOS(BackupDiffSetThread, &ctxt) )
	{
		// if thread creation fails then run non-threaded (without progress bar), shouldn't normally happen
		TermProgress();
		bRes = (BackupDiffSetThread(&ctxt) != NULL);
	}
	else
		bRes = RunProgress();

abort:
	if ( !EndCreateArchive(!bRes) )
		bRes = FALSE;

	TermProgress();

	return bRes;
}

static void FormatFileSizeValue(char *s, unsigned __int64 sz)
{
	const unsigned __int64 K = 1024;

	unsigned __int64 fz = sz;
	sz /= K;
	if (sz <= K)
	{
		sz = (fz + 512) / K;
		sprintf(s, $("%d kB"), (unsigned int)sz);
		return;
	}

	fz = sz;
	sz /= K;
	if (sz <= K)
	{
		sz = (fz + 512) / K;
		sprintf(s, $("%d MB"), (unsigned int)sz);
		return;
	}

	fz = sz;
	sz /= K;
	if (sz <= K)
	{
		sprintf(s, $("%d.%d GB"), (unsigned int)sz, (unsigned int)(((fz%1024)*10)/1024));
		return;
	}

	fz = sz;
	sz /= K;
	sprintf(s, $("%d.%d TB"), (unsigned int)sz, (unsigned int)(((fz%1024)*10)/1024));
}

static bool RemoveEnumeratedArchiveFile(const char *fname, void *p)
{
	ASSERT(p != NULL);

	const char *installdir = (const char*)p;
	char s[MAX_PATH_BUF];

	if (_snprintf_s(s, sizeof(s), _TRUNCATE, "%s" DIRSEP "%s", installdir, fname) == -1)
	{
		ASSERT(FALSE);
		return true;
	}

	remove_forced(s);

	return true;
}

static bool EnumMP3Files(const char *fname, void *p)
{
	ASSERT(p != NULL);

	std::list<string> &list = *(std::list<string>*)p;

	const int n = strlen(fname);
	if (n > 4 && !_stricmp(fname+n-4, ".mp3"))
		list.push_back(fname);

	return true;
}

static bool EnumMP3OGGFiles(const char *fname, void *p)
{
	ASSERT(p != NULL);

	std::list<string> *list = (std::list<string>*)p;

	const int n = strlen(fname);
	if (n > 4)
	{
		if ( !_stricmp(fname+n-4, ".mp3") )
			list[0].push_back(fname);
		else if ( !_stricmp(fname+n-4, ".ogg") )
			list[1].push_back(fname);
	}

	return true;
}

struct MP3Context
{
	std::list<string> *mp3files;
	const char *installdir;
};

static void* MP3Thread(void *p)
{
	MP3Context *ctx = (MP3Context*)p;

	std::list<string> &mp3files = *ctx->mp3files;
	const char *installdir = ctx->installdir;

	string mp3, wav;
	BOOL bOK = TRUE;

	for (std::list<string>::iterator it=mp3files.begin(); it!=mp3files.end(); it++, StepProgress(1))
	{
		mp3 = installdir;
		mp3 += DIRSEP;
		mp3 += *it;

		wav = mp3.substr(0, mp3.length()-3);
		wav += "wav";

		hip_t lh = lame_decode_init();
		if (!lh)
		{
			bOK = FALSE;
			continue;
		}

		FILE *fmp3 = fopen(mp3.c_str(), "rb");
		if (!fmp3)
		{
			bOK = FALSE;
			lame_decode_exit(lh);
			continue;
		}
		FILE *fwav = fopen(wav.c_str(), "wb");
		if (!fwav)
		{
			bOK = FALSE;
			fclose(fmp3);
			lame_decode_exit(lh);
			continue;
		}

		if ( !InitMp3File(fmp3) )
		{
			// not recognized as an mp3 file, rename to wav and hope for the best

			fclose(fmp3);
			fclose(fwav);

			// delete failed wav
			remove_forced( wav.c_str() );

			rename(mp3.c_str(), wav.c_str());

			lame_decode_exit(lh);

			continue;
		}

		InitWavFile(fwav);

		sMp3data mp3data = {0};
		short pcm_l[1152] = {0}, pcm_r[1152] = {0};
		short pcm_lr[1152*2];
		unsigned char buf[1024];
		int r = 0, l = 0;
		int skip_start = 0, skip_end = 0;

		// decode file

		BOOL bFileFailed = FALSE;

		while (!mp3data.header_parsed)
		{
			l = fread(buf, 1, sizeof(buf), fmp3);
			if (l < 0)
			{
				bFileFailed = TRUE;
				break;
			}
			r = lame_decode1_headersB(lh, buf, l, pcm_l, pcm_r, &mp3data, &skip_start, &skip_end);
			if (r < 0)
			{
				bFileFailed = TRUE;
				break;
			}
		}

		// sanity check to avoid crashes
		if (mp3data.channels < 0 || mp3data.channels > 2
			|| mp3data.frame_size < 0 || mp3data.frame_size > 1152)
			bFileFailed = TRUE;

		const int nSampleRate = mp3data.sample_rate;
		const int nChannels = mp3data.channels>1 ? 2 : 1;

		if (!bFileFailed)
		{
			if (r)
				goto output_data;

			do
			{
				while (l > 0 && r <= 0)
				{
					l = fread(buf, 1, sizeof(buf), fmp3);
					if (l < 0)
					{
						bFileFailed = TRUE;
						break;
					}
					r = lame_decode1_headers(lh, buf, l, pcm_l, pcm_r, &mp3data);
					ASSERT(r <= 1152);
					if (r < 0)
					{
						bFileFailed = TRUE;
						break;
					}
					if (!r)
						r = lame_decode1_headers(lh, buf, 0, pcm_l, pcm_r, &mp3data);
				}

				if (bFileFailed)
					break;

output_data:
				do
				{
					if (nChannels > 1)
					{
						// write L/R interleaved
						const short *left = pcm_l, *right = pcm_r;
						short *lr = pcm_lr;
						for (int i=0; i<r; i++)
						{
							*lr++ = *left++;
							*lr++ = *right++;
						}
						fwrite(pcm_lr, sizeof(short), r*2, fwav);
					}
					else
						fwrite(pcm_l, sizeof(short), r, fwav);

					// check for buffered data still in the decoder
					r = lame_decode1_headers(lh, buf, 0, pcm_l, pcm_r, &mp3data);
				}
				while (r > 0);

				if (r < 0)
				{
					bFileFailed = TRUE;
					break;
				}

				r = 0;
			}
			while (l > 0);
		}

		//

		if (bFileFailed || !FinalizeWavFile(fwav, nSampleRate, nChannels))
		{
			bOK = FALSE;

			fclose(fmp3);
			fclose(fwav);

			// delete failed wav
			remove_forced( wav.c_str() );
		}
		else
		{
			fclose(fmp3);
			fclose(fwav);

			CloneFileMTimeOS(mp3.c_str(), wav.c_str());

			// delete mp3
			remove_forced( mp3.c_str() );
		}

		lame_decode_exit(lh);
	}

	if (!bOK)
	{
		EndProgress(0);
		return 0;
	}

	EndProgress(1);

	return (void*)1;
}

struct OGGContext
{
	std::list<string> *oggfiles;
	const char *installdir;
};

static void* OGGThread(void *p)
{
	OGGContext *ctx = (OGGContext*)p;

	std::list<string> &oggfiles = *ctx->oggfiles;
	const char *installdir = ctx->installdir;

	string ogg, wav;
	BOOL bOK = TRUE;

	for (std::list<string>::iterator it=oggfiles.begin(); it!=oggfiles.end(); it++, StepProgress(1))
	{
		ogg = installdir;
		ogg += DIRSEP;
		ogg += *it;

		wav = ogg.substr(0, ogg.length()-3);
		wav += "wav";

		FILE *fogg = fopen(ogg.c_str(), "rb");
		if (!fogg)
		{
			bOK = FALSE;
			continue;
		}
		FILE *fwav = fopen(wav.c_str(), "wb");
		if (!fwav)
		{
			bOK = FALSE;
			fclose(fogg);
			continue;
		}

		if ( !ConvertOggFile(fogg, fwav) )
		{
			// conversion failed

			bOK = FALSE;

			fclose(fogg);
			fclose(fwav);

			// delete failed wav
			remove_forced( wav.c_str() );
		}
		else
		{
			fclose(fogg);
			fclose(fwav);

			CloneFileMTimeOS(ogg.c_str(), wav.c_str());

			// delete ogg
			remove_forced( ogg.c_str() );
		}
	}

	if (!bOK)
	{
		EndProgress(0);
		return 0;
	}

	EndProgress(1);

	return (void*)1;
}

static BOOL ConvertMP3Files(std::list<string> &mp3files, const char *installdir)
{
	if ( mp3files.empty() )
		return TRUE;

	InitProgress(mp3files.size(), $("Converting MP3s..."));

	BOOL bRes;

	MP3Context ctx = { &mp3files, installdir };

	if ( !CreateThreadOS(MP3Thread, &ctx) )
	{
		// if thread creation fails then run non-threaded (without progress bar), shouldn't normally happen
		TermProgress();
		bRes = (MP3Thread(&ctx) != NULL);
	}
	else
		bRes = RunProgress();

	if (!bRes)
		if ( !fl_choice($("MP3 conversion failed partially or completely, proceed anyway?"), fl_cancel, fl_ok, NULL) )
			return FALSE;

	return TRUE;
}

static BOOL ConvertOGGFiles(std::list<string> &oggfiles, const char *installdir)
{
	if ( oggfiles.empty() )
		return TRUE;

	InitProgress(oggfiles.size(), $("Converting OGGs..."));

	BOOL bRes;

	OGGContext ctx = { &oggfiles, installdir };

	if ( !CreateThreadOS(OGGThread, &ctx) )
	{
		// if thread creation fails then run non-threaded (without progress bar), shouldn't normally happen
		TermProgress();
		bRes = (OGGThread(&ctx) != NULL);
	}
	else
		bRes = RunProgress();

	if (!bRes)
		if ( !fl_choice($("OGG conversion failed partially or completely, proceed anyway?"), fl_cancel, fl_ok, NULL) )
			return FALSE;

	return TRUE;
}


static BOOL rename_instdir_safe(const char *src, const char *dst)
{
	// pauses and retries a few times if rename fails
	if ( rename(src, dst) )
	{
		WaitOS(500);
		if ( rename(src, dst) )
		{
			WaitOS(1000);
			if ( rename(src, dst) )
			{
				WaitOS(1000);
				if ( rename(src, dst) )
				{
retry:
					// notify user and allow one user-invoked retry
					const int ret = fl_choice(
						$("Failed to move the FM directory from the temp extraction location to the FM path:\n"
						"\"%s\" -> \"%s\"\n"
						"\n"
						"Another application could be locking the temp dir or FM path. You may want to check that\n"
						"or wait until disk activity settles and retry."),
						fl_cancel, $("Retry"), NULL, src, dst);

					if (ret && !rename(src, dst))
						return TRUE;

					if (ret)
						goto retry;

					return FALSE;
				}
			}
		}
	}

	return TRUE;
}


/*
static BOOL GetLanguagePacks(const char *archivepath, std::list<string> &langpacks)
{
	// check if archive contains language packs inside (archive files in the root that are named as a language)

	std::vector<string> list;

	if (ListFilesInArchiveRoot(archivepath, list) <= 0)
		return FALSE;

	string s;

	for (int i=0; i<(int)list.size(); i++)
	{
		const char *ext = strrchr(list[i].c_str(), '.');
		if (ext && IsArchiveFormatSupported(ext+1))
		{
			// check if archive is named as a language

			// TODO: this won't work in practice, as there's no strict naming on the language archives (some even with
			//       typos), some other solution would be needed, either listing all archives or some extended user
			//       editable list

			s = list[i].substr(0, (int)(ext - list[i].c_str()));

			for (int j=0; j<NUM_LANGUAGES; j++)
				if ( !_stricmp(s.c_str(), g_languages[j]) )
				{
					langpacks.push_back(list[i]);
					break;
				}
		}
	}

	return !langpacks.empty();
}
*/


static void CheckMissionFlags(const char *installdir)
{
#ifdef _WIN32
	const char *strdirname = "strings";
#else
	char strdirname[8] = "STRINGS";
#endif
	const char *missflagfile = "missflag.str";

	char fpath[MAX_PATH_BUF];
	if (_snprintf_s(fpath, sizeof(fpath), _TRUNCATE, "%s" DIRSEP "%s", installdir, strdirname) == -1)
		return;

	if ( !fl_filename_isdir_ex(fpath, TRUE) )
	{
#ifndef _WIN32
		for (char *c = strdirname; *c != '\0'; c++)
			*c = tolower(*c);
		if (snprintf(fpath, sizeof(fpath), "%s" DIRSEP "%s", installdir, strdirname) == -1)
			return;
		if ( !fl_filename_isdir_ex(fpath, TRUE) )
		{
#endif
			if ( _mkdir(fpath) )
				return;
#ifndef _WIN32
		}
#endif
	}

	BOOL bMissFlagsExist = FALSE;
	dirent **files;
	int nFiles = fl_filename_list(fpath, &files, NO_COMP_UTFCONV);
	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		int len = strlen(f->d_name);
		if (f->d_name[0] != '.' && isdirsep(f->d_name[len-1]))
		{
			if (strlen(fpath)+strlen(f->d_name)+1 >= MAX_PATH)
				break;

			strcat(fpath, DIRSEP);
			strcat(fpath, f->d_name);

			dirent **sfiles;
			int nSfiles = fl_filename_list(fpath, &sfiles, NO_COMP_UTFCONV);
			for (int j=0; j<nSfiles; j++)
			{
				f = sfiles[j];

				if ( stristr(f->d_name, missflagfile) )
				{
					bMissFlagsExist = TRUE;
					break;
				}
			}

			fl_filename_free_list(&sfiles, nSfiles);
		}
		else
		{
			if ( stristr(f->d_name, missflagfile) )
			{
				bMissFlagsExist = TRUE;
				break;
			}
		}

		if (bMissFlagsExist)
			break;
	}

	fl_filename_free_list(&files, nFiles);

	if (bMissFlagsExist)
		return;

	if (_snprintf_s(fpath, sizeof(fpath), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP "%s", installdir, strdirname, missflagfile) == -1)
		return;

	vector<unsigned char> misnums;

	nFiles = fl_filename_list(installdir, &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];
		string fname = f->d_name;

		size_t pre = fname.find("miss");
		size_t ext = fname.find(".mis", 4);
		if (pre != string::npos && ext != string::npos && pre == 0 && fname.size() == ext + 4)
		{
			if (isdigit(fname[4]) && ((isdigit(fname[5]) && ext == 6) || ext == 5))
			{
				int misnum = atoi(fname.substr(4, ext - 4).c_str());
				if (misnum > 0 && misnum <= 99)
					misnums.push_back((unsigned char)misnum);
			}
		}
	}

	fl_filename_free_list(&files, nFiles);

	if (misnums.size() == 0)
		return;

	sort(misnums.begin(), misnums.end());

	string str = "";
	for (unsigned char i = 1; i <= misnums.back(); i++)
	{
		str.append("miss_");
		char misnum[4];
		_snprintf_s(misnum, sizeof(misnum), _TRUNCATE, "%u", i);
		str.append(misnum);
		str.append(": ");
		if ( count(misnums.begin(), misnums.end(), i) )
		{
			str.append("\"no_briefing, no_loadout");
			if (i == misnums.back())
				str.append(", end");
		}
		else
		{
			str.append("\"skip");
		}
		str.append("\"\r\n");
	}

	FILE *f = fopen(fpath, "wb");
	if (!f)
	{
		fl_alert($("Failed to open file \"%s\" for writing."), fpath);
		return;
	}

	fprintf(f, str.c_str());

	fclose(f);
}


static BOOL InstallFM(FMEntry *fm)
{
	ASSERT( !fm->IsInstalled() );

	if ( g_sTempDir.empty() )
	{
		// shouldn't get here
		fl_alert($("No temp/cache directory available, cannot install."));
		return FALSE;
	}

	if ( !IsSafeFmDir(fm) )
	{
		fl_alert($("FM directory name \"%s\" is invalid, cannot install."), fm->name);
		return FALSE;
	}

	string archivepath = fm->GetArchiveFilePath();
	string bakarchive = fm->GetBakArchiveFilePath();

	// make sure the archive is available
	struct stat st = {0};
	if (stat(archivepath.c_str(), &st) || !(st.st_mode & S_IREAD))
	{
		fl_alert($("Failed to find archive file \"%s\"."), archivepath.c_str());
		return FALSE;
	}

	char installdir[MAX_PATH_BUF];
	if (_snprintf_s(installdir, sizeof(installdir), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
	{
		fl_alert($("Cannot install, path too long \"%s\"."), installdir);
		return FALSE;
	}

	// ensure that install dir is gone (if there is one and it isn't an install of this fm, which it can't be if we
	// end up in this install function, then something is very screwy)
	if ( !stat(installdir, &st) )
	{
		fl_alert($("Cannot install, there already is a directory \"%s\"."), installdir);
		return FALSE;
	}

	// generate tmp dir to extract to
	string tmpdir;
	if ( !GetTempFile(fm->name, tmpdir, TRUE) )
	{
		// uninstall failed, could not get tmp dir name (should never happen)
		ASSERT(FALSE);
		return FALSE;
	}

	// get list of MP3s and if needed OGGs
	std::list<string> compressedSndFiles[2];
	std::list<string> &mp3files = compressedSndFiles[0];
	std::list<string> &oggfiles = compressedSndFiles[1];
#ifdef OGG_SUPPORT
	EnumFullArchive(archivepath.c_str(), g_cfg.bDecompressOGG ? EnumMP3OGGFiles : EnumMP3Files, &compressedSndFiles[0]);
#else
	EnumFullArchive(archivepath.c_str(), EnumMP3Files, &compressedSndFiles[0]);
#endif

	BOOL bSupportsMP3 = FALSE;
	if ( !mp3files.empty() )
	{
		bSupportsMP3 = InitMP3();
		if (!bSupportsMP3
			&& !fl_choice($("MP3 to WAV conversion not possible, download %s (see About box).\nFM will not work properly, install anyway?"), fl_cancel, fl_ok, NULL, MP3LIB))
			return FALSE;
	}

	// get list of language packs
	/*std::list<string> langpacks;
	const BOOL bHasLangPacks = GetLanguagePacks(archivepath.c_str(), langpacks);*/

	//

	const unsigned __int64 MB = (unsigned __int64)1024 * (unsigned __int64)1024;
	const unsigned __int64 MIN_FREE_MB = 16;
	const unsigned __int64 MIN_PROGRESS_MB = 10;

	unsigned __int64 sz = 0, szBak = 0, disk = 0;
	unsigned int foo;
	// determine size of unpacked archive
	const BOOL szOk = GetUnpackedArchiveSize(archivepath.c_str(), sz, foo);
	// determine size of unpacked backup files
	const BOOL szBakOk = GetUnpackedArchiveSize(bakarchive.c_str(), szBak, foo, true);
	// determine free disk size
	const BOOL bDiskOk = GetFreeDiskSpaceOS(g_pFMSelData->sRootPath, disk);

	// skip progress bar for install when (unpacked) archive size is very small
	const BOOL bShowProgress = !szOk || sz >= (MIN_PROGRESS_MB * MB);
	const BOOL bShowBakProgress = !szBakOk || szBak >= (MIN_PROGRESS_MB * MB);

	char s1[256], s2[64];
	FormatFileSizeValue(s1, sz+szBak);
	if (bDiskOk)
		FormatFileSizeValue(s2, disk * MB);
	else
		strcpy(s2, $("N/A"));

	if (!mp3files.empty() && bSupportsMP3)
		sprintf(s1+strlen(s1), $(" + %d MP3 file(s), size unknown"), mp3files.size());
	if ( !oggfiles.empty() )
		sprintf(s1+strlen(s1), $(" + %d OGG file(s), size unknown"), oggfiles.size());

	// TODO: if bHasLangPacks then display a fancier install dialog with a droplist of available lang packs (and "None"),
	//       defaulting to the language last installed if possible (when the lang pack gets extracted after install it
	//       needs to check for MP3 files there too, and differential backups need to make sure files don't belong to
	//       the language pack, ick!)

	if (bDiskOk && szOk && disk < ((sz+szBak)/MB) + MIN_FREE_MB)
	{
		// low diskspace warning
		if ( !fl_choice(
			$("WARNING: You will/may run out of disk space!\n"
			"\n"
			"Install/Extract FM from archive anyway?\n"
			"\n"
			"Est. install size  : %s\n"
			"Free disk space: %s (!)"),
			fl_cancel, fl_ok, NULL, s1, s2) )
				return FALSE;
	}
	else
	{
		if ( !fl_choice(
			$("Install/Extract FM from archive?\n"
			"\n"
			"Est. install size  : %s\n"
			"Free disk space: %s"),
			fl_cancel, fl_ok, NULL, s1, s2) )
				return FALSE;
	}

	//

	const char *pErrMsg = NULL;

	BOOL bRet = ExtractFullArchive(archivepath.c_str(), tmpdir.c_str(), bShowProgress ? $("Installing...") : NULL, &pErrMsg);
	if (!bRet)
	{
		DelTree( tmpdir.c_str() );

		fl_alert($("Failed to extract FM archive, install aborted.\n\nError: %s"), pErrMsg ? pErrMsg : $("unknown error"));

		return FALSE;
	}
	else if (bRet == 2)
	{
		// partial failure, ask if user wants to risk it anyway
		if ( !fl_choice(
			$("Partially failed to extract FM archive.\n\nError: %s\nInstall FM anyway?"),
			fl_no, fl_yes, NULL, pErrMsg ? pErrMsg : $("unknown error")) )
		{
			DelTree( tmpdir.c_str() );
			return FALSE;
		}
	}

	// convert MP3s to WAVs
	if (bSupportsMP3 && !ConvertMP3Files(mp3files, tmpdir.c_str()))
	{
		DelTree( tmpdir.c_str() );

		return FALSE;
	}
	// convert OGGs to WAVs
	if ( !ConvertOGGFiles(oggfiles, tmpdir.c_str()) )
	{
		DelTree( tmpdir.c_str() );

		return FALSE;
	}

	// generate Thief mission flags
	if (g_cfg.bGenerateMissFlags && !g_bRunningShock)
		CheckMissionFlags(tmpdir.c_str());

	// restore savegames/screenshots
	if ( !stat(bakarchive.c_str(), &st) )
	{
		pErrMsg = NULL;

		// make sure there's no fmsel.inf in the main FM archive (with potentially malicious content to remove files
		// outside of install dir)
		const BOOL bInstallInfoSafe = !remove_forced( (tmpdir + DIRSEP "fmsel.inf").c_str() ) || errno == ENOENT;

		if (ExtractFullArchive(bakarchive.c_str(), tmpdir.c_str(), bShowBakProgress ? $("Restoring backup...") : NULL, &pErrMsg) != 1)
		{
			// extraction failed completely or partially
			if ( fl_choice(
				$("Failed to restore backed up file (savegames, screenshots and possibly more).\n"
				"If you proceed the install may be broken and the backed up files may be lost during next uninstall.\n"
				"Continue anyway?\n\nError: %s"),
				fl_yes, fl_no, NULL, pErrMsg ? pErrMsg : $("unknown error")) )
			{
				DelTree( tmpdir.c_str() );
				return FALSE;
			}

			// delete saves and screenshots that were partially restored (for differential backups this could end badly)
			EnumFullArchive(bakarchive.c_str(), RemoveEnumeratedArchiveFile, installdir);
		}
		else if (bInstallInfoSafe)
		{
			// check if backup data contained an install info file with list of files to delete
			LoadRemoveFileInfo(fm);
		}
	}

	if ( !SaveInstallInfo(fm, tmpdir.c_str()) )
	{
		ASSERT(FALSE);
		// TODO: should we handle if this fails? a bit lame to fail an install because of that, there is after all
		//       redundancy for the contained information in the main db
	}

	// install successful, the final step now is to move the temp dir to the actual location to "go live"
	if ( rename_instdir_safe(tmpdir.c_str(), installdir) )
		fm->flags |= FMEntry::FLAG_Installed;
	else
	{
		DelTree( tmpdir.c_str() );
		fl_alert($("Failed to move install dir to final location, install aborted."));
		return FALSE;
	}

	RedrawListControl(TRUE);

	return TRUE;
}


static BOOL EnumFileDiffInfo(const char *path, int relname_start)
{
	// if the path length is already at max, then fail, because we can't possibly append file or subdir names
	if (strlen(path) >= MAX_PATH)
		return FALSE;

	dirent **files;
	int nFiles = fl_filename_list(path, &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return TRUE;

	BOOL bRet = TRUE;

	string s;
	struct stat st = {0};
	FileDiffInfo info;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		int len = strlen(f->d_name);
		if ( isdirsep(f->d_name[len-1]) )
		{
			// recurse into subdir

			if (strcmp(f->d_name, "./") && strcmp(f->d_name, "../"))
			{
				f->d_name[len-1] = 0;

				s = path;
				s.append(DIRSEP);
				s.append(f->d_name);

				if ( !EnumFileDiffInfo(s.c_str(), relname_start) )
					bRet = FALSE;
			}
		}
		else
		{
			// get file info

			s = path;
			s.append(DIRSEP);
			s.append(f->d_name);

			if (s.length() > MAX_PATH || !GetFileSizeAndMTimeOS(s.c_str(), info.fsize, info.ftime))
			{
				ASSERT(FALSE);
				bRet = FALSE;
			}
			else
			{
				info.fname = _strdup( s.c_str() );
				info.fname_rel = info.fname + (relname_start + 1);

				g_fileDiffInfoMap[FKEY(info.fname_rel)] = info;
			}
		}
	}

	fl_filename_free_list(&files, nFiles);

	info.fname = NULL;

	return bRet;
}

static bool ClearIdenticalEnumeratedArchiveFile(const char *fname, unsigned __int64 fsize, time_t ftime, void *p)
{
	ASSERT(p != NULL);

	tFileDiffInfoHash::iterator dataIter = g_fileDiffInfoMap.find(FKEY(fname));
	if (dataIter != g_fileDiffInfoMap.end())
	{
		if ((fsize != ~(unsigned __int64)0 && fsize != dataIter->second.fsize)
			|| (ftime && ftime != dataIter->second.ftime))
			// file differs
			return true;

		// remove identical file from hash
		g_fileDiffInfoMap.erase(dataIter);
	}
	else
	{
		// if mp3 or ogg file then check if wav file exists with same name
		const int n = strlen(fname);
		if (n > 4 && (!_stricmp(fname+n-4, ".mp3") || !_stricmp(fname+n-4, ".ogg")))
		{
			char wavname[MAX_PATH_BUF];
			memcpy(wavname, fname, n-3);
			strcpy(wavname+n-3, "wav");

			dataIter = g_fileDiffInfoMap.find(FKEY(wavname));
			if (dataIter != g_fileDiffInfoMap.end())
			{
				// can't compare on filesize, only modtime
				if (ftime && ftime != dataIter->second.ftime)
					// file differs
					return true;

				// remove identical file from hash
				g_fileDiffInfoMap.erase(dataIter);

				return true;
			}
		}

		// file was deleted from install
		g_removedFiles.push_back(fname);
	}

	return true;
}

static bool ClearDiffInfoFileEntry(const char *fname)
{
	tFileDiffInfoHash::iterator dataIter = g_fileDiffInfoMap.find(FKEY(fname));
	if (dataIter != g_fileDiffInfoMap.end())
	{
		g_fileDiffInfoMap.erase(dataIter);
		return true;
	}

	return false;
}

static BOOL UninstallFM(FMEntry *fm)
{
	if ( g_sTempDir.empty() )
	{
		// shouldn't get here
		fl_alert($("No temp/cache directory available, cannot uninstall."));
		return FALSE;
	}

	if ( !IsSafeFmDir(fm) )
	{
		fl_alert($("FM directory name \"%s\" is invalid, cannot uninstall."), fm->name);
		return FALSE;
	}

	const int backup = fl_choice(
		g_cfg.bDiffBackups
			? $("Backup all modified/added/removed files (including savegames and screenshots)?")
			: $("Backup savegames and screenshots?"),
		fl_cancel, fl_yes, fl_no);
	if (!backup)
		return FALSE;

	const BOOL bBackupSaves = (backup != 2);
	const BOOL bDiffBackup = bBackupSaves && g_cfg.bDiffBackups;

	BOOL bRet;

	if (bDiffBackup)
	{
		// differential backup (backs up all added files and files that differ in mtime or size from FM archive)

		const char *msg = $("All files in the install dir will be deleted, any modified/added/removed files are backed up.\nProceed with uninstall?");

		if ( !fl_choice(msg, fl_cancel, fl_ok, NULL) )
			return FALSE;

		// do backup first, only if that succeeded we do a "deltree"

		char installdir[MAX_PATH_BUF];
		if (_snprintf_s(installdir, sizeof(installdir), _TRUNCATE, "%s" DIRSEP "%s", g_pFMSelData->sRootPath, fm->name) == -1)
		{
			fl_alert($("Path too long, uninstall aborted."));
			return FALSE;
		}

		ShowBusyCursor(TRUE);

		if (g_bRunningShock)
			// delete the 'current' dir from shock FMs (the current dir is just a temp folder that gets created
			// each time a savegame is loaded, we don't want that part of any backup)
			DelTree(string(installdir) + DIRSEP "current");

		// enumerate all files in the install dir with mtime and size into
		if ( !EnumFileDiffInfo(installdir, strlen(installdir)) )
		{
			fl_alert($("Failed to scan files to make backup, uninstall aborted."));
			bRet = FALSE;
		}
		else
		{
			// remove all files from g_fileDiffInfoMap that are identical to FM archive
			const char *pErrMsg = NULL;
			if ( !EnumFullArchiveEx(fm->GetArchiveFilePath().c_str(), ClearIdenticalEnumeratedArchiveFile, installdir, &pErrMsg) )
			{
				fl_alert($("Failed to determine changed files for backup, uninstall aborted\n\nArchive Error: %s"), pErrMsg ? pErrMsg : "unknown error");
				bRet = FALSE;
			}
			else
			{
				// remove install info from backup set
				ClearDiffInfoFileEntry("fmsel.inf");
				// remove thief checkpoint save
				if (!g_bRunningShock)
					ClearDiffInfoFileEntry("startmis.sav");

				// backup all files remaining in g_fileDiffInfoMap
				if ( !BackupDiffSet(fm) )
				{
					fl_alert($("Failed to backup changed files, uninstall aborted."));

					bRet = FALSE;
				}
				else
				{
					bRet = FmDelTree(fm);

					if (!bRet)
						fl_alert(bBackupSaves
							? $("Failed to delete install directory, uninstall aborted (backup archive was still created/updated).")
							: $("Failed to delete install directory, uninstall aborted."));
				}
			}
		}

		g_fileDiffInfoMap.clear();
		g_removedFiles.clear();

		ShowBusyCursor(FALSE);
	}
	else
	{
		const char *msg = bBackupSaves
			? $("All files in the install dir will be deleted, any modified/added files,\nexcept savegames and screenshots, will be lost.\nProceed with uninstall?")
			: $("All files in the install dir will be deleted, any modified/added files,\nsavegames and screenshots will be lost!\nProceed with uninstall?");

		if ( !fl_choice(msg, fl_cancel, fl_ok, NULL) )
			return FALSE;

		// do backup first, only if that succeeded we do a "deltree"
		if (bBackupSaves && !BackupSavesToArchive(fm))
		{
			fl_alert($("Failed to backup savegames and screenshots, uninstall aborted."));
			return FALSE;
		}

		bRet = FmDelTree(fm);

		if (!bRet)
			fl_alert(bBackupSaves
				? $("Failed to delete install directory, uninstall aborted (backup archive was still created/updated)")
				: $("Failed to delete install directory, uninstall aborted"));
	}

	RedrawListControl(TRUE);

	return bRet;
}


static BOOL DeleteFM(FMEntry *fm)
{
	if (!fm)
	{
		ASSERT(FALSE);
		return FALSE;
	}

	// select next (or previous if last) list entry before removing the old
	if (GetCurSelFM() == fm)
		SelectNeighborFM();

	InvalidateTagDb();

	g_bDbModified = TRUE;

	g_dbHash.erase( KEY(fm->name) );
	g_db.erase(g_db.begin() + GetDbIndex(fm));

	delete fm;

	RefreshFilteredDb();

	RemoveDeadTagFilters();

	return TRUE;
}


static bool ListEnumeratedArchiveFile(const char *fname, void *p)
{
	ASSERT(p != NULL);

	vector<string> &list = *(vector<string>*)p;
	list.push_back(fname);

	return true;
}

static __inline bool compare_relfname(const char *a, const char *b)
{
	// root files last
	if ( !strchr(a, *DIRSEP) )
	{
		if ( strchr(b, *DIRSEP) )
			return false;
	}
	else if ( !strchr(b, *DIRSEP) )
		return true;

	return _stricmp(a, b) < 0;
}

static void ListArchiveFiles(const char *archive, string &html)
{
	char buf[MAX_PATH_BUF];

	unsigned __int64 sz = 0;
	unsigned int filecount = 0;
	GetUnpackedArchiveSize(archive, sz, filecount);

	vector<string> list;
	list.reserve(filecount);

	if ( !EnumFullArchive(archive, ListEnumeratedArchiveFile, &list) )
		return;

	int i;

	// sort file list
	vector<const char*> sorted;
	sorted.reserve( list.size() );
	for (i=0; i<(int)list.size(); i++)
		sorted.push_back( list[i].c_str() );
	std::sort(sorted.begin(), sorted.end(), compare_relfname);

	for (i=0; i<(int)sorted.size(); i++)
	{
		html.append( TextToHtml(sorted[i]) );
		html.append("<br>");
	}

	if (sz)
	{
		char s[64];
		FormatFileSizeValue(s, sz);

		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "<br><b><font color=\"%s\">%s: </font>%s </b>(%d %s)", DARK() ? "#4A86AF" : "#87C5F0",
			$("Estimated install size"), s, filecount, $("files"));
		html.append(buf);
	}
}

static void ViewArchiveContents(FMEntry *fm)
{
	if ( !fm->IsArchived() )
		return;

	string html;
	char buf[MAX_PATH_BUF];

	ListArchiveFiles(fm->GetArchiveFilePath().c_str(), html);

	// list backup archive contents if available
	string bakarchive = fm->GetBakArchiveFilePath();
	struct stat st = {0};
	if ( !stat(bakarchive.c_str(), &st) )
	{
		html.append("<br><br><br><b><u>");
		html.append($("Backup archive"));
		html.append(":</u></b><br>");

		ListArchiveFiles(bakarchive.c_str(), html);
	}

	_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s \"%s\"", $("Contents of"), fm->archive.c_str());

	GenericHtmlTextPopup(buf, html.c_str());
}


struct LangEnumContext
{
	vector<const char*> langsToFind;
	vector<const char*> langsFound;
	const char *lpszEarlyOutOn;
};

static bool EnumLanguages(const char *fname, void *p)
{
	ASSERT(p != NULL);

	// must contain subdir
	if ( !strchr(fname, *DIRSEP) )
		return true;

	LangEnumContext &ctxt = *(LangEnumContext*)p;

	for (int i=0; i<(int)ctxt.langsToFind.size(); i++)
		if ( stristr(fname, ctxt.langsToFind[i]) )
		{
			const char *lpszLang = ctxt.langsToFind[i];

			// move language to found list
			ctxt.langsFound.push_back(lpszLang);
			ctxt.langsToFind.erase(ctxt.langsToFind.begin() + i);

			if (ctxt.lpszEarlyOutOn && !strcmp(lpszLang, ctxt.lpszEarlyOutOn))
				return false;

			i--;
		}

	return true;
}

static void ScanDirForLanguage(const char *subdirname, LangEnumContext &ctxt, int depth = 0)
{
	ASSERT(subdirname != NULL);

	// 'depth' is a dumb infinite recursion stopper, in case file system contains cyclic hard-links
	if (depth > 99)
		return;

	string sdir;

	dirent **files;
	int nFiles = fl_filename_list(subdirname, &files, NO_COMP_UTFCONV);
	if (nFiles <= 0)
		return;

	for (int i=0; i<nFiles; i++)
	{
		dirent *f = files[i];

		if (f->d_name[0] != '.')
		{
			int len = strlen(f->d_name);
			if ( isdirsep(f->d_name[len-1]) )
			{
				// recurse into subdir

				BOOL bRecurse = TRUE;

				// don't check for language dirs in the root
				if (depth)
				{
					int j;

					for (j=0; j<(int)ctxt.langsToFind.size(); j++)
						if ( !_stricmp(f->d_name, ctxt.langsToFind[j]) )
						{
							const char *lpszLang = ctxt.langsToFind[j];

							// move language to found list
							ctxt.langsFound.push_back(lpszLang);
							ctxt.langsToFind.erase(ctxt.langsToFind.begin() + j);

							if (ctxt.lpszEarlyOutOn && !strcmp(lpszLang, ctxt.lpszEarlyOutOn))
							{
								// we're done, free data and return
								fl_filename_free_list(&files, nFiles);
								return;
							}

							// don't recurse inside language dirs
							bRecurse = FALSE;

							break;
						}

					// don't recurse inside language dirs, check if the current dir is an already enumerated lang dir
					if (bRecurse)
					{
						for (j=0; j<(int)ctxt.langsFound.size(); j++)
							if ( !_stricmp(f->d_name, ctxt.langsFound[j]) )
							{
								bRecurse = FALSE;
								break;
							}
					}
				}

				if (bRecurse)
				{
					sdir = subdirname;
					sdir.append(f->d_name);
					ScanDirForLanguage(sdir.c_str(), ctxt, depth+1);
				}
			}
		}
	}

	fl_filename_free_list(&files, nFiles);
}

static BOOL GetLanguages(FMEntry *fm, std::list<string> &langlist, BOOL bEarlyOutOnEnglish = FALSE)
{
	if ( !fm->IsAvail() )
		return FALSE;

	char langdirs[NUM_LANGUAGES][16];
	LangEnumContext ctxt;
	ctxt.langsToFind.reserve(NUM_LANGUAGES);

	// prefer searching archive if available, it's faster and easier to enumerate files there
	if ( fm->IsArchived() )
	{
		for (int i=0; i<NUM_LANGUAGES; i++)
		{
			sprintf(langdirs[i], DIRSEP "%s" DIRSEP, g_languages[i]);
			ctxt.langsToFind.push_back(langdirs[i]);
		}

		ctxt.lpszEarlyOutOn = bEarlyOutOnEnglish ? langdirs[0] : NULL;

		EnumFullArchive(fm->GetArchiveFilePath().c_str(), EnumLanguages, &ctxt);
	}
	else
	{
		for (int i=0; i<NUM_LANGUAGES; i++)
		{
			sprintf(langdirs[i], "%s/", g_languages[i]);
			ctxt.langsToFind.push_back(langdirs[i]);
		}

		char installdir[MAX_PATH_BUF];
		if (_snprintf_s(installdir, sizeof(installdir), _TRUNCATE, "%s" DIRSEP "%s" DIRSEP, g_pFMSelData->sRootPath, fm->name) == -1)
			return FALSE;

		ctxt.lpszEarlyOutOn = bEarlyOutOnEnglish ? langdirs[0] : NULL;

		ScanDirForLanguage(installdir, ctxt);
	}

	if ( ctxt.langsFound.empty() )
		return FALSE;

	// handle if caller only wants english reported, if it's available
	if (bEarlyOutOnEnglish)
		for (int i=0; i<(int)ctxt.langsFound.size(); i++)
		{
			if ( !strcmp(ctxt.langsFound[i], langdirs[0]) )
			{
				langlist.push_back(g_languages[0]);
				return TRUE;
			}
		}

	// list all available languages (sorted in the same order as g_languages)
	for (int i=0; i<(int)ctxt.langsFound.size(); i++)
		for (int j=0; j<NUM_LANGUAGES; j++)
			if ( !strcmp(ctxt.langsFound[i], langdirs[j]) )
			{
				langlist.push_back(g_languages[j]);
				break;
			}

	return TRUE;
}

static BOOL GetFallbackLanguage(FMEntry *fm, string &lang)
{
	// determine if FM has languages defined, if it does and english is among them, then english is the fallback
	// if english is not among them then pick another, if no languages are found then no fallback language will
	// be defined

	std::list<string> languages;

	if ( !GetLanguages(fm, languages, TRUE) )
		return FALSE;

	// return first available language (which will be english if english is available)
	lang = languages.front();

	return TRUE;
}

static BOOL IsLanguageInList(std::list<string> &langlist, const char *lang)
{
	for (std::list<string>::iterator it=langlist.begin(); it!=langlist.end(); it++)
		if ( !_stricmp(it->c_str(), lang) )
			return TRUE;

	return FALSE;
}


/////////////////////////////////////////////////////////////////////
// Custom Widgets

//
// FM_Return_Button (hides return icon when disabled)
//

#if defined(CUSTOM_FLTK) || defined(STATIC)
extern int fl_return_arrow(int x, int y, int w, int h);
#else
int fl_return_arrow(int x, int y, int w, int h) {
	int size = w; if (h<size) size = h;
	int d = (size+2)/4; if (d<3) d = 3;
	int t = (size+9)/12; if (t<1) t = 1;
	int x0 = x+(w-2*d-2*t-1)/2;
	int x1 = x0+d;
	int y0 = y+h/2;
	fl_color(FL_LIGHT3);
	fl_line(x0, y0, x1, y0+d);
	fl_yxline(x1, y0+d, y0+t, x1+d+2*t, y0-d);
	fl_yxline(x1, y0-t, y0-d);
	fl_color(fl_gray_ramp(0));
	fl_line(x0, y0, x1, y0-d);
	fl_color(FL_DARK3);
	fl_xyline(x1+1, y0-t, x1+d, y0-d, x1+d+2*t);
	return 1;
}
#endif

class Fl_FM_Return_Button : public Fl_Return_Button
{
protected:
	virtual void draw()
	{
		if (type() == FL_HIDDEN_BUTTON) return;
		draw_box(value() ? (down_box()?down_box():fl_down(box())) : box(),
			value() ? selection_color() : color());
		int W = h();
		if (w()/3 < W) W = w()/3;
		if ( active_r() ) fl_return_arrow(x()+w()-W-4, y(), W, h());

		int XX = x()+Fl::box_dx(box());
		int WW = w()-Fl::box_dw(box())-((W+4)/2);
		if (WW > 11 && align()&(FL_ALIGN_LEFT|FL_ALIGN_RIGHT)) {XX += 3; WW -= 6;}
		draw_label(XX, y()+Fl::box_dy(box()), WW, h()-Fl::box_dh(box()));

		if (Fl::focus() == this) draw_focus();
	}
public:
	Fl_FM_Return_Button(int X, int Y, int W, int H, const char *l=0)
		: Fl_Return_Button(X,Y,W,H,l) {}
};


//
// FM_Menu_Button
//

class Fl_FM_Menu_Button : public Fl_Menu_Button
{
protected:
	virtual int handle(int e)
	{
		if (e == FL_PUSH)
		{
			if (!box()) {
				if (Fl::event_button() != 3) return 0;
			} else if (type()) {
				if (!(type() & (1 << (Fl::event_button()-1)))) return 0;
			}
			if (Fl::visible_focus()) Fl::focus(this);
			DoPopup();
			return 1;
		}

		return Fl_Menu_Button::handle(e);
	}

public:
	Fl_FM_Menu_Button(int X, int Y, int W, int H, const char *l=0)
		: Fl_Menu_Button(X,Y,W,H,l) {}

	virtual void DoPopup() = 0;
};


//
// FM_Filter_Button
//

class Fl_FM_Filter_Button : public Fl_Button
{
protected:
	virtual void draw()
	{
		Fl_Image *img = image();
		if ( !value() )
			image( deimage() );

		Fl_Button::draw();

		image(img);
	}

public:
	Fl_FM_Filter_Button(int X, int Y, int W, int H, const char *l=0)
		: Fl_Button(X,Y,W,H,l) {}
};


//
// FM_Config_Button
//

class Fl_FM_Config_Button : public Fl_FM_Menu_Button
{
public:
	Fl_FM_Config_Button(int X, int Y, int W, int H, const char *l=0)
		: Fl_FM_Menu_Button(X,Y,W,H,l) {}

	virtual void DoPopup()
	{
		FMEntry *fm = GetCurSelFM();

		enum
		{
			CMD_NONE,

			CMD_DateFmt0,
			CMD_DateFmtLast = CMD_DateFmt0+DATEFMT_NUM_FORMATS-1,

			CMD_TagRows0,
			CMD_TagRowsLast = CMD_TagRows0+4,
			CMD_ToggleVarSizeRows,

			CMD_ThemeGtk,
			CMD_ThemeGtkDark,
			CMD_ThemePlastic,
			CMD_ThemeBasic,

			CMD_FontSizeNormal,
			CMD_FontSizeLarge,

			CMD_Scale1,
			CMD_ScaleLast = CMD_Scale1+4,

			CMD_ToggleWrapDescrEdit,
			CMD_ToggleWrapNotesEdit,

			CMD_ToggleAutoRefresh,

			CMD_DebriefNever,
			CMD_DebriefAfterFM,
			CMD_DebriefAlways,

			CMD_BackupSaves,
			CMD_BackupDiff,

			CMD_ConvertOgg,
			CMD_GenerateMissFlags,

			CMD_ArchivePath,

			CMD_SaveDb,

			CMD_AutoScanDates,
			CMD_ExportIni,
			CMD_CreateIni,
			CMD_ExportBatchIni,
			CMD_ImportBatchIni,

			CMD_About,
		};

		const BOOL bAdvanced = !!(Fl::event_state() & FL_CTRL);

		#define MENU_DATEFMT(_n) MENU_RITEM(dateformats[_n], CMD_DateFmt0+(_n), g_cfg.datefmt == (_n))

		int i;
		char dateformats[DATEFMT_NUM_FORMATS][64];
		time_t t;
		time(&t);
		for (i=0; i<DATEFMT_NUM_FORMATS; i++)
			FormatDbDate(t, dateformats[i], sizeof(dateformats[i]), TRUE, i);
		strcat_s(dateformats[0], sizeof(dateformats[0]), " ");
		strcat_s(dateformats[0], sizeof(dateformats[0]), $("(cur locale)"));
		dateformats[0][sizeof(dateformats[0])-1] = 0;

		const int MAX_MENU_ITEMS = 64+8;
		int menu_items = 0;

		Fl_Menu_Item menu[MAX_MENU_ITEMS];

		MENU_SUB($("Tag Rows"));
			MENU_RITEM($("Hide Tags"), CMD_TagRows0, g_cfg.tagrows == 0);
			MENU_RITEM("1", CMD_TagRows0+1, g_cfg.tagrows == 1);
			MENU_RITEM("2", CMD_TagRows0+2, g_cfg.tagrows == 2);
			MENU_RITEM("3", CMD_TagRows0+3, g_cfg.tagrows == 3);
			MENU_RITEM("4", CMD_TagRows0+4, g_cfg.tagrows == 4);
			MENU_MOD_DIV();
			MENU_TITEM($("Variable Height"), CMD_ToggleVarSizeRows, g_cfg.bVarSizeList);
			MENU_MOD_DISABLE(g_cfg.tagrows == 0);
			MENU_END();
		MENU_SUB($("Date Format"));
			for (i=0; i<DATEFMT_NUM_FORMATS; i++)
				MENU_DATEFMT(i);
			MENU_END();
		MENU_SUB($("UI Theme"));
			MENU_RITEM($("Default"), CMD_ThemeGtk, g_cfg.uitheme == 0);
			MENU_RITEM($("Dark"), CMD_ThemeGtkDark, g_cfg.uitheme == 1);
			MENU_RITEM($("Plastic"), CMD_ThemePlastic, g_cfg.uitheme == 2);
			MENU_RITEM($("Basic"), CMD_ThemeBasic, g_cfg.uitheme == 3);
			MENU_END();
		MENU_SUB($("UI Font Size"));
			MENU_RITEM($("Normal"), CMD_FontSizeNormal, g_cfg.bLargeFont == 0);
			MENU_RITEM($("Large"), CMD_FontSizeLarge, g_cfg.bLargeFont != 0);
			MENU_END();
		MENU_SUB($("UI Scale"));
			MENU_RITEM($("1x"), CMD_Scale1, g_cfg.uiscale == 4);
			MENU_RITEM($("1.25x"), CMD_Scale1+1, g_cfg.uiscale == 5);
			MENU_RITEM($("1.5x"), CMD_Scale1+2, g_cfg.uiscale == 6);
			MENU_RITEM($("1.75x"), CMD_Scale1+3, g_cfg.uiscale == 7);
			MENU_RITEM($("2x"), CMD_Scale1+4, g_cfg.uiscale == 8);
			MENU_END();
		MENU_SUB($("Misc")); MENU_MOD_DIV();
			MENU_TITEM($("Word Wrap Descr Editor"), CMD_ToggleWrapDescrEdit, g_cfg.bWrapDescrEditor);
			MENU_TITEM($("Word Wrap Notes Editor"), CMD_ToggleWrapNotesEdit, g_cfg.bWrapNotesEditor);
			MENU_END();
		if (bAdvanced)
		{
			MENU_TITEM($("No Auto-refresh"), CMD_ToggleAutoRefresh, !g_cfg.bAutoRefreshFilteredDb);
			MENU_MOD_DIV();
		}
		MENU_SUB($("Return to FMSel")); MENU_MOD_DIV();
			MENU_RITEM($("Never"), CMD_DebriefNever, g_cfg.returnAfterGame[g_bRunningEditor] == RET_NEVER);
			MENU_RITEM($("After FM"), CMD_DebriefAfterFM, g_cfg.returnAfterGame[g_bRunningEditor] == RET_FM);
			MENU_RITEM($("Always"), CMD_DebriefAlways, g_cfg.returnAfterGame[g_bRunningEditor] == RET_ALWAYS);
			MENU_END();
		MENU_ITEM($("FM Archive Path..."), CMD_ArchivePath);
		MENU_SUB($("Backup Type")); MENU_MOD_DISABLE(!g_cfg.bRepoOK);
			MENU_RITEM($("Only Saves and Shots"), CMD_BackupSaves, !g_cfg.bDiffBackups);
			MENU_RITEM($("All Changed Files"), CMD_BackupDiff, g_cfg.bDiffBackups);
			MENU_END();
		MENU_SUB($("Install Options")); MENU_MOD_DISABLE(!g_cfg.bRepoOK); MENU_MOD_DIV();
#ifdef OGG_SUPPORT
			MENU_TITEM($("Convert OGG to WAV"), CMD_ConvertOgg, g_cfg.bDecompressOGG);
#endif
			MENU_TITEM($("Generate Mission Flags"), CMD_GenerateMissFlags, g_cfg.bGenerateMissFlags);
			MENU_END();
		MENU_SUB($("Tasks")); MENU_MOD_DIV();
			MENU_ITEM($("Auto-fill Release Dates"), CMD_AutoScanDates);
			MENU_MOD_DIV();
			MENU_ITEM($("Export FM.INI from FM..."), CMD_ExportIni);
			MENU_MOD_DISABLE(!fm);
			MENU_ITEM($("Create New FM.INI..."), CMD_CreateIni);
			MENU_MOD_DIV();
			MENU_ITEM($("Export Batched FM.INI..."), CMD_ExportBatchIni);
			MENU_ITEM($("Import Batched FM.INI..."), CMD_ImportBatchIni);
			MENU_END();
		if (bAdvanced)
		{
			MENU_ITEM($("Save Db Now"), CMD_SaveDb);
		}
		MENU_MOD_DIV();
		MENU_ITEM($("About"), CMD_About);
		MENU_END();

		const Fl_Menu_Item *m = menu->pulldown(x(), y(), w(), h(), 0, this);
		if (!m || !m->user_data())
			return;

		const int cmd_id = (intptr_t)m->user_data();
		switch (cmd_id)
		{
		case CMD_ToggleVarSizeRows:
			g_cfg.bVarSizeList = !g_cfg.bVarSizeList;
			g_cfg.OnModified();
			RefreshListControlRowSize();
			break;

		case CMD_ThemeGtk:
		case CMD_ThemeGtkDark:
		case CMD_ThemePlastic:
		case CMD_ThemeBasic:
			if ((g_cfg.uitheme == 1 && cmd_id-CMD_ThemeGtk != 1) || (g_cfg.uitheme != 1 && cmd_id-CMD_ThemeGtk == 1))
				ChangeScheme();
			g_cfg.uitheme = cmd_id-CMD_ThemeGtk;
			g_cfg.OnModified();
			if (g_cfg.uitheme == 2)
				Fl::scheme("plastic");
			else if (g_cfg.uitheme <= 1)
				Fl::scheme("gtk+");
			else
				Fl::scheme("base");
			pMainWnd->redraw();
			Fl::scrollbar_size(SCALECFG(Fl::scrollbar_size()));
			break;

		case CMD_FontSizeNormal:
		case CMD_FontSizeLarge:
			if ( fl_choice($("Restart is required to change font size. Change size and exit?"), fl_cancel, fl_ok, NULL) )
			{
				g_cfg.bLargeFont = (cmd_id == CMD_FontSizeLarge);
				g_cfg.OnModified();
				OnExit(NULL, NULL);
			}
			break;

		case CMD_ToggleWrapDescrEdit:
			g_cfg.bWrapDescrEditor = !g_cfg.bWrapDescrEditor;
			g_cfg.OnModified();
			break;
		case CMD_ToggleWrapNotesEdit:
			g_cfg.bWrapNotesEditor = !g_cfg.bWrapNotesEditor;
			g_cfg.OnModified();
			break;

		case CMD_ToggleAutoRefresh:
			g_cfg.bAutoRefreshFilteredDb = !g_cfg.bAutoRefreshFilteredDb;
			g_cfg.OnModified();
			break;

		case CMD_DebriefNever:
			g_cfg.returnAfterGame[g_bRunningEditor] = RET_NEVER;
			g_cfg.OnModified();
			break;
		case CMD_DebriefAfterFM:
			g_cfg.returnAfterGame[g_bRunningEditor] = RET_FM;
			g_cfg.OnModified();
			break;
		case CMD_DebriefAlways:
			g_cfg.returnAfterGame[g_bRunningEditor] = RET_ALWAYS;
			g_cfg.OnModified();
			break;

		case CMD_BackupSaves:
			g_cfg.bDiffBackups = FALSE;
			g_cfg.OnModified();
			break;
		case CMD_BackupDiff:
			g_cfg.bDiffBackups = TRUE;
			g_cfg.OnModified();
			break;

		case CMD_ConvertOgg:
			g_cfg.bDecompressOGG = !g_cfg.bDecompressOGG;
			g_cfg.OnModified();
			break;
		case CMD_GenerateMissFlags:
			g_cfg.bGenerateMissFlags = !g_cfg.bGenerateMissFlags;
			g_cfg.OnModified();
			break;

		case CMD_ArchivePath:
			ConfigArchivePath();
			break;

		case CMD_SaveDb:
			if ( !SaveDb() )
				fl_alert($("Failed to save \"fmsel.ini\"."));
			break;

		case CMD_AutoScanDates:
			AutoScanReleaseDates();
			break;
		case CMD_ExportIni:
			ExportFmIni(fm);
			break;
		case CMD_CreateIni:
			CreateStandaloneFmIni();
			break;
		case CMD_ExportBatchIni:
			ExportBatchFmIni();
			break;
		case CMD_ImportBatchIni:
			ImportBatchFmIni();
			break;

		case CMD_About:
			ViewAbout();
			break;

		default:
			if (cmd_id >= CMD_DateFmt0 && cmd_id <= CMD_DateFmtLast)
			{
				g_cfg.SetDatFmt(cmd_id-CMD_DateFmt0);
				((Fl_Widget*)pFMList)->redraw();
			}
			else if (cmd_id >= CMD_TagRows0 && cmd_id <= CMD_TagRowsLast)
			{
				g_cfg.tagrows = cmd_id-CMD_TagRows0;
				g_cfg.OnModified();
				RefreshListControlRowSize();
			}
			else if (cmd_id >= CMD_Scale1 && cmd_id <= CMD_ScaleLast)
			{
				if ( fl_choice($("Restart is required to change UI scale. Change size and exit?"), fl_cancel, fl_ok, NULL) )
				{
					g_cfg.uiscale = cmd_id-CMD_Scale1+4;
					g_cfg.OnModified();
					OnExit(NULL, NULL);
				}
			}
		}
	}
};


//
// FM_Filter_Input (pressing tab always changes to next control instead of moving cursor to end of text if all selected)
//

class Fl_FM_Filter_Input : public Fl_Input
{
protected:
	BOOL m_bEnterOrFocus;

protected:
	virtual int handle(int e)
	{
		m_bEnterOrFocus = FALSE;

		if (e == FL_UNFOCUS)
			m_bEnterOrFocus = TRUE;
		else if (e == FL_KEYBOARD)
		{
			const int key = Fl::event_key();
			const int _XK_ISO_Left_Tab = 0xfe20;

			if (key == FL_Tab || key == _XK_ISO_Left_Tab)
				return parent()->Fl_Group::handle(e);

			if (key == FL_Enter || key == FL_KP_Enter)
				m_bEnterOrFocus = TRUE;
		}

		return Fl_Input::handle(e);
	}

public:
	Fl_FM_Filter_Input(int X, int Y, int W, int H, const char *l=0)
		: Fl_Input(X,Y,W,H,l)
	{
		m_bEnterOrFocus = FALSE;
	}

	// returns TRUE if callback was called during text edit and not due to losing focus or pressing enter
	// (useful for WHEN_CHANGED to know why the callback was called)
	BOOL callback_reason_edit() const { return !m_bEnterOrFocus; }
};


//
// FM_Tag_Input (custom tag input control)
//

class Fl_FM_Tag_Input : public Fl_Input
{
	enum
	{
		MAX_SUGGESTION_ROWS = 16,
	};

protected:
	Fl_Callback *m_tagcb;

protected:
	//
	// ListWidget
	//

	class ListWidget : public Fl_Browser_
	{
	public:
		enum
		{
			ROW_HEIGHT = 14,
		};

	protected:
		Fl_Font m_prefixfont;
		string m_prefix;
		int m_prefixW;

		vector<string> m_suggestions;

	protected:
		friend class ListWnd;

		virtual int item_width(void *p) const { return w() - Fl::scrollbar_size(); }
		virtual int item_height(void *p) const { return ROW_HEIGHT; }

		virtual void *item_first() const
		{
			return m_suggestions.empty() ? 0 : (void*)1;
		}

		virtual void *item_next(void *p) const
		{
			return ((intptr_t)p < (int)m_suggestions.size()) ? (void*)((intptr_t)p + 1) : 0;
		}

		virtual void *item_prev(void *p) const
		{
			return (intptr_t)p ? (void*)((intptr_t)p - 1) : 0;
		}

		virtual void item_draw(void *p, int X, int Y, int W, int H) const
		{
			const char *str = m_suggestions[(intptr_t)p - 1].c_str();
			const int prefixlen = m_prefix.length();
			int prefixW = m_prefixW;

			X++;
			Y++;
			W -= 2;
			H -= 2;

			fl_color( labelcolor() );

			// prefix (ie. the matching substring)
			fl_font(m_prefixfont, labelsize());
#if 0
			fl_draw(m_prefix.c_str(), X, Y, W, H, (Fl_Align)(FL_ALIGN_LEFT|FL_ALIGN_CLIP), 0, 0);
#else
			// draw the prefix using the suggestion string so that case is preserved
			char ch = str[prefixlen];
			(char&)str[prefixlen] = 0;
			prefixW = (int)fl_width(str);
			fl_draw(str, X, Y, W, H, (Fl_Align)(FL_ALIGN_LEFT|FL_ALIGN_CLIP), 0, 0);
			(char&)str[prefixlen] = ch;
#endif

			// the remainder of the string
			str += prefixlen;
			if (*str)
			{
				fl_font(labelfont(), labelsize());
				fl_draw(str, X+prefixW, Y, W-prefixW, H, (Fl_Align)(FL_ALIGN_LEFT|FL_ALIGN_CLIP), 0, 0);
			}
		}

		static void ExtractCat(const char *s, string &out)
		{
			char *q = (char*) strchr(s, ':') + 1;
			char ch = *q;
			*q = 0;
			out = s;
			*q = ch;
		}

		void EnumSuggestion(const char *suggestion, const char *prefix)
		{
			// don't add suggestions that match entered value 100%
			if ( strcmp(suggestion, prefix) )
				m_suggestions.push_back(suggestion);
		}

	public:
		ListWidget(int W, int H)
			: Fl_Browser_(0, 0, W, H)
		{
#if 0
			// bold prefix
			labelfont(FL_HELVETICA);
			m_prefixfont = FL_HELVETICA_BOLD;
#else
			// bold suffix (ala google)
			labelfont(FL_HELVETICA_BOLD);
			m_prefixfont = FL_HELVETICA;
#endif

			labelsize(12);
			labelcolor(FL_FOREGROUND_COLOR);
			color(FL_BACKGROUND2_COLOR);
			selection_color(FL_GRAY0+18);
			box(COMPLETION_LIST_BOX);
			has_scrollbar(VERTICAL);
			type(FL_SELECT_BROWSER);
			when(FL_WHEN_RELEASE | FL_WHEN_NOT_CHANGED);

			m_suggestions.reserve(64);
		}

		int PopulateSuggestions(const char *s)
		{
			int i;

			m_suggestions.clear();
			m_prefix = s;

			const BOOL bHasCat = (strchr(s, ':') != NULL);

			// generate suggestions
			if (!bHasCat && !g_tagNameList.empty())
			{
				// also suggest stand-alone category names
				string lastCat;
				for (i=0; i<(int)g_tagNameList.size(); i++)
				{
					const char *tag = g_tagNameList[i];
					if ( !strncasecmp_utf_b(tag, s, m_prefix.length()) )
					{
						if (lastCat.empty() || strncasecmp_utf_b(g_tagNameList[i], lastCat.c_str(), lastCat.length()))
						{
							// we've come across a new category
							ExtractCat(tag, lastCat);
							EnumSuggestion(lastCat.c_str(), s);
						}
						EnumSuggestion(tag, s);
					}
				}
			}
			else
			{
				for (i=0; i<(int)g_tagNameList.size(); i++)
				{
					const char *tag = g_tagNameList[i];
					if ( !strncasecmp_utf_b(tag, s, m_prefix.length()) )
						EnumSuggestion(tag, s);
				}
			}
			for (i=0; i<(int)g_tagNameListNoCat.size(); i++)
			{
				const char *tag = g_tagNameListNoCat[i];
				if ( !strncasecmp_utf_b(tag, s, m_prefix.length()) )
					EnumSuggestion(tag, s);
			}

			new_list();

			if ( !m_suggestions.empty() )
			{
				fl_font(m_prefixfont, labelsize());
				m_prefixW = (int)fl_width(s);
			}

			return (int)m_suggestions.size();
		}

		const char* value() const
		{
			return selection() ? m_suggestions[(intptr_t)selection() - 1].c_str() : NULL;
		}

		const char* first_value() const
		{
			return m_suggestions.empty() ? NULL : m_suggestions[0].c_str();
		}

		void move_selection(int rows)
		{
			if (!rows)
				return;

			if ( !selection() )
			{
				// nothing selected, select first
				select_first();
				return;
			}

			if (rows == 1)
			{
				void *p = item_next( selection() );
				if (p)
					select_only(p);
				return;
			}
			else if (rows == -1)
			{
				void *p = item_prev( selection() );
				if (p)
					select_only(p);
				return;
			}

			if (rows > 0)
			{
				void *p = item_next( selection() );
				if (!p)
					return;

				for (int i=0; i<rows; i++)
				{
					void *q = item_next(p);
					if (!q)
						break;
					p = q;
				}

				select_only(p);
			}
			else
			{
				void *p = item_prev( selection() );
				if (!p)
					return;

				rows = -rows;

				for (int i=0; i<rows; i++)
				{
					void *q = item_prev(p);
					if (!q)
						break;
					p = q;
				}

				select_only(p);
			}
		}

		void select_first()
		{
			if ( !m_suggestions.empty() )
				select_only((void*)1);
		}

		void select_last()
		{
			select_only( (void*)m_suggestions.size() );
		}

		int count() const
		{
			return m_suggestions.size();
		}
	};

	//
	// ListWnd
	//

	class ListWnd : public Fl_Double_Window
	{
	protected:
		ListWidget *m_pList;

		Fl_FM_Tag_Input *m_pOuter;

		int m_listrows;

	protected:
		virtual int handle(int e)
		{
			switch (e)
			{
			case FL_HIDE:
				((Fl_Widget*)pTagEdGroup)->activate();
				break;
			case FL_SHOW:
				((Fl_Widget*)pTagEdGroup)->deactivate();
				break;

			case FL_KEYBOARD:
				switch ( Fl::event_key() )
				{
				case FL_Escape:
					hide();
					return 1;
				}
				break;
			}

			return Fl_Double_Window::handle(e);
		}

		static void selchange_cb(ListWidget *o, void *p)
		{
			((ListWnd*)p)->handle_list_key(FL_Enter);
		}

	public:
		ListWnd(Fl_FM_Tag_Input *p)
			: Fl_Double_Window(p->x(), p->y()+p->h(), p->w(), ListWidget::ROW_HEIGHT)
		{
			m_pOuter = p;
			m_listrows = 0;

			box(FL_NO_BOX);
			clear_border();
			clear_visible();

			m_pList = new ListWidget(w(), h());

			m_pList->callback((Fl_Callback*)selchange_cb, this);

			end();

			resizable(m_pList);

			p->window()->add(this);
		}

		void Suggest(const char *s)
		{
			int rows = m_pList->PopulateSuggestions(s);
			if (rows)
			{
				const int max_fit_rows = ((parent()->h() - y() - 8) - Fl::box_dh( m_pList->box() )) / ListWidget::ROW_HEIGHT;
				const int max_rows = std::min((int)MAX_SUGGESTION_ROWS, max_fit_rows);
				if (rows > max_rows)
					rows = max_rows;

				m_listrows = rows;

				resize(x(), y(), w(), ListWidget::ROW_HEIGHT * rows + Fl::box_dh( m_pList->box() ));

				show();
			}
			else
				hide();
		}

		int handle_list_key(int key)
		{
			const int _XK_ISO_Left_Tab = 0xfe20;

			const char *s;

			switch (key)
			{
			case FL_Tab:
				// tab complete (or if nothing is selected in list then select first row)
				s = m_pList->value();
				if (s)
				{
					m_pOuter->value( m_pList->value() );
					return 1;
				}
				else if (m_pList->count() == 1)
				{
					m_pOuter->value( m_pList->first_value() );
					hide();
					return 1;
				}
			case _XK_ISO_Left_Tab:
				// if nothing is selected in list then select first row
				if ( !m_pList->value() )
					m_pList->select_first();
				return 1;

			case FL_Enter:
			case FL_KP_Enter:
				s = m_pList->value();
				if (s)
				{
					m_pOuter->value( m_pList->value() );
					hide();
					return 1;
				}
				else if (m_pList->count() == 1)
				{
					m_pOuter->value( m_pList->first_value() );
					hide();
					return 1;
				}
				break;

			case FL_Home:
				m_pList->select_first();
				return 1;
			case FL_End:
				m_pList->select_last();
				return 1;

			case FL_Up:
				m_pList->move_selection(-1);
				return 1;
			case FL_Down:
				m_pList->move_selection(1);
				return 1;
			case FL_Page_Up:
				m_pList->move_selection(-(m_listrows-1));
				return 1;
			case FL_Page_Down:
				m_pList->move_selection(m_listrows-1);
				return 1;
			}

			return 0;
		}
	};

	//

protected:
	ListWnd *m_pList;

protected:
	virtual int handle(int e)
	{
		switch (e)
		{
		case FL_KEYBOARD:
			{
			const int _XK_ISO_Left_Tab = 0xfe20;

			if (m_pList && m_pList->visible())
				if ( m_pList->handle_list_key( Fl::event_key() ) )
					return 1;

			switch ( Fl::event_key() )
			{
			case FL_Tab:
			case _XK_ISO_Left_Tab:
				return parent()->Fl_Group::handle(e);

			case FL_Left:
			case FL_Right:
				if (m_pList && m_pList->visible())
				{
					// allow cursor navigation but not switching widget
					Fl_Input::handle(e);
					return 1;
				}
				break;

			case FL_Enter:
			case FL_KP_Enter:
				HideSuggestions();
				if (m_tagcb)
					m_tagcb(this, NULL);
				return 1;

			case FL_Escape:
				if (m_pList && m_pList->visible())
				{
					HideSuggestions();
					return 1;
				}
				break;
			}
			}
			break;
		}

		return Fl_Input::handle(e);
	}

	static void change_cb(Fl_Widget *o, void *p) { ((Fl_FM_Tag_Input*)o)->OnChange(); }

	void HideSuggestions()
	{
		m_pList->hide();
	}

	void ShowSuggestions(const char *s)
	{
		m_pList->Suggest(s);
	}

	static void draw_frame(int x, int y, int w, int h, Fl_Color c)
	{
		fl_color(c);
		fl_rectf(x, y, w, h);
		fl_color(FL_FOREGROUND_COLOR);
		fl_rect(x, y, w, h);
	}

public:
	Fl_FM_Tag_Input(int X, int Y, int W, int H, const char *l=0)
		: Fl_Input(X,Y,W,H,l)
	{
		m_tagcb = NULL;
		m_pList = new ListWnd(this);

		when(FL_WHEN_CHANGED);
		callback(change_cb);
	}

	void tag_callback(Fl_Callback *cb)
	{
		m_tagcb = cb;
	}

	void OnChange()
	{
		const char *s = value();

		while (s && isspace_(*s)) s++;

		if (s && *s)
		{
			pTagEdAddTag->activate();
			ShowSuggestions(s);
		}
		else
		{
			pTagEdAddTag->deactivate();
			HideSuggestions();
		}
	}

	// workaround to check for event that should result in list being hidden
	void check_listhide_event(int e)
	{
		if (!m_pList || !m_pList->visible())
			return;

		switch (e)
		{
		case FL_PUSH:
			// clicking anywhere outside the input or list widget closes list
			if ( !Fl::event_inside(x(), y(), x()+w(), y()+h()+m_pList->h()) )
				HideSuggestions();
			break;

		case FL_FOCUS:
			{
			Fl_Widget *o = Fl::focus();
			if (o != this && o != m_pList && m_pList->find(o) != m_pList->children())
				HideSuggestions();
			}
			break;
		}
	}

	static void static_init()
	{
		Fl::set_boxtype(COMPLETION_LIST_BOX, draw_frame, 1, 1, 2, 2);
	}
};


//
// FM_List
//

class Fl_FM_List : public Fl_Table_Row
{
	struct ColDef
	{
		const char *name;
		int width;
		int minwidth;
		int width2;		// alt size when using large font
		BOOL visible;
		int sortid;

		int ui_col_index;
	};

	static ColDef s_columns[COL_NUM_COLS];
	static int s_column_idx[COL_NUM_COLS]; // map ui col index to s_columns index

	int m_font;
	int m_fontsize;
	int m_tagfontsize;
	Fl_Align m_tagalign;

	int m_noTagH;

	int m_topH;
	int m_tagY;
	int m_tagRowH;

	int m_nPendingRclickRow;

private:
	static void event_callback(Fl_Widget*, void *data)
	{
		Fl_FM_List *o = (Fl_FM_List*)data;
		o->OnCallback();
	}
	void OnCallback();

protected:
	virtual void draw_cell(TableContext context, int R=0, int C=0, int X=0, int Y=0, int W=0, int H=0);

	virtual int handle(int e)
	{
		switch (e)
		{
		case FL_PUSH:
			if (Fl::event_button() == FL_RIGHT_MOUSE)
			{
				int R, C;
				ResizeFlag resizeflag;
				TableContext context = cursor2rowcol(R, C, resizeflag);
				m_nPendingRclickRow = -1;
				if (context == CONTEXT_CELL)
				{
					m_nPendingRclickRow = R;
					select_row_ex(R);
				}

				return 1;
			}
			else if (Fl::event_button() == FL_LEFT_MOUSE && Fl::event_clicks() > 0)
			{
				int R, C;
				ResizeFlag resizeflag;
				TableContext context = cursor2rowcol(R, C, resizeflag);
				m_nPendingRclickRow = -1;
				if (context == CONTEXT_CELL)
				{
					select_row_ex(R);
					OnDblClick();
				}
				return 1;
			}
			break;

		case FL_RELEASE:
			if (Fl::event_button() == FL_RIGHT_MOUSE)
			{
				if (m_nPendingRclickRow >= 0)
				{
					DoContextMenu();
					m_nPendingRclickRow = -1;
				}
				return 1;
			}
			break;

		case FL_KEYBOARD:
			switch( Fl::event_key() )
			{
			case FL_Enter:
			case FL_KP_Enter: OnDblClick(); break;
			case FL_Home: move_selection(-1000000, 0); break;
			case FL_End: move_selection(1000000, 0); break;
			case FL_Page_Up: move_selection(-(botrow - toprow - 1), 0); break;
			case FL_Page_Down: move_selection(botrow - toprow - 1 , 0); break;
			case FL_Up: move_selection(-1, 0); break;
			case FL_Down: move_selection(1, 0); break;
			case FL_Left: scroll_horz(-50); break;
			case FL_Right: scroll_horz(50); break;
			default:
				return Fl_Table_Row::handle(e);
			}
			return 1;

		case FL_FOCUS:
		case FL_UNFOCUS:
			redraw();
			break;
		}

		return Fl_Table_Row::handle(e);
	}

	void select_row_ex(int R)
	{
		if ( !row_selected(R) )
		{
			select_row(R);
			this->Fl_Table::select_row = current_row = R;
			do_callback(CONTEXT_CELL, R, 0);
		}
	}

	void move_selection(int R, int C)
	{
		if ( move_cursor(R, C, true) )
			select_row_ex(current_row);
	}

	void scroll_horz(int dx)
	{
		int newleft = hscrollbar->value() + dx;
		if (newleft < 0)
			newleft = 0;
		else if (newleft > hscrollbar->maximum())
			newleft = (int)hscrollbar->maximum();
		hscrollbar->Fl_Slider::value(newleft);
		table_scrolled();
		redraw();
	}

	void OnDblClick()
	{
		FMEntry *fm = selected();
		if (!fm)
			return;

		if ( fm->IsInstalled() )
			OnPlayFM(NULL, NULL);
		else if ( fm->IsArchived() )
			InstallFM(fm);
	}

public:
	Fl_FM_List(int x, int y, int w, int h, const char *l=0)
		: Fl_Table_Row(x,y,w,h,l)
	{
		m_font = FL_HELVETICA;
		m_fontsize = FL_NORMAL_SIZE;
		m_tagfontsize = m_tagRowH = FL_NORMAL_SIZE-1;
		m_tagY = m_topH = m_noTagH = m_fontsize + 2;// dummy value, will be recalced in post_init
		m_tagalign = FL_ALIGN_TOP_LEFT;// can alternatively be FL_ALIGN_BOTTOM_LEFT if not doing automatic row height
		m_nPendingRclickRow = -1;
		end();

		type(SELECT_SINGLE_PERSIST);
		always_show_vscroll(1);

		callback(event_callback, (void*)this);
	}

	virtual ~Fl_FM_List()
	{
	}

	// post-create initializtion
	void post_init_header()
	{
		int i;
		int num_cols = 0;

		ASSERT(g_cfg.colvis & (1<<COL_Name));

		if (g_cfg.colvis & (1<<COL_DirName))
			s_columns[COL_DirName].visible = TRUE;
		if (g_cfg.colvis & (1<<COL_Archive))
			s_columns[COL_Archive].visible = TRUE;

		for (i=0; i<COL_NUM_COLS; i++)
		{
			s_columns[i].ui_col_index = -1;

			// apply col default sizes of large font if necessary
			if (g_cfg.bLargeFont && s_columns[i].width2)
				s_columns[i].width = s_columns[i].width2;

			// count visible columns and init column index mapping table
			if (s_columns[i].visible)
			{
				s_columns[i].ui_col_index = num_cols;
				s_column_idx[num_cols] = i;
				num_cols++;
			}
		}

		cols(num_cols);
		int totw = Fl::scrollbar_size();
		col_minwidth(0, SCALECFG(s_columns[0].minwidth));
		for (i=1; i<COL_NUM_COLS; i++)
		{
			if (s_columns[i].visible)
			{
				int width = SCALECFG(s_columns[i].width);
				int minwidth = SCALECFG(s_columns[i].minwidth);
				if (width < 0) width = -1;
				if (minwidth < 0) minwidth = -1;
				totw += SCALECFG(s_columns[i].width);
				col_width(s_columns[i].ui_col_index, width);
				col_minwidth(s_columns[i].ui_col_index, minwidth);
			}
		}
		col_width(0, std::max(SCALECFG(s_columns[0].width), w()-totw));

		col_header(1);
		col_resize(1);

		// init col sizes from config
		for (i=0; i<COL_NUM_COLS; i++)
		{
			int minwidth = SCALECFG(s_columns[i].minwidth);
			if (s_columns[i].visible && g_cfg.colsizes[i] != -1 && minwidth != -1 && g_cfg.colsizes[i] >= minwidth)
				col_width(s_columns[i].ui_col_index, g_cfg.colsizes[i]);
		}
	}

	// post-create initializtion
	void post_init()
	{
		rows( (int)g_dbFiltered.size() );

		refresh_rowsize();
	}

	void refresh_rowsize()
	{
		const int notag_tophalf = std::max(imgStar.h(), imgProgress.h()) + 2;
		const int min_tophalf = g_cfg.tagrows > 0 ? imgStar.h() : notag_tophalf;

		fl_font(m_font, m_fontsize);
		const int fheight = fl_height();

		const int tophalf = m_topH = std::max(fheight, min_tophalf) + 2;
		fl_font(FL_HELVETICA, m_tagfontsize);
		m_tagRowH = fheight;
		m_noTagH = std::max(fheight, notag_tophalf) + 2;
		const int bottomhalf = g_cfg.tagrows > 0 ? m_tagRowH * g_cfg.tagrows + 2 : 0;

		m_tagY = m_topH;

		if (g_cfg.tagrows <= 0 || !g_cfg.bVarSizeList)
			row_height_all(tophalf + bottomhalf);
		else
		{
			const int maxH = tophalf + bottomhalf;

			const int W = col_width(0);
			const int X = 0;
			const int Y = 0;
			const int XX = X+4;
			const int WW = (X+W) - XX - 2;

			for (int r=0; r<rows(); r++)
			{
				FMEntry *fm = g_dbFiltered[r];

				if ( fm->tagsUI.empty() )
					row_height(r, std::min(maxH, m_noTagH));
				else
				{
					int lw = WW, lh = 0;
					fl_measure(fm->tagsUI.c_str(), lw, lh, ALIGN_NO_DRAW);

					const int bottomhalf2 = lh + 2;
					row_height(r, std::min(maxH, tophalf + bottomhalf2));
				}
			}
		}
	}

	void refresh_rowsize(FMEntry *fm)
	{
		if (g_cfg.tagrows <= 0 || !g_cfg.bVarSizeList)
			;
		else
		{
			int r = GetFilteredDbIndex(fm);
			if (r < 0)
				return;

			const int tophalf = m_topH;
			const int bottomhalf = g_cfg.tagrows > 0 ? m_tagRowH * g_cfg.tagrows + 2 : 0;

			const int maxH = tophalf + bottomhalf;

			const int W = col_width(0);
			const int X = 0;
			const int Y = 0;
			const int XX = X+4;
			const int WW = (X+W) - XX - 2;

			if ( fm->tagsUI.empty() )
				row_height(r, std::min(maxH, m_noTagH));
			else
			{
				fl_font(FL_HELVETICA, m_tagfontsize);

				int lw = WW, lh = 0;
				fl_measure(fm->tagsUI.c_str(), lw, lh, ALIGN_NO_DRAW);

				const int bottomhalf2 = lh + 2;
				row_height(r, std::min(maxH, tophalf + bottomhalf2));
			}
		}
	}

	void refresh(FMEntry *selection)
	{
		RefreshFilteredDb();
		rows( (int)g_dbFiltered.size() );
		refresh_rowsize();

		int R = 0;

		if (selection)
		{
			R = GetFilteredDbIndex(selection);
			if (R == -1)
				R = 0;
		}

		select_row_ex(R);
		ensure_visible(R, 2);

		FMEntry *newsel = selected();
		if (newsel != selection)
			OnListSelChange(newsel);

		redraw();
	}

	void refresh_order(FMEntry *selection)
	{
		pFMList->select(selection, TRUE);

		// recalc row heights if var size list entries is enabled
		if (g_cfg.tagrows > 0 && g_cfg.bVarSizeList)
			refresh_rowsize();

		pFMList->redraw();

		int R = GetFilteredDbIndex(selection);
		if (R != -1)
			ensure_visible(R, 2);
	}

	void refresh_formatting()
	{
		// recalc row heights if var size list entries is enabled
		if (g_cfg.tagrows > 0 && g_cfg.bVarSizeList)
		{
			refresh_rowsize();
			pFMList->redraw();
		}
	}

	FMEntry* selected() const
	{
		if ( row_selected(this->Fl_Table::select_row) )
			return g_dbFiltered[this->Fl_Table::select_row];

		for (int i=0; i<rows(); i++)
		{
			if ( row_selected(i) )
			{
				ASSERT((unsigned int)i < g_dbFiltered.size());
				return g_dbFiltered[i];
			}
		}

		return NULL;
	}

	void select(FMEntry *fm, BOOL bCenterSel = FALSE)
	{
		if (this->Fl_Table::select_row < 0)
			bCenterSel = TRUE;

		int R = 0;

		if (fm)
		{
			int r = GetFilteredDbIndex(fm);
			if (r != -1)
				R = r;
		}

		select_row_ex(R);
		ensure_visible(R, bCenterSel);
	}

	void DoContextMenu()
	{
		FMEntry *fm = selected();
		if (!fm)
			return;

		// auto-check for info file if none is set an none has been looked for
		if (fm->infofile.empty() && !(fm->flags & FMEntry::FLAG_NoInfoFile))
			AutoSelectInfoFile(fm);

		enum
		{
			CMD_NONE,

			CMD_Play,
			CMD_StartPlay,

			CMD_Install,
			CMD_Uninstall,

			CMD_InProgress,
			CMD_Completed,

			CMD_PrioNormal,
			CMD_PrioHigh,
			CMD_PrioUrgent,
			CMD_PrioCrit,

			CMD_RemoveRating,
			CMD_Rating0,
			CMD_Rating10 = CMD_Rating0+10,

			CMD_Delete,

			CMD_Edit,
			CMD_EditTags,

			CMD_ViewInfo,
			CMD_ViewSummary,
			CMD_ViewArchContent,
		};

		Fl_Image *imgGray = NULL;

		const BOOL bAdvanced = !!(Fl::event_state() & FL_CTRL);

		#define MENU_RATE(_name, _val) MENU_RITEM(_name, CMD_Rating0+_val, fm->rating==_val)
		#define MENU_PRIO(_val, _id) \
			MENU_ITEM("", _id); \
			if (fm->priority == (_val)+1) \
			{ \
				imgGray = imgPrio[_val]->copy(); \
				imgGray->inactive(); \
				menu[menu_items-1].image(imgGray); \
				menu[menu_items-1].deactivate(); \
			} \
			else \
				menu[menu_items-1].image(imgPrio[_val])

		const int MAX_MENU_ITEMS = 48;
		int menu_items = 0;

		Fl_Menu_Item menu[MAX_MENU_ITEMS];

		if (fm->IsInstalled() || !fm->IsArchived())
		{
			if (!g_bRunningEditor)
			{
				MENU_ITEM($("Play"), CMD_Play);
				MENU_MOD_DISABLE( !fm->IsInstalled() );
				MENU_ITEM($("Start Playthrough"), CMD_StartPlay);
				MENU_MOD_DISABLE(!fm->IsInstalled() || (fm->status == FMEntry::STATUS_InProgress));
			}
			else
			{
				MENU_ITEM($("Select"), CMD_Play);
				MENU_MOD_DISABLE( !fm->IsInstalled() );
			}
		}
		else
		{
			MENU_ITEM($("Install"), CMD_Install);
			MENU_MOD_DISABLE( g_sTempDir.empty() );
		}
		if (fm->IsInstalled() && fm->IsArchived())
		{
			MENU_MOD_DIV();
			MENU_ITEM($("Uninstall"), CMD_Uninstall);
			MENU_MOD_DISABLE( g_sTempDir.empty() );
		}
		if (bAdvanced)
		{
			MENU_MOD_DIV();
			MENU_ITEM($("Delete from Db"), CMD_Delete);
		}
		MENU_MOD_DIV();
		MENU_SUB($("Status"));
			MENU_ITEM(fm->status==FMEntry::STATUS_InProgress?$("Abort Playthrough"):$("Playing"), CMD_InProgress);
			MENU_ITEM($("Completed"), CMD_Completed);
			MENU_END();
		MENU_SUB($("Priority"));
			MENU_ITEM("", CMD_PrioNormal);
			MENU_PRIO(0, CMD_PrioHigh);
			MENU_PRIO(1, CMD_PrioUrgent);
			MENU_PRIO(2, CMD_PrioCrit);
			MENU_END();
		MENU_SUB($("Rating")); MENU_MOD_DIV();
			MENU_RITEM($("None"), CMD_RemoveRating, fm->rating == -1);
			MENU_RATE("0", 0); MENU_RATE("0.5", 1);
			MENU_RATE("1", 2); MENU_RATE("1.5", 3);
			MENU_RATE("2", 4); MENU_RATE("2.5", 5);
			MENU_RATE("3", 6); MENU_RATE("3.5", 7);
			MENU_RATE("4", 8); MENU_RATE("4.5", 9);
			MENU_RATE("5", 10);
			MENU_END();
		MENU_ITEM($("Edit..."), CMD_Edit);
		MENU_ITEM($("Edit Tags..."), CMD_EditTags);
		MENU_MOD_DIV();
		if (bAdvanced && fm->IsArchived())
		{
			MENU_ITEM($("View Archive Contents..."), CMD_ViewArchContent);
		}
		MENU_ITEM($("View Info File..."), CMD_ViewInfo);
		MENU_MOD_DISABLE(!fm->IsAvail() || fm->infofile.empty());
		MENU_ITEM($("View Summary..."), CMD_ViewSummary);
		MENU_END();

		// bold text for double-click item
		menu[0].labelfont(FL_HELVETICA_BOLD);
		//menu[0].labelsize(11);

		const Fl_Menu_Item *m = menu->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);

		if (imgGray)
			delete imgGray;

		if (!m || !m->user_data())
			return;

		const int cmd_id = (intptr_t)m->user_data();
		switch (cmd_id)
		{
		case CMD_Play:
			OnPlayFM(NULL, NULL);
			break;
		case CMD_StartPlay:
			OnStartFM(NULL, NULL);
			break;

		case CMD_Install:
			InstallFM(fm);
			break;
		case CMD_Uninstall:
			UninstallFM(fm);
			break;

		case CMD_InProgress:
			if (fm->status == FMEntry::STATUS_InProgress)
				fm->SetStatus(fm->nCompleteCount>0 ? FMEntry::STATUS_Completed : FMEntry::STATUS_NotPlayed);
			else
				fm->SetStatus(FMEntry::STATUS_InProgress);
			RedrawListControl(TRUE);
			break;
		case CMD_Completed:
			if ( fl_choice($("Completed \"%s\"?"), fl_no, fl_yes, NULL, fm->GetFriendlyName()) )
			{
				fm->OnCompleted();
				RedrawListControl(TRUE);
			}
			break;

		case CMD_RemoveRating:
			fm->SetRating(-1);
			RedrawListControl();
			break;

		case CMD_Delete:
			if ( fl_choice($("Are you sure you want to delete \"%s\" from the database?"), fl_no, fl_yes, NULL, fm->GetFriendlyName()) )
				DeleteFM(fm);
			break;

		case CMD_Edit:
			DoTagEditor(fm, TABPAGE_MISC);
			break;

		case CMD_EditTags:
			DoTagEditor(fm);
			break;

		case CMD_ViewInfo:
			if ( !fm->infofile.empty() )
			{
				if ( !FmFileExists(fm, fm->infofile.c_str()) )
					if ( !AutoSelectInfoFile(fm) )
					{
						fl_alert($("Couldn't find any info file."));
						break;
					}

				if ( !FmOpenFileWithAssociatedApp(fm, fm->infofile.c_str()) )
					fl_alert($("Failed to view info file \"%s\"."), fm->infofile.c_str());
			}
			break;

		case CMD_ViewSummary:
			ViewSummary(fm);
			break;

		case CMD_ViewArchContent:
			ViewArchiveContents(fm);
			break;

		default:
			if (cmd_id >= CMD_Rating0 && cmd_id <= CMD_Rating10)
				fm->SetRating(cmd_id - CMD_Rating0);
			else if (cmd_id >= CMD_PrioNormal && cmd_id <= CMD_PrioCrit)
				fm->SetPriority(cmd_id - CMD_PrioNormal);
		}

		redraw();
	}

	// for fluid generated code
	void textfont(int n) { m_font = n; }
	void textsize(int n) { m_fontsize = n; }
	int textsize() { return m_fontsize; }

	BOOL save_col_state(int *cols)
	{
		BOOL bChanged = FALSE;
		for (int i=0; i<COL_NUM_COLS; i++)
		{
			if (s_columns[i].ui_col_index != -1)
			{
				const int n = col_width(s_columns[i].ui_col_index);
				if (cols[i] != n)
				{
					cols[i] = n;
					bChanged = TRUE;
				}
			}
		}
		return bChanged;
	}

	static void static_init()
	{
#ifdef LOCALIZATION_SUPPORT
		// localization of column labels
		for (int i=0; i<COL_NUM_COLS; i++)
			s_columns[i].name = $(s_columns[i].name);
#endif
	}
};


Fl_FM_List::ColDef Fl_FM_List::s_columns[COL_NUM_COLS] =
{
	{"Name", 150, 150, 0, TRUE, SORT_Name},
	{"Pri", 24, -1, 26, TRUE, SORT_Priority},
	{"Status", 50, -1, 0, TRUE, SORT_Status},
	{"Last Played", 80, 60, 90, TRUE, SORT_LastPlayed},
	{"Release Date", 80, 60, 90, TRUE, SORT_ReleaseDate},
	{"Directory", 170, 60, 90, FALSE, SORT_DirName},
	{"Archive", 230, 60, 90, FALSE, SORT_Archive}
};

int Fl_FM_List::s_column_idx[COL_NUM_COLS] = {0};


void Fl_FM_List::draw_cell(TableContext context, int R, int C, int X, int Y, int W, int H)
{
	int i;
	char s[64];

	const FMEntry *fm = (R < (int)g_dbFiltered.size()) ? g_dbFiltered[R] : NULL;

	switch (context)
	{
	case CONTEXT_STARTPAGE:
		fl_font(m_font, m_fontsize);
		return;

	case CONTEXT_ROW_HEADER:
		fl_color(FL_RED);
		fl_rectf(X, Y, W, H);
		return;

	case CONTEXT_COL_HEADER:
		fl_push_clip(X, Y, W, H);
		fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, Fl::focus()==this ? (Fl_Color)(USER_CLR+1) : (Fl_Color)(FL_GRAY0+18));
		if (C < COL_NUM_COLS)
		{
			C = s_column_idx[C];

			int XX = X+3;
			const char *label = s_columns[C].name;

			fl_font(labelfont(), labelsize());
			fl_color( labelcolor() );
			fl_draw(label, XX, Y, W-3, H, FL_ALIGN_LEFT, 0, 0);

			// pseudo-column for rating (so it's possible to sort on rating)
			if (!C)
			{
				fl_draw(RATING_COL_LABEL, X+W-(5*16)-2, Y, W, H, FL_ALIGN_LEFT, 0, 0);

				if (abs(g_cfg.sortmode) == SORT_Rating)
				{
					XX = X+W-(5*16)-2;
					label = RATING_COL_LABEL;
					goto draw_sortarrow;
				}
			}

			// sort arrow
			if (s_columns[C].sortid == abs(g_cfg.sortmode))
			{
draw_sortarrow:
				int lw = 0, lh = 0;
				fl_measure(label, lw, lh);
				XX += lw+2;
				fl_color(FL_GRAY0+8);
				if (g_cfg.sortmode >= 0)
				{
					// down
					fl_yxline(XX, Y+(H>>1), Y+H-3);
					fl_xyline(XX-2, Y+H-3-2, XX+2);
					fl_xyline(XX-1, Y+H-3-1, XX+1);
				}
				else
				{
					// up
					fl_yxline(XX, Y+2, Y+(H>>1)-1);
					fl_xyline(XX-1, Y+2+1, XX+1);
					fl_xyline(XX-2, Y+2+2, XX+2);
				}
			}
		}
		fl_pop_clip();
		return;

	case CONTEXT_CELL:
		fl_push_clip(X, Y, W, H);
		{
			// bg
			if ( fm->IsInstalled() )
				fl_color(row_selected(R) ? selection_color() : ((R & 1) ? (Fl_Color)(USER_CLR+2) : FL_BACKGROUND2_COLOR));
			else if ( fm->IsAvail() )
				fl_color(row_selected(R) ? (Fl_Color)(USER_CLR+3) : ((R & 1) ? (Fl_Color)(USER_CLR+4) : (Fl_Color)(USER_CLR+5)));
			else
				fl_color(row_selected(R) ? (Fl_Color)(USER_CLR+6) : ((R & 1) ? (Fl_Color)(USER_CLR+7) : (Fl_Color)(USER_CLR+8)));
			fl_rectf(X, Y, W, H);

			if (fm)
			{
				C = s_column_idx[C];

				switch (C)
				{
				case COL_Name:
					{
					// rating (first so name can be drawn over it if column is too small
					if (fm->rating >= 0)
					{
						const int XX = X + W - 5*16 - 2;
						const int YY = (H > m_noTagH) ? Y + 1 : Y + ((H - imgStar.h()) >> 1);
						for (i=0; i<fm->rating>>1; i++)
							imgStars[2]->draw(XX+i*16, YY);
						if (fm->rating & 1)
						{
							imgStars[1]->draw(XX+i*16, YY);
							i++;
						}
						for (; i<5; i++)
							imgStars[0]->draw(XX+i*16, YY);
					}

					// name
					int iw = 0;
					if ( !fm->IsInstalled() )
					{
						if ( fm->IsAvail() )
						{
							if (fm->status == FMEntry::STATUS_NotPlayed)
							{
								fl_color((Fl_Color)(USER_CLR+9));
								if (fm->tmLastStarted)
									fl_font(FL_HELVETICA_ITALIC, m_fontsize);
								else
									fl_font(FL_HELVETICA_BOLD_ITALIC, m_fontsize);
							}
							else if (fm->status == FMEntry::STATUS_InProgress)
							{
								fl_color((Fl_Color)(USER_CLR+10));
								fl_font(FL_HELVETICA_BOLD_ITALIC, m_fontsize);
							}
							else
							{
								fl_color(FL_FOREGROUND_COLOR);
								fl_font(FL_HELVETICA_ITALIC, m_fontsize);
							}
						}
						else
						{
							fl_color(FL_GRAY0+7);
							fl_font(FL_HELVETICA_ITALIC, m_fontsize);
						}

						Fl_Pixmap *pix = fm->IsArchived() ? &imgArchived : &imgNotAvail;
						pix->draw(X+1, Y+1+std::max(0, ((m_topH-pix->h())>>1)));

						iw = pix->w() + 2;
					}
					else
					{
						if ( fm->IsArchived() )
						{
							Fl_Pixmap *pix = &imgArchiveInstalled;
							pix->draw(X+1, Y+1+std::max(0, ((m_topH-pix->h())>>1)));

							iw = pix->w() + 2;
						}

						if (fm->status == FMEntry::STATUS_NotPlayed)
						{
							fl_color(FL_BLUE);
							if (fm->tmLastStarted)
								fl_font(FL_HELVETICA, m_fontsize);
							else
								fl_font(FL_HELVETICA_BOLD, m_fontsize);
						}
						else if (fm->status == FMEntry::STATUS_InProgress)
						{
							fl_color((Fl_Color)(USER_CLR+11));
							fl_font(FL_HELVETICA_BOLD, m_fontsize);
						}
						else
						{
							fl_color(FL_FOREGROUND_COLOR);
							fl_font(FL_HELVETICA, m_fontsize);
						}
					}
					fl_draw(fm->GetFriendlyName(), X+2+iw, Y, W-(fm->rating>=0 ? 5*16+4 : 0)-iw, H, (H>m_noTagH)?FL_ALIGN_TOP_LEFT:FL_ALIGN_LEFT);

					// tags
					if (!fm->tagsUI.empty() && (H>m_noTagH))
					{
						int XX = X+4;
						int YY = Y+m_tagY;
						int WW = (X+W) - XX - 2;
						int HH = H-m_tagY-2;

						fl_font(FL_HELVETICA, m_tagfontsize);
						fl_color(FL_GRAY0+16);
						fl_draw(TAGS_LABEL, XX+1, YY, WW, HH, (Fl_Align)(m_tagalign|FL_ALIGN_CLIP|FL_ALIGN_WRAP));
						fl_color(FL_GRAY0+7);
						fl_draw(fm->tagsUI.c_str(), XX, YY, WW, HH, (Fl_Align)(m_tagalign|FL_ALIGN_CLIP|FL_ALIGN_WRAP));
					}
					}
					break;

				case COL_Status:
					if (fm->status)
					{
						int XX = X+((W-24)>>1);
						int YY = Y+((H-20)>>1);

						switch (fm->status)
						{
						case FMEntry::STATUS_InProgress:
							imgProgress.draw(XX, YY);
							break;
						case FMEntry::STATUS_Completed:
							imgCheck.draw(XX, YY);
							break;
						}

						if (fm->nCompleteCount > 1
							|| (fm->nCompleteCount == 1 && fm->status == FMEntry::STATUS_InProgress))
						{
							if (H > m_noTagH)
							{
								XX += 24+4;
								YY += 20-6;
							}
							else
							{
								XX = X+W-4;
								YY = Y+3;
							}
							sprintf(s, "%d", fm->nCompleteCount);
							fl_font(FL_HELVETICA, FL_NORMAL_SIZE-3);
							fl_color(FL_BACKGROUND2_COLOR);
							fl_draw(s, XX+1, YY+1, 0, 0, FL_ALIGN_TOP_RIGHT);
							fl_draw(s, XX-1, YY-1, 0, 0, FL_ALIGN_TOP_RIGHT);
							fl_color(FL_FOREGROUND_COLOR);
							fl_draw(s, XX, YY, 0, 0, FL_ALIGN_TOP_RIGHT);
						}
					}
					break;

				case COL_Priority:
					if (fm->priority)
					{
						const int XX = X+((W-15)>>1);
						const int YY = Y+((H-15)>>1);
						imgPrio[fm->priority-1]->draw(XX, YY);
					}
					break;

				case COL_LastPlayed:
					if (fm->tmLastStarted)
					{
						FormatDbDate(fm->tmLastStarted, s, sizeof(s), TRUE);

						fl_font(FL_HELVETICA, m_fontsize);
						fl_color(FL_FOREGROUND_COLOR);
						fl_draw(s, X, Y, W, H, FL_ALIGN_CENTER);
					}
					break;

				case COL_ReleaseDate:
					if (fm->tmReleaseDate)
					{
						FormatDbDate(fm->tmReleaseDate, s, sizeof(s), FALSE);

						if (fm->flags & FMEntry::FLAG_UnverifiedRelDate)
						{
							fl_font(FL_HELVETICA_ITALIC, m_fontsize);
							fl_color(FL_GRAY0+10);
						}
						else
						{
							fl_font(FL_HELVETICA, m_fontsize);
							fl_color(FL_FOREGROUND_COLOR);
						}
						fl_draw(s, X, Y, W, H, FL_ALIGN_CENTER);
					}
					break;

				case COL_DirName:
					fl_font(FL_HELVETICA, m_fontsize);
					fl_color(FL_FOREGROUND_COLOR);
					fl_draw(fm->name_utf.c_str(), X+2, Y, W-4, H, FL_ALIGN_LEFT);
					break;

				case COL_Archive:
					if ( fm->IsArchived() )
					{
						fl_font(FL_HELVETICA, m_fontsize);
						fl_color(FL_FOREGROUND_COLOR);
						fl_draw(fm->archive.c_str(), X+2, Y, W-4, H, FL_ALIGN_LEFT);
					}
					break;
				}
			}

			// border
			fl_color(FL_GRAY);
			//fl_rect(X, Y, W, H);
			fl_xyline(X, Y+H-1, X+W-1);

			fl_color((Fl_Color)(USER_CLR+12));
			fl_line_style(FL_DOT);
			fl_yxline(X+W-1, Y, Y+H-1);
			fl_line_style(FL_SOLID);
		}
		fl_pop_clip();
		return;

	case CONTEXT_ENDPAGE:
	case CONTEXT_RC_RESIZE:
	case CONTEXT_RC_RESIZED:
	case CONTEXT_NONE:
	case CONTEXT_TABLE:
		return;
	}
}

void Fl_FM_List::OnCallback()
{
	int /*R = callback_row(),*/ Ci = callback_col();
	int C = s_column_idx[Ci];

	TableContext context = callback_context();

	switch (context)
	{
	case CONTEXT_COL_HEADER:
		if (Fl::event() == FL_RELEASE && Fl::event_button() == 1)
		{
			int sortid = s_columns[C].sortid;
			if (sortid == SORT_None)
				break;

			// check for pseudo column "Rating"
			if (!C)
			{
				const int X = pFMList->x();
				const int W = pFMList->col_width(Ci);
				if (Fl::event_x() >= (X+W-(5*16)-2))
					sortid = SORT_Rating;
			}

			if (sortid == abs(g_cfg.sortmode))
			{
				// flip sort order
				g_cfg.SetSortMode(-g_cfg.sortmode);
			}
			else
			{
				g_cfg.SetSortMode(sortid);
			}
		}
		break;

	case CONTEXT_CELL:
		OnListSelChange( selected() );
		break;

	case CONTEXT_RC_RESIZED:
		refresh_formatting();
		break;

	case CONTEXT_STARTPAGE:
	case CONTEXT_ENDPAGE:
	case CONTEXT_ROW_HEADER:
	case CONTEXT_TABLE:
	case CONTEXT_RC_RESIZE:
	case CONTEXT_NONE:
		break;
	}
}


static FMEntry* GetCurSelFM()
{
	return pFMList->selected();
}

static void RefreshListControl(FMEntry *pPrevSel, BOOL bOnlyChangedOrder)
{
	if (bOnlyChangedOrder)
		pFMList->refresh_order(pPrevSel);
	else
		pFMList->refresh(pPrevSel);
}

static void RefreshListControlRowSize()
{
	pFMList->refresh_rowsize();
	pFMList->redraw();
}

static void RedrawListControl(BOOL bUpdateButtons)
{
	pFMList->redraw();

	if (bUpdateButtons)
		OnListSelChange( pFMList->selected() );
}

static void SelectNeighborFM()
{
	// select next FM, or previous if current selection is last (called before an FM entry is removed)

	FMEntry *fm = pFMList->selected();
	if (!fm)
		return;

	int i = GetFilteredDbIndex(fm);
	if (i < 0)
		return;

	if (g_dbFiltered.size() <= 1)
		return;

	if (!i || i < ((int)g_dbFiltered.size()-1))
		// select next
		fm = g_dbFiltered[i+1];
	else if (i > 0)
		// select previous
		fm = g_dbFiltered[i-1];

	pFMList->select(fm);
	OnListSelChange(fm);
}


//
// FM_Link_Button
//

class Fl_Link_Button : public Fl_Button
{
protected:
	Fl_Color m_hoverclr;
	int m_underlineY;
	int m_underlineW;

protected:
	virtual void draw()
	{
		if (type() == FL_HIDDEN_BUTTON) return;
		draw_box(box(), parent()->color());

		int clr = labelcolor();
		if (Fl::belowmouse() == this)
			labelcolor(m_hoverclr);

		draw_label(x(), y(), w(), h());
		// underline
		fl_xyline(x(), y()+m_underlineY+2-fl_descent(), x()+m_underlineW);

		labelcolor(clr);
		if (Fl::focus() == this) draw_focus();
	}

	virtual int handle(int e)
	{
		switch (e)
		{
		case FL_ENTER:
			fl_cursor(FL_CURSOR_HAND);
			redraw();
			break;
		case FL_LEAVE:
			fl_cursor(FL_CURSOR_DEFAULT);
			redraw();
			break;
		}

		return Fl_Button::handle(e);
	}

	void autosize_to_label()
	{
		// auto-size
		int lw = 0, lh = 0;
		fl_font(labelfont(), labelsize());
		fl_measure(label(), lw, lh);
		m_underlineY = lh;
		m_underlineW = lw;
		size(lw, lh+2);
	}

public:
	Fl_Link_Button(int X, int Y, const char *l,
		Fl_Color clr = (Fl_Color)(USER_CLR+13), Fl_Color hoverclr = (Fl_Color)(USER_CLR+14),
		Fl_Font font = FL_HELVETICA_BOLD, int fontsize = FL_NORMAL_SIZE)
		: Fl_Button(X,Y,1,1,l)
	{
		box(FL_FLAT_BOX);
		down_box(FL_FLAT_BOX);
		labelfont(font);
		labelsize(fontsize);
		labelcolor(clr);
		align(FL_ALIGN_TOP_LEFT | FL_ALIGN_INSIDE);

		m_hoverclr = hoverclr;

		if (l)
			autosize_to_label();
	}
};


//
// Fl_AddTagFilter_Button
//

class Fl_AddTagFilter_Button : public Fl_Link_Button
{
	string m_label;

public:
	Fl_AddTagFilter_Button(int X, int Y, BOOL bNoFilter = FALSE)
		: Fl_Link_Button(X,Y,NULL,(Fl_Color)(USER_CLR+13),(Fl_Color)(USER_CLR+14),FL_HELVETICA_BOLD,FL_NORMAL_SIZE)
	{
		m_label = bNoFilter ? $("Add Tag") : $("Add Tag Filter");

		// underline width (excludes " »")
		fl_font(labelfont(), labelsize());
		const int underlineW = (int)fl_width( m_label.c_str() ) - 1;

		// append " »" to label
		m_label += " »";
		label( m_label.c_str() );
		autosize_to_label();

		m_underlineW = underlineW;

		tooltip(bNoFilter ? $("Select a tag from list to add") : $("Select a tag from list to add to tag filters"));
	}
};


//
// FM_Html_Popup
//

class Fl_FM_Html_Popup : public Fl_Double_Window
{
protected:
	BOOL m_bDone;
	BOOL m_bClickedLink;
	Fl_Widget *m_pushed;

	static char s_retlink[256];

protected:
	virtual int handle(int e)
	{
		switch (e)
		{
		case FL_KEYBOARD:
			switch ( Fl::event_key() )
			{
			case FL_Escape:
				m_bDone = TRUE;
				return 1;
			}
			break;

		case FL_PUSH:
			if (m_bDone)
				return 1;
			{
			// close if clicked outside of popup
			int mx = Fl::event_x_root();
			int my = Fl::event_y_root();
			if (mx < x_root() || mx >= x_root()+w() || my < y_root() || my >= y_root()+h())
			{
				m_bDone = TRUE;
				return 1;
			}
			}
			// workaround for release events not reaching child widgets (ie scrollbar buttons) when grab is enabled
			e = Fl_Double_Window::handle(e);
			m_pushed = Fl::pushed();
			if (m_pushed == child(0))
				m_pushed = NULL;
			return e;

		// workaround for release events not reaching child widgets (ie scrollbar buttons) when grab is enabled
		case FL_RELEASE:
			if (m_pushed && !Fl::pushed())
			{
				m_pushed->handle(e);
				m_pushed = NULL;
				return 1;
			}
			m_pushed = NULL;
			break;
		}

		return Fl_Double_Window::handle(e);
	}

	static const char *link_cb(Fl_Widget *o, const char *url)
	{
		Fl_FM_Html_Popup *w = (Fl_FM_Html_Popup*)((Fl_Html_View*)o)->user_data2();
		ASSERT(w != NULL);

		BOOL bClose = TRUE;

		if (w->callback() != (Fl_Callback*)default_callback)
		{
			bClose = ((BOOL (*)(Fl_Widget*,void*))w->callback())(o, (void*)url);
			w->clear_changed();
		}
		else
			w->ReturnLink(url);

		w->m_bDone = !!bClose;

		return NULL;
	}

	static void draw_frame(int x, int y, int w, int h, Fl_Color c)
	{
		fl_color(c);
		fl_rectf(x+4, y+4, w-8, h-8);
		fl_color(FL_FOREGROUND_COLOR);
		fl_rect(x, y, w, h);
		//fl_color( fl_rgb_color(130,120,115) );
		fl_color((Fl_Color)(USER_CLR+15));
		fl_rectf(x+1, y+1, w-2, 3);
		fl_rectf(x+1, y+4, 3, h-8);
		fl_rectf(x+w-4, y+4, 3, h-8);
		fl_rectf(x+1, y+h-4, w-2, 3);
	}

public:
	Fl_FM_Html_Popup(int X, int Y, int W, int H)
		: Fl_Double_Window(X,Y,W,H,0)
	{
		m_bDone = FALSE;
		m_bClickedLink = FALSE;
		m_pushed = NULL;

		end();
		set_modal();
		clear_border();
		box(FL_NO_BOX);
	}

	virtual ~Fl_FM_Html_Popup()
	{
		hide();
	}

	void DeferredClose()
	{
		m_bDone = TRUE;
	}

	void ReturnLink(const char *url)
	{
		m_bClickedLink = TRUE;
		strcpy_safe(s_retlink, sizeof(s_retlink), url);
	}

	void do_popup()
	{
		m_bDone = FALSE;
		m_bClickedLink = FALSE;

		fl_cursor(FL_CURSOR_DEFAULT);

		show();

		Fl::grab(this);

		while (!m_bDone)
		{
			Fl_Widget *o = Fl::readqueue();
#ifdef _WIN32
			if (!o) Fl::wait();
#else
			if (!o) Fl::wait(0);
#endif
		}

		Fl::release();

		hide();
	}

	// if linkcb is NULL then popup will close after clicking a link and the link name will be returned
	// otherwise the linkcb controls when the popup closes and the return value is always NULL
	static const char* popup(int X, int Y, int W, int H, const char *html, BOOL (*linkcb)(Fl_Html_View*, const char*) = NULL)
	{
		if (!html)
		{
			ASSERT(FALSE);
			return NULL;
		}

		X += Fl::event_x_root() - Fl::event_x();
		Y += Fl::event_y_root() - Fl::event_y();

		Fl_FM_Html_Popup w(X,Y,W,H);

		if (linkcb)
			w.callback((Fl_Callback*)linkcb);

		Fl_Html_View *o = new Fl_Html_View(0, 0, w.w(), w.h());
		w.resizable(o);

		o->user_data2(&w);
		o->box(HTML_POPUP_BOX);
		o->color((Fl_Color)(USER_CLR+16));
		//o->textfont(FL_HELVETICA_BOLD);
		o->textfont(FL_HELVETICA);
		o->textsize(FL_NORMAL_SIZE);
		//o->textcolor( fl_rgb_color(200,230,250) );
		o->textcolor(DARK() ? (Fl_Color)FL_GRAY0 : (Fl_Color)(USER_CLR+12));
		o->link(link_cb);
		o->include_anchor_to_cb();

		o->value(html);

		// shrink height to fit document if document is smaller
		if (o->size() < o->h() - Fl::box_dh( o->box() ))
		{
			// bypass resize of Fl_Html_View to avoid a complete re-parse for the html code
			o->Fl_Widget::resize(o->x(), o->y(), o->w(), o->size());
			w.size(w.w(), o->h());
		}

		w.add(o);

		w.do_popup();

		return (linkcb || !w.m_bClickedLink) ? NULL : s_retlink;
	}

	static void static_init()
	{
		Fl::set_boxtype(HTML_POPUP_BOX, draw_frame, 4, 4, 8, 8);
	}
};

char Fl_FM_Html_Popup::s_retlink[256];


//
// FM_TagFilter_Button
//

class Fl_FM_TagFilter_Button : public Fl_Widget
{
	int m_nicelabelX;
	string m_nicelabel;
	string m_category;

	BOOL m_bHilightedX;

protected:
	enum
	{
		XBUTTON_WIDTH = 17,//16,
		LABEL_MARGIN_LEFT = 4,
		LABEL_MARGIN_RIGHT = 4,
	};

	virtual void draw()
	{
		const BOOL bFocus = (Fl::focus() == this);

		fl_color( color() );
		fl_rectf(x()+1, y()+1, w()-XBUTTON_WIDTH-1, h()-2);

		// X button
		const int X = x()+w()-XBUTTON_WIDTH;
		const BOOL bMouseOverX = m_bHilightedX = (Fl::belowmouse() == this && Fl::event_x() >= X);
		fl_color(bMouseOverX ? (Fl_Color)(USER_CLR+17) : (Fl_Color)(USER_CLR+18));
		fl_rectf(X, y()+1, XBUTTON_WIDTH, h()-2);
		imgTagX.draw(X+4, y()+4);

		fl_color(bFocus ? FL_FOREGROUND_COLOR : (Fl_Color)(USER_CLR+19));
		fl_xyline(x()+1, y(), x()+w()-2);
		fl_xyline(x()+1, y()+h()-1, x()+w()-2);
		fl_yxline(x(), y()+1, y()+h()-2);
		fl_yxline(x()+w()-1, y()+1, y()+h()-2);

		fl_yxline(X, y()+2, y()+h()-3);
		fl_xyline(X-1, y()+1, X-1);
		fl_xyline(X-1, y()+h()-2, X-1);

		fl_color(FL_FOREGROUND_COLOR);
		fl_font(labelfont(), labelsize());
		fl_draw(m_nicelabel.c_str(),
			x()+LABEL_MARGIN_LEFT+m_nicelabelX, y()+2,
			w()-XBUTTON_WIDTH-(LABEL_MARGIN_LEFT+LABEL_MARGIN_RIGHT)-m_nicelabelX, h()-4,
			(Fl_Align)(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CLIP), 0, 0);

		if ( !m_category.empty() )
		{
			fl_color((Fl_Color)(USER_CLR+20));
			fl_draw(m_category.c_str(),
				x()+LABEL_MARGIN_LEFT, y()+2,
				w()-XBUTTON_WIDTH-(LABEL_MARGIN_LEFT+LABEL_MARGIN_RIGHT), h()-4,
				(Fl_Align)(FL_ALIGN_LEFT|FL_ALIGN_INSIDE|FL_ALIGN_CLIP), 0, 0);
		}
	}

	virtual int handle(int e)
	{
		switch (e)
		{
		case FL_FOCUS:
		case FL_UNFOCUS:
			redraw();
			return 1;

		case FL_MOVE:
			{
			const int X = x()+w()-XBUTTON_WIDTH;
			const BOOL bMouseOverX = (Fl::belowmouse() == this && Fl::event_x() >= X);
			if (m_bHilightedX != bMouseOverX)
				redraw();
			}
			break;

		case FL_PUSH:
			if (Fl::visible_focus() && handle(FL_FOCUS))
				Fl::focus(this);
			return 1;

		case FL_RELEASE:
			{
			const int X = x()+w()-XBUTTON_WIDTH;
			const BOOL bMouseOverX = (Fl::belowmouse() == this && Fl::event_x() >= X);
			if (bMouseOverX)
				OnDelete();
			}
			return 1;

		case FL_ENTER:
			//fl_cursor(FL_CURSOR_HAND);
			redraw();
			return 1;
		case FL_LEAVE:
			//fl_cursor(FL_CURSOR_DEFAULT);
			redraw();
			return 1;

		case FL_KEYBOARD:
			if (Fl::event_key() == ' ')
			{
				OnDelete();
				return 1;
			}
			break;
		}

		return Fl_Widget::handle(e);
	}

	void OnDelete();

public:
	Fl_FM_TagFilter_Button(int X, int Y, int H, const char *l, int op, int maxW)
		: Fl_Widget(X,Y,1,1,l)
	{
		m_bHilightedX = FALSE;

		box(FL_FLAT_BOX);
		switch (op)
		{
		case FOP_AND: color((Fl_Color)(USER_CLR+21)); break;
		case FOP_NOT: color((Fl_Color)(USER_CLR+22)); break;
		default:
			color((Fl_Color)(USER_CLR+23));
		}
		labelfont(FL_HELVETICA);
		labelsize(FL_NORMAL_SIZE);
		set_visible_focus();

		const int margin = XBUTTON_WIDTH + LABEL_MARGIN_LEFT + LABEL_MARGIN_RIGHT;

		m_nicelabelX = 0;
		// make label a bit more display friendly (and localized)
		const char *colon = strchr(l, ':');
		if (colon)
		{
			*(char*)colon = 0;
			m_category = $tag(l);
			*(char*)colon = ':';
			m_category += ":";
			// don't run "*" through localization, it's a reserved wildcard tag
			m_nicelabel = (colon[1]=='*' && !colon[2]) ? (colon+1) : $tag(colon+1);
		}
		else
			m_nicelabel = $tag(l);

		maxW -= margin;

		// auto-size
		int lw = 0, lh = 0;
		fl_font(labelfont(), labelsize());
		if (colon)
		{
			fl_measure(m_category.c_str(), lw, lh);
			m_nicelabelX = lw + 2;
			lw = lh = 0;
			fl_measure(m_nicelabel.c_str(), lw, lh);
			lw += m_nicelabelX;
		}
		else
			fl_measure(m_nicelabel.c_str(), lw, lh);
		if (lw > maxW)
		{
			lw = maxW;
			// TODO: render clipped text with "..." at end ?, allthough this is very low prio since in practice should never happen
		}

		lw += margin;

		size(lw, H);
	}
};


//
// FM_Ellipsis_Label
//

class FM_Ellipsis_Label : public Fl_Box
{
public:
	FM_Ellipsis_Label(int X, int Y, int H)
		: Fl_Box(X,Y,1,1,"...")
	{
		box(FL_NO_BOX);
		labelfont(FL_HELVETICA_BOLD);
		labelsize(14);

		// auto-size
		int lw = 0, lh = 0;
		fl_font(labelfont(), labelsize());
		fl_measure(label(), lw, lh);
		size(lw, H);
	}
};


//
// FM_TagFilter_Group
//

class Fl_FM_TagFilter_Group : public Fl_Group
{
protected:
	enum
	{
		BORDER_WIDTH = 3,	// border margin (on one side)
		BUTTON_SPACING = 4,	// spacing between tag buttons
		ROW_SPACING = 3,
	};

	// this widget can also be used to edit tags for an FMEntry
	BOOL m_bTagEdit;
	int m_buttonheight;

protected:
	static void addtag_cb(Fl_Widget *o, void *p) { ((Fl_FM_TagFilter_Group*)p)->OnAddTagFilter(); }

	virtual BOOL IsTagAdded(const char *tag) const
	{
		return IsTagInFilterList(tag);
	}

	virtual BOOL HasTags() const { return FALSE; }

	virtual void OnClickLink(const char *link)
	{
		int op = FOP_OR;
		if (Fl::event_state() & FL_SHIFT)
			op = FOP_AND;
		else if (Fl::event_state() & (FL_ALT|FL_CTRL))
			op = FOP_NOT;

		if ( !strcmp(link, "*") )
			// special reset "command"
			g_cfg.ResetTagFilters();
		else
			AddTagFilter(link, op);

		OnFilterChanged();
	}

public:
	Fl_FM_TagFilter_Group(int X, int Y, int W, int H,const char *l=0)
		: Fl_Group(X,Y,W,H,l)
	{
		m_bTagEdit = FALSE;
		m_buttonheight = SCALECFG(17);

		end();
		resizable(NULL);
	}

	void init()
	{
		Fl_AddTagFilter_Button *o = new Fl_AddTagFilter_Button(x()+BORDER_WIDTH, y()+BORDER_WIDTH, m_bTagEdit);
		o->callback(addtag_cb, this);
		add(o);

		FM_Ellipsis_Label *ellipsis = new FM_Ellipsis_Label(x()+BORDER_WIDTH, y()+BORDER_WIDTH, m_buttonheight);
		ellipsis->hide();
		add(ellipsis);
	}

	// add tag filter from other places than the add filter popup, uses current ALT/SHIFT/CTRL key state to determine op
	void ExternalFilterAdd(const char *tag)
	{
		OnClickLink(tag);
	}

	void OnAddTagFilter()
	{
		int X = x() + 10;
		int Y = y() + child(0)->h() + 10;
		int W = w() - 40;
		int H = (m_bTagEdit ? std::max(pMainWnd->h(), ((Fl_Widget*)pTagEdWnd)->h()) : pMainWnd->h()) - Y - 20;

		const int MAX_W = SCALECFG((g_cfg.bLargeFont ? 770 : 630));
		const int MAX_H = SCALECFG((g_cfg.bLargeFont ? 980 : 800));

		if (W >= MAX_W)
			W = MAX_W;
		if (H >= MAX_H)
			H = MAX_H;

		string html = GenerateHtml();

		if ( html.empty() )
			return;

		const char *link = Fl_FM_Html_Popup::popup(X, Y, W, H, html.c_str());
		if (!link)
			return;

		// strip leading slash (used to trick Fl_Html_View into delivering the link as-is to the callback)
		ASSERT(link[0] == '/');
		link++;

		OnClickLink(link);
	}

	// remove handler called when clicking X on a filter button
	virtual void OnTagFilterRemove(const char *tagfilter)
	{
		ASSERT(tagfilter != NULL);

		RemoveTagFilter(tagfilter);

		OnFilterChanged();
	}

	virtual void OnFilterChanged()
	{
		// delete any existing tag buttons (the first button is our Add button and second the ellipsis label)
		while (children() > 2)
		{
			Fl_Widget *o = child(2);
			remove(o);
			delete o;
		}

		const int maxw = w() - BORDER_WIDTH*2;

		// create all tag buttons (their placement will be updated after)
		for (int j=0; j<FOP_NUM_OPS; j++)
			for (int i=0; i<(int)g_cfg.tagFilterList[j].size(); i++)
				insert(*new Fl_FM_TagFilter_Button(x(), y(), m_buttonheight, g_cfg.tagFilterList[j][i], j, maxw), children());

		RearrangeButtons();
	}

	void RearrangeButtons()
	{
		const int maxw = w() - BORDER_WIDTH*2;
		const int listbottom = m_bTagEdit ? y() + h() : pFMList->y() + pFMList->h();

		int X = BORDER_WIDTH;
		int Y = BORDER_WIDTH;
		int rows = 1;

		// Fl_AddTagFilter_Button
		X += child(0)->x() + child(0)->w() - x() + 2;

		const int GROUP_LIST_SPACE = 8;
		const int MIN_LIST_HEIGHT = 160;
		const int max_group_bottom = m_bTagEdit ? listbottom : ((listbottom - MIN_LIST_HEIGHT) - GROUP_LIST_SPACE);
		const int maxrows = (((max_group_bottom - y()) - (BORDER_WIDTH * 2 + m_buttonheight)) / (m_buttonheight + ROW_SPACING)) + 1;

		// hide ellipsis
		Fl_Widget *ellipsis = child(1);
		ellipsis->clear_visible();

		const int n = children() - 2;
		for (int i=0; i<n; i++)
		{
			Fl_Widget *o = child(i+2);

			o->set_visible();

			// check if button fits on current row
			if ((X + o->w()) > maxw)
			{
				if (rows >= maxrows)
				{
					// can't fit any more controls, add "..." label

					// hide  remaining buttons that don't fit
					for (; i<n; i++)
						child(i+2)->clear_visible();
					o = child(i+1);

					// show ellipsis
					ellipsis->position(x()+X+BUTTON_SPACING, y()+Y);
					ellipsis->set_visible();

					// check if ellipsis fits after the last fitting button, if not then delete than one too
					if (ellipsis->x()+ellipsis->w() > x()+w()-BORDER_WIDTH)
					{
						ellipsis->position(o->x(), ellipsis->y());
						child(i+1)->clear_visible();
					}

					break;
				}
				rows++;
				Y += m_buttonheight + ROW_SPACING;
				X = BORDER_WIDTH;
			}

			o->position(x() + X, y() + Y);

			X += o->w() + BUTTON_SPACING;
		}

		if (!m_bTagEdit)
		{
			int newh = BORDER_WIDTH * 2 + m_buttonheight + (rows - 1) * (m_buttonheight + ROW_SPACING);
			if (newh != h())
			{
				Fl_Group::resize(x(), y(), w(), newh);

				const int newy = y() + h() + GROUP_LIST_SPACE;
				pFMList->resize(pFMList->x(), newy, pFMList->w(), listbottom - newy);
			}
		}

		parent()->redraw();
	}

	virtual void resize(int xx, int yy, int ww, int hh)
	{
		Fl_Group::resize(xx, yy, ww, hh);

		if ( children() )
			RearrangeButtons();
	}

	string GenerateHtml()
	{
		string html;
		int i, j;
		char buff[1024];
		char buff2[512];

		RefreshTagDb();

		const int COLS = 3;
		#define TABLE_COLS_STR "3"

		// color of generic text in group/category headers
		//#define GROUP_LABEL_CLR "#FFFFFF"
		#define GROUP_LABEL_CLR "#87C5F0"
		#define GROUP_LABEL_CLR_DARK "#4A86AF"
		// color of "disabled" link for already added tags
		#define TAG_ADDED_CLR "#74ABE2"
		#define TAG_ADDED_CLR_DARK "#386EA1"
		// same as above but for tag categories
		//#define CAT_ADDED_CLR "#87C5F0"
		#define CAT_ADDED_CLR "#9BC1DB"
		#define CAT_ADDED_CLR_DARK "#5F839B"

		const char *header;
		string headerstr;
		if (!m_bTagEdit)
		{
			headerstr = "<html><body bgcolor=\"";
			headerstr.append(DARK() ? "#003E75\" link=\"#89A6B9\">" : "#3C78B4\" link=\"#C8E6FA\">");
			headerstr.append(
				"<table width=\"98%%\"><tr>"
				"<td width=\"20%%\"><font size=\"2.7\" color=\"");
			headerstr.append(DARK() ? "#000019" : "#18426C");
			headerstr.append(
				"\"><b>&lt;" $mid("SHIFT") "&gt; :</b> " $mid("AND filter") "</font></td>"
				"<td width=\"2%%\"></td>"
				"<td width=\"20%%\"><font size=\"2.7\" color=\"");
			headerstr.append(DARK() ? "#000019" : "#18426C");
			headerstr.append(
				"\"><b>&lt;" $mid("ALT") "&gt; :</b> " $mid("NOT filter") "</font></td>"
				"<td width=\"57%%\" align=\"right\"><b>%s</b></td>"
				"<td width=\"1%%\"></td>"
			"</tr></table>");
		}
		else
		{
			headerstr = "<html><body bgcolor=\"";
			headerstr.append(DARK() ? "#003E75\" link=\"#89A6B9" : "#3C78B4\" link=\"#C8E6FA");
			headerstr.append(
				"\">"
			"<table width=\"98%%\"><tr>"
				"<td width=\"99%%\" align=\"right\"><b>%s</b></td>"
				"<td width=\"1%%\"></td>"
			"</tr></table>");
		}

		header = headerstr.c_str();

		const char *footer =
			"</body></html>";

		const char *table_header =
			"<center><table width=\"98%%\">"
				"<tr bgcolor=\"%s\">"
					"<td colspan=\"" TABLE_COLS_STR "\">%s</td>"
				"</tr>"
				"<tr>";

		const char *table_footer =
				"</tr>"
			"</table></center><br>";

		if (!m_bTagEdit)
		{
			if ( g_cfg.HasTagFilters() )
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<a color=\"%s\" href=\"/*\">%s</a>", DARK() ? "#948700" : "#D2C62A", $("Reset Tag Filters"));
			else
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<font color=\"%s\">%s</font>", DARK() ? "#25629C" : "#629FDD", $("Reset Tag Filters"));

			_snprintf_s(buff, sizeof(buff), _TRUNCATE, header, buff2);
		}
		else
		{
			if ( HasTags() )
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<a color=\"%s\" href=\"/*\">%s</a>", DARK() ? "#948700" : "#D2C62A" , $("Remove All Tags"));
			else
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<font color=\"%s\">%s</font>", DARK() ? "#25629C" : "#629FDD", $("Remove All Tags"));

			_snprintf_s(buff, sizeof(buff), _TRUNCATE, header, buff2);
		}
		html.append(buff);

		const char *sCategory = $("Category");

		// tag categories
		for (i=0; i<(int)g_tagNameList.size();)
		{
			const char *cat = g_tagNameList[i];
			ASSERT( strchr(cat,':') );
			const int cat_len = strchr(cat,':') - cat + 1;

			// determine if category should be display as a html link or not (in tag editor it's always a link,
			// otherwise only link if not already added as a filter)
#ifdef _WIN32
			strncpy_s(buff, sizeof(buff), cat, cat_len);
#else
			memset(buff, 0, sizeof(buff));
			strncpy(buff, cat, sizeof(buff) > (size_t)cat_len ? cat_len : sizeof(buff)-1);
#endif
			const int cat_fm_count = g_dbTagCountHash[KEY_UTF(buff)];
			buff[cat_len] = '*'; buff[cat_len+1] = 0;
			const BOOL cat_is_filtered = !m_bTagEdit && IsTagInFilterList(buff);
			buff[cat_len-1] = 0;
			if (!cat_is_filtered)
			{
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<b><font color=\"%s\">%s:</font> <a href=\"/%s:*\">%s</a></b> (%d)",
					DARK() ? GROUP_LABEL_CLR_DARK : GROUP_LABEL_CLR, sCategory, buff, $tag(buff), cat_fm_count);
			}
			else
			{
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<b><font color=\"%s\">%s:</font> </b><font color=\"%s\"><b>%s</b> (%d)</font>",
					DARK() ? GROUP_LABEL_CLR_DARK : GROUP_LABEL_CLR, sCategory, DARK() ? CAT_ADDED_CLR_DARK : CAT_ADDED_CLR, $tag(buff), cat_fm_count);
			}
			_snprintf_s(buff, sizeof(buff), _TRUNCATE, table_header, DARK() ? "#035493" : "#4C90D4" , buff2);
			html.append(buff);

			// count number of tags in current category
			int num_in_cat = 1;
			for (j=i+1; j<(int)g_tagNameList.size() && !strncasecmp_utf_b(g_tagNameList[j],cat,cat_len); j++, num_in_cat++);

			const int ROWS = (num_in_cat + (COLS-1)) / COLS;

			// add (3 columns of) tags in current category
			for (j=0; j<3; j++)
			{
				html.append(j ? "<td width=\"33%\">" : "<td width=\"34%\">");
				for (int k=0; k<ROWS && num_in_cat; i++, k++, num_in_cat--)
				{
					if ( !IsTagAdded(g_tagNameList[i]) )
					{
						_snprintf_s(buff, sizeof(buff), _TRUNCATE, "<b><a href=\"/%s\">%s</a></b> (%d)<br>",
							g_tagNameList[i], $tag(g_tagNameList[i]+cat_len), g_dbTagCountHash[KEY_UTF(g_tagNameList[i])]);
					}
					else
					{
						_snprintf_s(buff, sizeof(buff), _TRUNCATE, "<font color=\"%s\"><b>%s</b> (%d)<font><br>",
							DARK() ? TAG_ADDED_CLR_DARK : TAG_ADDED_CLR, $tag(g_tagNameList[i]+cat_len), g_dbTagCountHash[KEY_UTF(g_tagNameList[i])]);
					}

					html.append(buff);
				}
				html.append("</td>");
			}

			html.append(table_footer);
		}

		// uncategorized tags
		if ( !g_tagNameListNoCat.empty() )
		{
			const int ROWS = (g_tagNameListNoCat.size() + (COLS-1)) / COLS;

			_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, "<b><font color=\"%s\">%s</font></b>", DARK() ? GROUP_LABEL_CLR_DARK : GROUP_LABEL_CLR, $("Misc Tags"));
			_snprintf_s(buff, sizeof(buff), _TRUNCATE, table_header, DARK() ? "#035493" : "#4C90D4" , buff2);
			html.append(buff);

			i = 0;
			for (j=0; j<3; j++)
			{
				html.append(j ? "<td width=\"33%\">" : "<td width=\"34%\">");
				for (int k=0; k<ROWS && i<(int)g_tagNameListNoCat.size(); i++, k++)
				{
					if ( !IsTagAdded(g_tagNameListNoCat[i]) )
					{
						_snprintf_s(buff, sizeof(buff), _TRUNCATE, "<b><a href=\"/%s\">%s</a></b> (%d)<br>",
							g_tagNameListNoCat[i], $tag(g_tagNameListNoCat[i]), g_dbTagCountHash[KEY_UTF(g_tagNameListNoCat[i])]);
					}
					else
					{
						_snprintf_s(buff, sizeof(buff), _TRUNCATE, "<font color=\"%s\"><b>%s</b> (%d)</font><br>",
							DARK() ? TAG_ADDED_CLR_DARK : TAG_ADDED_CLR, $tag(g_tagNameListNoCat[i]), g_dbTagCountHash[KEY_UTF(g_tagNameListNoCat[i])]);
					}

					html.append(buff);
				}
				html.append("</td>");
			}

			html.append(table_footer);
		}

		if (g_tagNameList.empty() && g_tagNameListNoCat.empty())
		{
			string notags =
			"<br><br><table width=\"98%\"><tr>"
				"<td width=\"99%\" align=\"left\"><font color=\"";
			notags.append(DARK() ? "#BEBEBE" : "#FFFFFF");
			notags.append(
				"\"><b>" $mid("No tags defined") ".</b></font></td>"
				"<td width=\"1%\"></td>"
			"</tr></table><br><br>");

			html.append(notags);
		}

		html.append(footer);

		return html;
	}
};


void Fl_FM_TagFilter_Button::OnDelete()
{
	((Fl_FM_TagFilter_Group*)parent())->OnTagFilterRemove( label() );
}


//
// Fl_FM_Tag_Group
//

class Fl_FM_Tag_Group : public Fl_FM_TagFilter_Group
{
protected:
	FMEntry *m_pFM;
	vector<string> m_taglist;

protected:
	virtual BOOL IsTagAdded(const char *tag) const
	{
		for (int i=0; i<(int)m_taglist.size(); i++)
			if ( !fl_utf_strcasecmp(m_taglist[i].c_str(), tag) )
				return TRUE;
		return FALSE;
	}

	virtual BOOL HasTags() const
	{
		return !m_taglist.empty();
	}

	virtual void OnClickLink(const char *link)
	{
		if ( !strcmp(link, "*") )
			// special reset "command"
			m_taglist.clear();
		else if ( strstr(link, ":*") )
		{
			// put category in input field (like tag presets does)

			const char *s = strstr(link, ":*");
			char buf[256];
			int n = (int)(s-link+1);
			memcpy(buf, link, n);
			buf[n] = 0;

			SetAddTagInputText(buf);
		}
		else
			m_taglist.push_back(link);

		OnFilterChanged();
	}

	virtual int handle(int e)
	{
		if ( !active() )
		{
			switch (e)
			{
			case FL_KEYBOARD:
			case FL_MOVE:
			case FL_DRAG:
			case FL_PUSH:
			case FL_RELEASE:
				return 0;
			}
		}

		return Fl_FM_TagFilter_Group::handle(e);
	}

public:
	Fl_FM_Tag_Group(int X, int Y, int W, int H,const char *l=0)
		: Fl_FM_TagFilter_Group(X,Y,W,H,l)
	{
		m_bTagEdit = TRUE;
		m_pFM = NULL;
	}

	void init(FMEntry *fm)
	{
		ASSERT(fm != NULL);
		m_pFM = fm;

		// make copy of tag list (so edit can be cancelled)
		m_taglist.clear();
		m_taglist.reserve( m_pFM->taglist.size() );
		for (int i=0; i<(int)m_pFM->taglist.size(); i++)
			m_taglist.push_back(m_pFM->taglist[i]);

		Fl_FM_TagFilter_Group::init();

		OnFilterChanged();
	}

	virtual void OnTagFilterRemove(const char *tag)
	{
		ASSERT(tag != NULL);

		for (int i=0; i<(int)m_taglist.size(); i++)
			if ( !fl_utf_strcasecmp(m_taglist[i].c_str(), tag) )
			{
				m_taglist.erase(m_taglist.begin() + i);
				OnFilterChanged();
				break;
			}
	}

	virtual void OnFilterChanged()
	{
		// delete any existing tag buttons (the first button is our Add button and second the ellipsis label)
		while (children() > 2)
		{
			Fl_Widget *o = child(2);
			remove(o);
			delete o;
		}

		const int maxw = w() - BORDER_WIDTH*2;

		// create all tag buttons (their placement will be updated after)
		for (int i=0; i<(int)m_taglist.size(); i++)
			insert(*new Fl_FM_TagFilter_Button(x(), y(), m_buttonheight, m_taglist[i].c_str(), 0, maxw), children());

		RearrangeButtons();
	}

	void add_custom_tag(const char *tag)
	{
		if ( IsTagAdded(tag) )
			return;

		m_taglist.push_back(tag);

		OnFilterChanged();
	}

	BOOL has_changes()
	{
		if (m_taglist.size() != m_pFM->taglist.size())
			return TRUE;

		for (int i=0; i<(int)m_pFM->taglist.size(); i++)
			if ( strcmp(m_pFM->taglist[i], m_taglist[i].c_str()) )
				return TRUE;

		return FALSE;
	}

	BOOL apply_changes()
	{
		if ( !has_changes() )
			return FALSE;

		string s;
		s.reserve(2048);
		for (int i=0; i<(int)m_taglist.size(); i++)
		{
			if (i)
				s.append(",");
			s.append( m_taglist[i].c_str() );
		}

		m_pFM->SetTags(s.empty() ? NULL : s.c_str());

		return TRUE;
	}
};


//
// FM Description Popup
//

class Fl_FM_Descr_Popup : public Fl_FM_Html_Popup
{
public:
	static FMEntry *s_pShowNext;
	static vector<string> s_infoList;

protected:
	virtual int handle(int e)
	{
		switch (e)
		{
		// workaround for some annoying issue probably because popup is opened in response to the context menu
		// which messes up some internal FLTK stuff so the html view never receives move message
		case FL_MOVE:
			if ( child(0)->handle(e) )
				return 1;
			break;

		case FL_KEYBOARD:
			switch ( Fl::event_key() )
			{
			case FL_Left:
				// go to previous FM in list
				{
				FMEntry *fm = pFMList->selected();
				int n = GetFilteredDbIndex(fm);
				if (n > 0)
				{
					s_pShowNext = g_dbFiltered[n-1];
					DeferredClose();
				}
				}
				return 1;
			case FL_Right:
				// go to next FM in list
				{
				FMEntry *fm = pFMList->selected();
				int n = GetFilteredDbIndex(fm);
				if (n < (int)g_dbFiltered.size()-1)
				{
					s_pShowNext = g_dbFiltered[n+1];
					DeferredClose();
				}
				}
				return 1;
			}
			break;
		}

		return Fl_FM_Html_Popup::handle(e);
	}

	static BOOL OnLink(Fl_Html_View *o, const char *url)
	{
		if ( _strnicmp(url, "http", 4) )
		{
			Fl_FM_Html_Popup *w = (Fl_FM_Html_Popup*)o->user_data2();
			w->ReturnLink(url);
			return TRUE;
		}

		OpenUrlWithAssociatedApp(url, g_cfg.browserApp.empty() ? NULL : g_cfg.browserApp.c_str());

		return FALSE;
	}

	static const char *link_cb2(Fl_Widget *o, const char *url)
	{
		if ( !strncmp(url, "/img/", 5) )
			// return raw filename (ie. image file url)
			return url+5;
		else if ( !strncmp(url, "/info/", 6) )
		{
			// view info file
			FMEntry *fm = pFMList->selected();
			ASSERT(fm != NULL);
			if (fm)
			{
				string &str = url[6] ? s_infoList[ atoi(url+6) ] : fm->infofile;

				ASSERT( !str.empty() );
				const char *filename = str.c_str();
				if ( !FmOpenFileWithAssociatedApp(fm, filename) )
					fl_alert($("Failed to view file \"%s\"."), filename);
			}
			return NULL;
		}
		else if ( !strncmp(url, "/tag/", 5) )
		{
			InvokeAddTagFilter(url+5);

			// close summary after adding filter
			Fl_FM_Html_Popup *w = (Fl_FM_Html_Popup*)((Fl_Html_View*)o)->user_data2();
			ASSERT(w != NULL);
			w->DeferredClose();

			return NULL;
		}

		return link_cb(o, url);
	}

	static Fl_Image* native_image_cb(const char *name)
	{
		if ( !strcmp(name, "star") )
			return &imgStar;
		else if ( !strcmp(name, "starhalf") )
			return &imgStarHalf;
		else if ( !strcmp(name, "stargray") )
			return &imgStarGray;
		else if ( !strcmp(name, "completed") )
			return &imgCheck;
		else if ( !strcmp(name, "inprogress") )
			return &imgProgress;

		ASSERT(FALSE);
		return NULL;
	}

public:
	Fl_FM_Descr_Popup(int X, int Y, int W, int H)
		: Fl_FM_Html_Popup(X,Y,W,H)
	{
	}

	static const char* popup(Fl_Window *parent, int W, int H, const char *html)
	{
		if (!html)
		{
			ASSERT(FALSE);
			return NULL;
		}

		int X = parent->x();
		int Y = parent->y();

		Fl_FM_Descr_Popup w(X,Y,W,H);

		w.callback((Fl_Callback*)OnLink);

		Fl_Html_View *o = new Fl_Html_View(0, 0, w.w(), w.h());
		w.resizable(o);

		o->user_data2(&w);
		o->box(HTML_POPUP_BOX);
		o->color((Fl_Color)(USER_CLR+16));
		//o->textfont(FL_HELVETICA_BOLD);
		o->textfont(FL_HELVETICA);
		o->textsize(FL_NORMAL_SIZE);
		o->textcolor(DARK() ? FL_GRAY0 : FL_BACKGROUND2_COLOR);
		o->link(link_cb2);
		o->image_callback(native_image_cb);
		o->include_anchor_to_cb();

		o->value(html);

		// shrink height to fit document if document is smaller
		if (o->size() < o->h() - Fl::box_dh( o->box() ))
		{
			// bypass resize of Fl_Html_View to avoid a complete re-parse for the html code
			o->Fl_Widget::resize(o->x(), o->y(), o->w(), o->size());
			w.size(w.w(), o->h());
		}

		// center to parent window
		w.position(
			parent->x() + ((parent->w() - w.w()) / 2),
			parent->y() + ((parent->h() - w.h()) / 2)
			);

		w.add(o);

		w.do_popup();

		return w.m_bClickedLink ? s_retlink : NULL;
	}
};


FMEntry *Fl_FM_Descr_Popup::s_pShowNext = NULL;
vector<string> Fl_FM_Descr_Popup::s_infoList;


/*
static void replace_all(string &s, const string &sFind, const string &sReplace)
{
	size_t start_pos = 0;
	while ((start_pos = s.find(sFind, start_pos)) != string::npos)
	{
		s.replace(start_pos, sFind.length(), sReplace);
		start_pos += sReplace.length();
	}
}
*/

// convert a string to HTML (with automatic hyperlinks)
static string TextToHtml(const char *text)
{
	const char *s = text;
	const int len = strlen(text);
	const char *s_end = text + len;

	string d;
	d.reserve(len+256);

	while (s < s_end)
	{
		if (*s == '\n')
			d += "<br>";
		else if (*s == '\r')
			/*skip*/;
		else if (*s == '\t')
			d += '\t';// TODO: what should we do with tabs?
		else if ( !_strnicmp(s, "http://", 7) || !_strnicmp(s, "https://", 8) )
		{
			const char *url = s;

			// find end of link
			s += 7;
			while (*s && !isspace_(*s))
				s++;
			// strip any trailing punctuations from url
			if (s[-1] == '.' || s[-1] == ',' || s[-1] == ':' || s[-1] == ';')
				s--;
			char ch = *s;
			*(char*)s = 0;

			d += "<a href=\"";
			d += url;
			d += "\"><b>";
			d += url;
			d += "</b></a>";

			*(char*)s = ch;
			s--;
		}
		// convert html special chars
		else if (*s == '<')
			d += "&lt;";
		else if (*s == '>')
			d += "&gt;";
		else if (*s == '&')
			d += "&amp;";
		else if (*s == '\"')
			d += "&quot;";
		else
			d += *s;

		s++;
	}

	return d;
}

// convert an escaped string to HTML (with automatic hyperlinks)
static string EscapedTextToHtml(const char *text)
{
	string rawtext = UnescapeString(text);

	const char *s = rawtext.c_str();
	const int len = (int) rawtext.size();
	const char *s_end = rawtext.c_str() + len;

	// strip trailing whitespaces
	while (s_end > s && isspace_(s_end[-1]) )
		s_end--;
	if (s_end == s && isspace_(*s))
		return "";

	char ch = *s_end;
	*(char*)s_end = 0;

	string d = TextToHtml(s);

	*(char*)s_end = ch;

	return d;
}

static string GenerateHtmlSummary(FMEntry *fm)
{
	string html;
	char buff[1024];
	int i;

	#define LINK_CLR		"#FFCC30"
	#define LINK_CLR_DARK		"#BD8C00"
	#define CAT_LABEL_CLR		"#C8E6FA"
	#define CAT_LABEL_CLR_DARK	"#89A6B9"

	const char *preheader =
		"<html><body bgcolor=\"%s\" link=\"%s\">";

	const char *header =
		"<table width=\"98%%\"><tr bgcolor=\"%s\">"
			"<td width=\"89%%\"><font color=\"%s\"><b>%s</b></font></td>"
			"<td width=\"10%%\" align=\"right\"><b><a href=\"/*\">%s</a></b></td>"
			"<td width=\"1%%\"></td>"
		"</tr></table><br>";

	const char *footer =
		"</body></html>";

	if (DARK())
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, preheader, "#003E75", LINK_CLR_DARK);
	else
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, preheader, "#3C78B4", LINK_CLR);

	html.append(buff);

#if 0
	html.append("<br>");
#else
	const char *pager =
		"<center><font face=\"courier\" size=\"2.7\" color=\"%s\">&lt;</font>"
		"<font face=\"courier\" size=\"2.7\" color=\"%s\"> | </font>"
		"<font face=\"courier\" size=\"2.7\" color=\"%s\">&gt;</font></center>";

	const int curindex = GetFilteredDbIndex(fm);
	const BOOL hasprev = (curindex > 0);
	const BOOL hasnext = (curindex < (int)g_dbFiltered.size()-1);

	if (DARK())
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, pager, hasprev ? CAT_LABEL_CLR_DARK : "#035493", "#035493", hasnext ? CAT_LABEL_CLR_DARK : "#035493");
	else
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, pager, hasprev ? CAT_LABEL_CLR : "#4C90D4", "#4C90D4", hasnext ? CAT_LABEL_CLR : "#4C90D4");
	html.append(buff);
#endif

	_snprintf_s(buff, sizeof(buff), _TRUNCATE, header, DARK() ? "#035493" : "#4C90D4", DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR, fm->GetFriendlyName(), $("Close"));
	html.append(buff);

	html.append("<table width=\"98%\"><tr><td>");

	//

	if ( FmFileExists(fm, "fmthumb.jpg") )
	{
		if ( !fm->IsInstalled() )
		{
			// get fmthumb.jpg from archive
			string imgfile;
			if ( FmGetPhysicalFileFromArchive(fm, "fmthumb.jpg", imgfile, TRUE) )
			{
				const char *img = "<center><img src=\"/img/%s\" /></center><br><br>";

				_snprintf_s(buff, sizeof(buff), _TRUNCATE, img, imgfile.c_str());

				html.append(buff);
			}
		}
		else
		{
			const char *img = "<center><img src=\"/img/%s" DIRSEP "%s" DIRSEP "%s\" /></center><br><br>";

			_snprintf_s(buff, sizeof(buff), _TRUNCATE, img, g_pFMSelData->sRootPath, fm->name, "fmthumb.jpg");

			html.append(buff);
		}
	}

	// release date
	if (fm->tmReleaseDate)
	{
		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Release Date"));
		html.append(":</b></u></font> ");

		FormatDbDate(fm->tmReleaseDate, buff, sizeof(buff), FALSE);
		html.append( TextToHtml(buff) );

		html.append("<br><br>");
	}

	// rating
	if (fm->rating >= 0)
	{
		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Rating"));
		html.append(":</b></u></font> ");

		for (i=0; i<fm->rating>>1; i++)
			html.append("<img src=\"imgres:star\" />");
		if (fm->rating & 1)
		{
			html.append("<img src=\"imgres:starhalf\" />");
			i++;
		}
		for (; i<5; i++)
			html.append("<img src=\"imgres:stargray\" />");

		html.append("<br><br>");
	}

	BOOL bExtraSpace = FALSE;

	// completed
	if (fm->nCompleteCount > 0)
	{
		const char *c = "<font color=\"%s\"><u><b>%s:</b></u></font> %d %s%s";

		char buff2[256];
		buff2[0] = 0;
		if (fm->tmLastCompleted)
		{
			char date[128];
			FormatDbDate(fm->tmLastCompleted, date, sizeof(date), TRUE);
			if (fm->nCompleteCount > 1)
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, " (<u>%s</u>: %s)", $("Last"), TextToHtml(date).c_str());
			else
				_snprintf_s(buff2, sizeof(buff2), _TRUNCATE, " (<u>%s</u>: %s)", $("On"), TextToHtml(date).c_str());
		}

		_snprintf_s(buff, sizeof(buff), _TRUNCATE, c, $("Completed"), DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR,
			fm->nCompleteCount, fm->nCompleteCount>1 ? $("times") : $("time"), buff2);
		html.append(buff);

		html.append("<br>");

		bExtraSpace = TRUE;
	}

	// last played
	if (fm->tmLastStarted)
	{
		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Last Played"));
		html.append(":</b></u></font> ");

		FormatDbDate(fm->tmLastStarted, buff, sizeof(buff), TRUE);
		html.append( TextToHtml(buff) );

		html.append("<br>");

		bExtraSpace = TRUE;
	}

	if (bExtraSpace)
		html.append("<br>");

	// info file
	vector<string> &list = Fl_FM_Descr_Popup::s_infoList;
	GetDocFiles(fm, list);

	if (!fm->infofile.empty() || list.size())
	{
		if (list.size() > 1
			|| (list.size() == 1 && !fm->infofile.empty() && _stricmp(fm->infofile.c_str(), list[0].c_str())))
		{
			// allthough it shouldn't happen, make it foolproof and handle case where fm->infofile is not in list
			if ( !fm->infofile.empty() )
			{
				html.append("<font color=\"");
				html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
				html.append("\"><u><b>");
				html.append($("Info Files"));
				html.append(":</b></u></font> <a href=\"/info/\"><b>");
				html.append( TextToHtml( fm->infofile.c_str() ) );
				html.append("</b></a>");
			}
			else
			{
				html.append("<font color=\"");
				html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
				html.append("\"><u><b>");
				html.append($("Info Files"));
				html.append(": ");
			}

			for (i=0; i<(int)list.size(); i++)
				if ( _stricmp(fm->infofile.c_str(), list[i].c_str()) )
				{
					if ( !fm->infofile.empty() )
						html.append(", ");

					_snprintf_s(buff, sizeof(buff), _TRUNCATE, "<a href=\"/info/%d\">%s</a>", i, TextToHtml( list[i].c_str() ).c_str());
					html.append(buff);
				}
		}
		else
		{
			html.append("<font color=\"");
			html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
			html.append("\"><u><b>");
			html.append($("Info File"));
			html.append(":</b></u></font> <a href=\"/info/\"><b>");
			html.append( TextToHtml( fm->infofile.c_str() ) );
			html.append("</b></a>");
		}

		html.append("<br><br>");
	}

	const char *section_header = DARK() ? "</td></tr><tr bgcolor=\"#044679\"><td>" : "</td></tr><tr bgcolor=\"#4780B8\"><td>";

	const char *section_footer =
		"</td></tr>"
		"<tr><td height=\"14\"></td></tr>"
		"<tr><td>";

	bExtraSpace = FALSE;

	// description
	if ( !fm->descr.empty() )
	{
		if (!bExtraSpace)
		{
			html.append("<br>");
			bExtraSpace = TRUE;
		}

		html.append(section_header);

		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Description"));
		html.append(":</b></u></font><br>");
		html.append( EscapedTextToHtml( fm->descr.c_str() ) );
		//html.append("<br><br>");

		html.append(section_footer);
	}

	// tags
	if ( !fm->taglist.empty() )
	{
		if (!bExtraSpace)
		{
			html.append("<br>");
			bExtraSpace = TRUE;
		}

		html.append(section_header);

		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Tags"));
		html.append(":</b></u></font><br>");

		string s;
		for (i=0; i<(int)fm->taglist.size(); i++)
		{
			if (i)
				s += ", ";
			if ( IsTagInFilterList(fm->taglist[i]) )
			{
				// already added to filters
				s += "<u>";
				s += TextToHtml(fm->taglist[i]);
				s += "</u>";
			}
			else
			{
				// not added to filter, make a link
				s += "<a href=\"/tag/";
				s += fm->taglist[i];
				s += "\">";
				s += TextToHtml(fm->taglist[i]);
				s += "</a>";
			}
		}
		html.append(s);

		//html.append("<br><br>");

		html.append(section_footer);
	}

	// notes
	if ( !fm->notes.empty() )
	{
		if (!bExtraSpace)
		{
			html.append("<br>");
			bExtraSpace = TRUE;
		}

		html.append(section_header);

		html.append("<font color=\"");
		html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
		html.append("\"><u><b>");
		html.append($("Notes"));
		html.append(":</b></u></font><br>");
		html.append( EscapedTextToHtml( fm->notes.c_str() ) );
		//html.append("<br><br>");

		html.append(section_footer);
	}

	//

	html.append("</td></tr></table>");

	html.append("<table width=\"98%\"><tr bgcolor=\"");
	html.append(DARK() ? "#035493" : "#4C90D4");
	html.append(
		"\"><td></td>"
		"</tr></table><br>"
		);

	html.append(footer);

	return html;
}

static void ViewSummary(FMEntry *fm)
{
	int X = 0;
	int Y = 0;
	int W = pMainWnd->w() - 40;
	int H = pMainWnd->h() - 40;

	const int MAX_W = SCALECFG((g_cfg.bLargeFont ? 770 : 630));
	const int MAX_H = SCALECFG((g_cfg.bLargeFont ? 980 : 800));

	if (W >= MAX_W)
		W = MAX_W;
	if (H >= MAX_H)
		H = MAX_H;

	Fl_FM_Descr_Popup::s_pShowNext = NULL;

show_popup:

	string html = GenerateHtmlSummary(fm);

	if ( html.empty() )
		return;

	Fl_FM_Descr_Popup::popup(pMainWnd, W, H, html.c_str());

	// clear doc list
	Fl_FM_Descr_Popup::s_infoList.clear();
	Fl_FM_Descr_Popup::s_infoList.reserve(0);

	if (Fl_FM_Descr_Popup::s_pShowNext)
	{
		pFMList->select(Fl_FM_Descr_Popup::s_pShowNext);
		Fl_FM_Descr_Popup::s_pShowNext = NULL;

		FMEntry *p = pFMList->selected();
		if (p && p != fm)
		{
			fm = p;

			pFMList->select(fm);

			AutoSelectInfoFile(fm);

			goto show_popup;
		}
	}
}

// display a generic html popup where inside a standard format, 'html_body' contains html code for the "interior"
// returns url of any clicked link contained in the supplied body
static const char* GenericHtmlTextPopup(const char *caption, const char *html_body, int W, int maxH)
{
	if (!html_body)
		return NULL;

	int X = 0;
	int Y = 0;
	int H = pMainWnd->h() - 40;

	const int MAX_H = maxH;

	if (H >= MAX_H)
		H = MAX_H;

	string html;
	char buff[256];

	//

	const char *header_part1 =
		"<html><body bgcolor=\"%s\" link=\"%s\">"
		"<br><table width=\"98%%\"><tr bgcolor=\"%s\">"
		"<td width=\"89%%\"><font color=\"%s\"><b>";
	string header_part2 =
		"</b></font></td>"
			"<td width=\"10%\" align=\"right\"><b><a href=\"/*\">" $mid("Close") "</a></b></td>"
			"<td width=\"1%\"></td>"
		"</tr></table>"
		"<table width=\"98%\"><tr><td>";

	const char *footer =
		"<br><br>"
		"</td></tr></table>"
		"<table width=\"98%%\"><tr bgcolor=\"%s\">"
			"<td></td>"
		"</tr></table><br>"
		"</body></html>";

	if (DARK())
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, header_part1, "#003E75", LINK_CLR_DARK, "#035493", "#BEBEBE");
	else
		_snprintf_s(buff, sizeof(buff), _TRUNCATE, header_part1, "#3C78B4", LINK_CLR, "#4C90D4", "#FFFFFF");
	html.append(buff);
	html.append( TextToHtml(caption ? caption : "") );
	html.append(header_part2);

	html.append(html_body);

	_snprintf_s(buff, sizeof(buff), _TRUNCATE, footer, DARK() ? "#035493" : "#4C90D4");
	html.append(buff);

	//

	return Fl_FM_Descr_Popup::popup(pMainWnd, W, H, html.c_str());
}

// display about box (uses html popup)
static void ViewAbout()
{
	string html;

	html.append("<h1><font color=\"");
	html.append(DARK() ? CAT_LABEL_CLR_DARK : CAT_LABEL_CLR);
	html.append("\">FMSel " FMSEL_VERSION "</font></h1><br>");

	// about text doesn't (only supports localization of vital information)
	char msg[3072];
	sprintf(msg,
		"%s<br>" // $("FM selector and manager for dark engine games.")
		"Licensed under the FLTK license.<br>"
		"<br>"

		"<center><font color=\"%s\"><hr></font></center>"

		"%s <b>" MP3LIB "</b> (x86 / 32-bit) %s " FMSELLIB ".<br>" // $("To enable support for MP3 to WAV conversion during FM install, you must download"), $("and put it in the same directory as")
		"<br>"
		"%s<br>" // $("Links for binary downloads of the LAME MP3 library can be found on:")
		"<a href=\"https://lame.sourceforge.net/links.php#Binaries\"><b>https://lame.sourceforge.net/links.php#Binaries</b></a><br>"
		"<br>"
		"%s<br>" // $("Direct link to the recommended site on that list:")
#ifdef _WIN32
		"<a href=\"https://www.rarewares.org/mp3-lame-libraries.php\"><b>https://www.rarewares.org/mp3-lame-libraries.php</b></a><br>"
		"<br>"
#endif

		"<center><font color=\"%s\"><hr></font></center>"
		"This program uses:<br>"

		"</td></tr></table>"
		"<table width=\"98%%\"><tr bgcolor=\"%s\"><tr><td width=\"5%%\"></td><td width=\"95%%\">"

		"<b>fltk v%d.%d.%d</b><br>"
		"<a href=\"https://www.fltk.org\"><b>https://www.fltk.org</b></a>"
		"<br><br>"
		"<b>" LIB_7ZIP_7ZIP_VERSION "</b><br>"
		"<a href=\"https://code.google.com/archive/p/lib7zip/\"><b>https://code.google.com/archive/p/lib7zip/</b></a>"
		"<br><br>"
		"<b>7-Zip dynamic library</b> (which is licensed under GNU LGPL)<br>"
		"<a href=\"https://7-zip.org\"><b>https://7-zip.org</b></a>"
#ifdef OGG_SUPPORT
		"<br><br>"
		"<b>%s</b>, Copyright (c) 2002-2008 Xiph.org Foundation<br>"
		"<a href=\"https://xiph.org/vorbis/\"><b>https://xiph.org/vorbis/</b></a>"
#endif

		, $("FM selector and manager for dark engine games.")
		, DARK() ? "#376189" : "#719DC8"
		, $("To enable support for MP3 to WAV conversion during FM install, you must download")
		, $("and put it in the same directory as")
		, $("Links for binary downloads of the LAME MP3 library can be found on:")
#ifdef _WIN32
		, $("Direct link to the recommended site on that list:")
#else
		, $("It is recommended to use your operating system's package manager to acquire compatible binaries if at all possible.")
#endif
		, DARK() ? "#376189" : "#719DC8"
		, DARK() ? "#035493" : "#4C90D4"
		, FL_MAJOR_VERSION, FL_MINOR_VERSION, FL_PATCH_VERSION
#ifdef OGG_SUPPORT
		, GetOggVersion()
#endif
		);

	html.append(msg);

	GenericHtmlTextPopup($("About"), html.c_str(), SCALECFG((g_cfg.bLargeFont ? 610 : 500)), SCALECFG((g_cfg.bLargeFont ? 610 : 500)));
}


//
// Menus for combo boxes
//

// NOTE: don't forget to call InitMenuDef (in InitFLTK) when adding new menu defs here, and use font size 12
//       so that the UI font setting applies correctly

static Fl_Menu_Item g_mnuFiltRating[] =
{
	{ "*", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "0", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "0.5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "1", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "1.5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "2", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "2.5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "3", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "3.5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "4", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "4.5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "5", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ 0 }
};

static Fl_Menu_Item g_mnuFiltPrio[] =
{
	{ "*", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ 0 }
};

#ifdef LOCALIZATION_SUPPORT
static char TAGPRESET_CAT_CUST[64] = "<custom>";
#else
#define TAGPRESET_CAT_CUST "<custom>"
#endif

// NOTE: Don't localize any standard tag names because that would not make them standard anymore, which only hurts the
//       ability to shared tag databases and fm.ini functionality would also suffer greatly. A user can still add their
//       own tags in any language they want but the FMSel defined standard tags should strive for a single global standard.
static Fl_Menu_Item g_mnuTagPresets[] =
{
	{ "author:", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "contest:", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "genre:", 0, NULL, NULL, FL_SUBMENU, 0, FL_HELVETICA, 12 },
		{ TAGPRESET_CAT_CUST, 0, NULL, (void*)"genre:", FL_MENU_DIVIDER, 0, FL_HELVETICA, 12 },
		{ "action", 0, NULL, (void*)"genre:", 0, 0, FL_HELVETICA, 12 },
		{ "crime", 0, NULL, (void*)"genre:", 0, 0, FL_HELVETICA, 12 },
		{ "horror", 0, NULL, (void*)"genre:", 0, 0, FL_HELVETICA, 12 },
		{ "mystery", 0, NULL, (void*)"genre:", 0, 0, FL_HELVETICA, 12 },
		{ "puzzle", 0, NULL, (void*)"genre:", 0, 0, FL_HELVETICA, 12 },
		{ 0 },
	{ "language:", 0, NULL, NULL, FL_SUBMENU, 0, FL_HELVETICA, 12 },
		{ TAGPRESET_CAT_CUST, 0, NULL, (void*)"language:", FL_MENU_DIVIDER, 0, FL_HELVETICA, 12 },
		{ "English", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Czech", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Dutch", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "French", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "German", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Hungarian", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Italian", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Japanese", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Polish", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Russian", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ "Spanish", 0, NULL, (void*)"language:", 0, 0, FL_HELVETICA, 12 },
		{ 0 },
	{ "series:", 0, NULL, NULL, FL_MENU_DIVIDER, 0, FL_HELVETICA, 12 },
	//{ "setting:", 0, NULL, NULL, FL_MENU_DIVIDER, 0, FL_HELVETICA, 12 },

	{ "campaign", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "demo", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "long", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "other protagonist", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "short", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ "unknown author", 0, NULL, NULL, 0, 0, FL_HELVETICA, 12 },
	{ 0 }
};

#ifdef LOCALIZATION_SUPPORT
// however we can display localized tags, as long as long as they're kept in english internally and the db
// this is a 1:1 array to g_mnuTagPresets containing the original untranslated menu item names
static const char* g_tagPresetsEnglish[sizeof(g_mnuTagPresets)/sizeof(g_mnuTagPresets[0])] = {0};
// just an array to keep extra allocated strings for localized categories (used to free mem)
static const char* g_tagPresetsExtraAllocated[sizeof(g_mnuTagPresets)/sizeof(g_mnuTagPresets[0])] = {0};
#endif

static Fl_Menu_Item g_mnuTagPresetsLabel[] =
{
	{ "<Tag Presets>", 0, NULL, NULL, 0, 0, FL_HELVETICA_BOLD, 11 },
	{ 0 }
};


/////////////////////////////////////////////////////////////////////
// FLUID autogenerated code

#include "fl_mainwnd.cpp.h"


/////////////////////////////////////////////////////////////////////
// CONFIG ARCHIVE PATH

static BOOL IsArchivePathSameAsFmPath(const char *archpath)
{
	char fmpath[MAX_PATH_BUF];
	char absarchpath[MAX_PATH_BUF];

	fl_filename_absolute_ex(absarchpath, sizeof(absarchpath), archpath, TRUE);
	CleanDirSlashes(absarchpath);
	string sArchPath = absarchpath;
	TrimTrailingSlashFromPath(sArchPath);

	fl_filename_absolute_ex(fmpath, sizeof(fmpath), g_pFMSelData->sRootPath, TRUE);
	CleanDirSlashes(fmpath);
	string sFmPath = fmpath;
	TrimTrailingSlashFromPath(sFmPath);

	if ( !_stricmp(sFmPath.c_str(), sArchPath.c_str()) )
		return TRUE;

	// check that the FM path isn't a subdir of the archive path and vice versa
	if (sFmPath.length() < sArchPath.length())
	{
		// archive path might be inside the FM path
		// C:\thief\fmpath
		// C:\thief\fmpath\archives
		// allow special case where path name starts with . those dirs aren't scanned by ScanFmDir
		// C:\thief\fmpath\.archives
		string s = sFmPath + DIRSEP;
		if (!_strnicmp(sArchPath.c_str(), s.c_str(), s.length()) && sArchPath[s.length()] != '.')
			return TRUE;
	}
	else if (sArchPath.length() < sFmPath.length())
	{
		// FM path might be inside the archive path
		// C:\thief\archives
		// C:\thief\archives\fmpath
		string s = sArchPath + DIRSEP;
		if ( !_strnicmp(sFmPath.c_str(), s.c_str(), s.length()) )
			return TRUE;
	}

	return FALSE;
}

static void ConfigArchivePath(BOOL bStartupConfig)
{
retry:
	char fname[MAX_PATH_BUF];
	if (g_cfg.bRepoOK)
		_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "anyfile", g_cfg.archiveRepo.c_str());
	else
		_snprintf_s(fname, sizeof(fname), _TRUNCATE, "%s" DIRSEP "anyfile", g_pFMSelData->sRootPath);

	const char *pattern[] =
	{
		$("Archive Files"), "*.zip;*.rar;*.7z;*.ss2mod",
		$("All Files"), "*.*",
		NULL,
	};

	if ( !FileDialog(pMainWnd, FALSE, $("Set FM Archive Library Path (select any existing or non-existing file inside it)"), pattern, NULL, fname, fname, sizeof(fname), TRUE) )
		return;

	// strip filename from path
	char *s = fname + strlen(fname) - 1;
	while (s > fname && !isdirsep(*s)) s--;
	*s = 0;

	CleanDirSlashes(fname);

	string str = fname;

	TrimTrailingSlashFromPath(str);

	if ( str.empty() )
	{
		fl_alert($("The root dir is not a valid archive path."));
		return;
	}

	// make sure the archive path isn't the FM path
	if ( IsArchivePathSameAsFmPath( str.c_str() ) )
	{
		fl_alert($("The archive path may not be inside the FM path, or vice versa."));
		goto retry;
	}

	if (g_cfg.archiveRepo.empty() || g_cfg.archiveRepo != str)
	{
		if (g_cfg.bRepoOK && !fl_choice($("You will lose access to all archives in the current path.\nAre you sure you want to change archive path?"), fl_cancel, fl_ok, NULL))
			return;

		// to help avoid confusion for first time users, make sure archived FMs are visible after the archive path
		// is set for the first time
		if ( g_cfg.archiveRepo.empty() )
			g_cfg.filtShow |= FSHOW_ArchivedOnly;

		g_cfg.archiveRepo = str;
		g_cfg.OnModified();

		if (!bStartupConfig)
		{
			fl_message($("Changing the archive path requires a restart. FMSel will now exit."));

			OnExit(NULL, NULL);
		}
	}
}


/////////////////////////////////////////////////////////////////////
// IMPORT BATCH FM.INI DIALOG

static int DoImportBatchFmIniDialog()
{
	Fl_Window *w = new Fl_Double_Window(280, 320, $("Import Batch FM.INI"));
	ASSERT(w != NULL);
	if (!w)
		return 0;

	w->set_modal();
	w->box(FL_FLAT_BOX);
	w->resizable(NULL);

	int mode = 0;
	int i;

	// NOTE: if "Tags" gets a different array index than 4 then update index
	const int TAGS_IND = 4;
	// NOTE: if "Fill in missing and add tags" gets a different array index than 1 then update index
	const int FILL_ADDTAG_IND = 1;

	const char *typenames[] =
	{
		$("Name"),
		$("Release Date"),
		$("Description"),
		$("Info File"),
		$("Tags")
	};
	const int num_types = sizeof(typenames)/sizeof(typenames[0]);

	const char *modenames[] =
	{
		$("Fill in missing data"),
		$("Fill in missing but merge tags"),
		$("Overwrite existing data")
	};
	const char *modename_ttips[] =
	{
		$("Only import data that is missing/empty (nothing is overwritten)"),
		$("Only import data that is missing/empty, except tags which are added/merged into already existing tags (nothing is overwritten)"),
		$("Imported data replaces existing data")
	};
	const int num_modes = sizeof(modenames)/sizeof(modenames[0]);

	int X = 10;
	int Y = 10;
	int W = 130;
	int H = FL_NORMAL_SIZE + 6;

	fl_font(FL_HELVETICA, FL_NORMAL_SIZE);

	//

	Fl_Box *l = new Fl_Box(X, Y, W, H, $("Data to import:"));
	l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	l->box(FL_NO_BOX);
	Y += H + 4;

	Fl_Check_Button *typecheck[num_types];
	for (i=0; i<num_types; i++)
	{
		int ww = 0, foo = 0;
		fl_measure(typenames[i], ww, foo);
		ww += H + 8;

		Fl_Check_Button *o = typecheck[i] = new Fl_Check_Button(X + 32, Y, ww, H, typenames[i]);
		o->value(1);

		Y += H + 2;
	}

	//

	Y += 8;
	l = new Fl_Box(X, Y, W, H, $("Import mode:"));
	l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
	l->box(FL_NO_BOX);
	Y += H + 4;

	Fl_Round_Button *modecheck[num_modes];
	for (i=0; i<num_modes; i++)
	{
		int ww = 0, foo = 0;
		fl_measure(modenames[i], ww, foo);
		ww += H + 8;

		Fl_Round_Button *o = modecheck[i] = new Fl_Round_Button(X + 32, Y, ww, H, modenames[i]);
		o->tooltip(modename_ttips[i]);
		o->type(FL_RADIO_BUTTON);
		o->clear();

		Y += H + 2;
	}

	modecheck[0]->setonly();

	//

	Fl_Check_Button *filteredonly;
	{
	Y += 8;
	const char *label = $("Only apply to FMs visible in the list");
	int ww = 0, foo = 0;
	fl_measure(label, ww, foo);
	ww += H + 8;

	filteredonly = new Fl_Check_Button(X, Y, ww, H, label);
	filteredonly->tooltip($("Only apply imported data if it is for an FM currently visible in the list"));
	}

	//

	W = 80;
	H = 25;
	X = w->w() - W - 10;
	Y = w->h() - H - 10;

	Fl_Button *cancel = new Fl_Button(X, Y, W, H, fl_cancel);
	Fl_Button *ok = new Fl_Button(X - W - 16, Y, W, H, fl_ok);

	w->end();

	w->position(
		pMainWnd->x() + ((pMainWnd->w() - w->w()) / 2),
		pMainWnd->y() + ((pMainWnd->h() - w->h()) / 2)
		);

	w->show();

	BOOL bOK = FALSE;

	while ( w->visible() )
	{
		Fl_Widget *o = Fl::readqueue();

		if (o == cancel) w->hide();
		else if (o == ok)
		{
			int val = 0;
			for (i=0; i<num_types; i++)
				if ( typecheck[i]->value() )
					val |= 1 << i;

			if (val)
			{
				bOK = TRUE;
				w->hide();
			}
			else
				fl_alert($("No data selected for import."));
		}
		else if (o == typecheck[TAGS_IND])
		{
			if ( typecheck[TAGS_IND]->value() )
				modecheck[FILL_ADDTAG_IND]->activate();
			else
			{
				if ( modecheck[FILL_ADDTAG_IND]->value() )
					modecheck[0]->setonly();
				modecheck[FILL_ADDTAG_IND]->deactivate();
			}
		}

#ifdef _WIN32
		if (!o) Fl::wait();
#else
		if (!o) Fl::wait(0);
#endif
	}

	if (bOK)
	{
		for (i=0; i<num_types; i++)
			if ( typecheck[i]->value() )
				mode |= 1 << i;

		for (i=0; i<num_modes; i++)
			if ( modecheck[i]->value() )
				mode |= 1 << (i+16);

		if ( filteredonly->value() )
			mode |= IMP_ModeFiltOny;
	}

	delete (Fl_Widget*)w;

	if (!bOK)
		return 0;

	//

	if (mode & IMP_ModeOverwrite)
	{
		if ( !fl_choice($("Are you sure you want to overwrite existing data?"), fl_no, fl_yes, NULL) )
			return 0;
	}
	else if ((mode & IMP_ModeFillAddTags) && (mode & IMP_Tags))
	{
		if ( !fl_choice($("Are you sure you want add any imported tags to existing ones?"), fl_no, fl_yes, NULL) )
			return 0;
	}

	return mode;
}


/////////////////////////////////////////////////////////////////////
// PROGRESS WINDOW

class Fl_FM_Progress_Window : public Fl_Double_Window
{
public:
	Fl_FM_Progress_Window(int W, int H, const char *l=0)
		: Fl_Double_Window(W,H,l) {}

	virtual int handle(int e)
	{
		if (e == FL_KEYBOARD)
			return 1;
		return Fl_Double_Window::handle(e);
	}
};


static Fl_Window *g_pProgressDlg = NULL;
static Fl_Progress *g_pProgressBar = NULL;
static volatile int g_nMaxSteps = 0;
static volatile int g_nLastProgress = 0;
static volatile int g_nCurProgress = 0;
static volatile int g_nProgressResult = 0;
static volatile BOOL g_nProgressDone;


// called from worker thread to increment step count
void StepProgress(int nSteps)
{
	if (g_pProgressDlg)
	{
		g_nCurProgress += nSteps;
		if (g_nCurProgress > g_nMaxSteps)
			g_nCurProgress = g_nMaxSteps;

		const int NUM_VISUAL_STEPS = 20;

		const int vistick0 = (NUM_VISUAL_STEPS * g_nLastProgress) / g_nMaxSteps;
		const int vistick1 = (NUM_VISUAL_STEPS * g_nCurProgress) / g_nMaxSteps;

		if (vistick0 != vistick1)
		{
			g_nLastProgress = g_nCurProgress;
			Fl::awake();
		}
	}
}

// called from worker thread to signal end of work (will make RunProgress(), called by main thread, return)
void EndProgress(int result)
{
	if (g_pProgressDlg)
	{
		g_nProgressResult = result;
		g_nProgressDone = TRUE;
		Fl::awake();
	}
}

// clean up progress window
void TermProgress()
{
	if (g_pProgressDlg)
	{
		g_pProgressDlg->hide();

		delete (Fl_Widget*)g_pProgressDlg;
		g_pProgressDlg = NULL;
		g_pProgressBar = NULL;

		// make sure main window is brought to front and activated again
		// (if the html popup was opened it can cause it to lose activation)
		pMainWnd->show();
	}
}

// run progress window (modal loop), returns value pased to EndProgress
int RunProgress()
{
	if (!g_pProgressDlg)
	{
		ASSERT(FALSE);
		return 0;
	}

	fl_cursor(FL_CURSOR_WAIT);

	while (g_pProgressDlg->visible() && !g_nProgressDone)
	{
		Fl_Widget *o = Fl::readqueue();

		if (g_nProgressDone)
		{
			g_pProgressDlg->hide();
			break;
		}
		else
			g_pProgressBar->value((float)g_nCurProgress);

#ifdef _WIN32
		if (!o) Fl::wait();
#else
		if (!o) Fl::wait(0);
#endif
	}

	TermProgress();

	return g_nProgressResult;
}

void InitProgress(int nSteps, const char *label)
{
	if (g_pProgressDlg || nSteps <= 0)
	{
		ASSERT(FALSE);
		return;
	}

	Fl_Window *w = g_pProgressDlg = new Fl_FM_Progress_Window(250, 40);
	ASSERT(w != NULL);
	if (!w)
		return;

	w->clear_border();
	w->set_modal();
	w->box(FL_UP_BOX);

	Fl_Progress *o = g_pProgressBar = new Fl_Progress(10, 10, w->w()-20, w->h()-20, label);
	o->labelfont(FL_HELVETICA_BOLD);
	o->labelsize(FL_NORMAL_SIZE);
	o->color((Fl_Color)(USER_CLR+24));
	o->color2((Fl_Color)(USER_CLR+25));

	w->end();

	g_nMaxSteps = nSteps;
	g_nCurProgress = g_nLastProgress = 0;
	g_nProgressDone = FALSE;

	g_pProgressBar->minimum(0);
	g_pProgressBar->maximum((float)nSteps);

	w->position(
		pMainWnd->x() + ((pMainWnd->w() - w->w()) / 2),
		pMainWnd->y() + ((pMainWnd->h() - w->h()) / 2)
		);

	w->show();
}


/////////////////////////////////////////////////////////////////////
// TAG EDITOR (turned into FM properties editor)

class Fl_FM_TagEd_Window : public Fl_Double_Window
{
public:
	Fl_FM_TagEd_Window(int W, int H, const char *l=0)
		: Fl_Double_Window(W,H,l) {}

	virtual ~Fl_FM_TagEd_Window()
	{
		pTagEdWnd = NULL;
	}

	virtual int handle(int e);
};


#include "fl_taged.cpp.h"


static FMEntry *g_pTagEdFM = NULL;

// set to TRUE when not really editing an existing FM, just a dummy used to create a standalon FM.INI
static BOOL g_bTagEdFakeFM = FALSE;

static BOOL g_bTagEdFakeFMForcedHasChanges = FALSE;
static Fl_Input *pTagEdFakeInfoFile = NULL;

static Fl_Menu_Item *g_mnuInfoFiles;
static vector<string> g_infoFiles;

static Fl_Text_Buffer *g_pNotesBuffer = NULL;
static Fl_Text_Buffer *g_pDescrBuffer = NULL;


int Fl_FM_TagEd_Window::handle(int e)
{
	int res = Fl_Double_Window::handle(e);
	if (pTagEdInput) pTagEdInput->check_listhide_event(e);
	return res;
}


static void OnTagEdAddTag(Fl_Button *o, void *p)
{
	Fl::focus(pTagEdInput);

	OnAddCustomTag(pTagEdInput, pTagEdInput->user_data());
}

static void OnAddCustomTag(Fl_FM_Tag_Input *o, void *p)
{
	const char *tag = o->value();
	if (!tag || !*tag)
		return;

	// trim left
	while ( isspace_(*tag) ) tag++;
	if (!*tag)
		return;

	char *s = _strdup(tag);

	// trim right
	TrimRight(s);

	string org = s;

	if ( !CleanTag(s, FALSE) )
	{
		free(s);
		return;
	}

#ifdef LOCALIZATION_SUPPORT
	if (localization_supported)
	{
		ASSERT(localization_initialized);

		// try to do a reverse lookup of entered string to see if there's a "tag_*" entry in the language db
		char *colon = strchr(s, ':');
		if (colon)
		{
			*colon = 0;
			string sCat = s, sTag = colon+1;
			*colon = ':';
			if (UnTranslateTag(sCat) || UnTranslateTag(sTag))
			{
				sTag = sCat + ":" + sTag;
				free(s);
				s = _strdup( sTag.c_str() );
			}
		}
		else
		{
			string sTag = s;
			if ( UnTranslateTag(sTag) )
			{
				free(s);
				s = _strdup( sTag.c_str() );
			}
		}
	}
#endif

	// if CleanTag had to modify the string (or changes due to localization)
	// then just put the modified string in the input control and select all
	if (org != s)
	{
		o->value(s);
		o->position(o->size(), 0);
	}
	else
	{
		pTagEdGroup->add_custom_tag(s);
		o->value("");

		pTagEdAddTag->deactivate();
	}

	free(s);
}

static void DelayedCompletionList(void *p)
{
	if (pTagEdWnd && pTagEdWnd->visible())
		pTagEdInput->OnChange();
}

static void SetAddTagInputText(const char *s)
{
	pTagEdInput->value(s);
	Fl::focus(pTagEdInput);
	pTagEdInput->position( strlen(s) );

	pTagEdAddTag->activate();

	// can't call OnChange directly because we're inside a FL_PUSH event that will hide it again
	if ( strchr(s, ':') )
		Fl::add_timeout(50.0/1000.0, DelayedCompletionList, NULL);
}

static void OnTagPreset(Fl_Choice *o, void *p)
{
	if (o->mvalue() == g_mnuTagPresetsLabel)
		return;

	const int n = o->value();

#ifdef LOCALIZATION_SUPPORT
	const char *s = g_tagPresetsEnglish[n];
#else
	const char *s = g_mnuTagPresets[n].label();
#endif
	const char *cat = g_mnuTagPresets[n].user_data() ? (const char*)g_mnuTagPresets[n].user_data() : NULL;

	pTagEdPresets->value(g_mnuTagPresetsLabel);

	// if not an incomplete category tag (that requires the user to type in more), then add tag directly
	if (!cat)
	{
		if ( !strchr(s, ':') )
			pTagEdGroup->add_custom_tag(s);
		else
			SetAddTagInputText(s);
	}
	else if ( strcmp(s, TAGPRESET_CAT_CUST) )
	{
		string str = cat;
		str += s;
		pTagEdGroup->add_custom_tag( str.c_str() );
	}
	else
		SetAddTagInputText(cat);
}

static void OnTagEdViewInfoFile(Fl_Button *o, void *p)
{
	FMEntry *fm = g_pTagEdFM;

	const char *filename = g_mnuInfoFiles[ pTagEdInfoFile->value() ].label();

	if ( !FmOpenFileWithAssociatedApp(fm, filename) )
		fl_alert($("Failed to view file \"%s\"."), filename);
}

static BOOL TagEditorHasChanges(int *pValidationFailedOnPage = NULL)
{
	if (g_bTagEdFakeFMForcedHasChanges)
		return g_bTagEdFakeFMForcedHasChanges;

	// returns 2 if change (potentially) requires db re-filtering

	FMEntry *fm = g_pTagEdFM;

	if (pValidationFailedOnPage) *pValidationFailedOnPage = -1;

	// NOTE: must check those that can return 2 first
	// NOTE: if there are more things that can fail validation then we can't early out

	// release date
	if (*pTagEdRelYear->value() || *pTagEdRelMonth->value() || *pTagEdRelDay->value())
	{
		// check for incomplete date entry
		if (!*pTagEdRelYear->value() || !*pTagEdRelMonth->value() || !*pTagEdRelDay->value())
		{
			if (pValidationFailedOnPage)
			{
				*pValidationFailedOnPage = TABPAGE_MISC;
				fl_alert($("Incomplete release date specified."));
			}
			return 2;
		}

		int y = atoi( pTagEdRelYear->value() );
		int m = atoi( pTagEdRelMonth->value() );
		int d = atoi( pTagEdRelDay->value() );

		if (y < 1980 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31)
		{
invalid_date:
			if (pValidationFailedOnPage)
			{
				*pValidationFailedOnPage = TABPAGE_MISC;
				fl_alert($("Invalid release date specified."));
			}
			return 2;
		}

		struct tm t = {0};
		t.tm_year = y - 1900;
		t.tm_mon = m - 1;
		t.tm_mday = d;
		t.tm_hour = 12;
		if (_mkgmtime(&t) == (time_t)-1)
			goto invalid_date;

		// when pressing OK in create fm.ini mode, consider any entered date as a changed value
		if (pValidationFailedOnPage && g_bTagEdFakeFM)
			return TRUE;

		if (fm->tmReleaseDate)
		{
			struct tm *tm = gmtime(&fm->tmReleaseDate);
			if (tm->tm_year+1900 != y || tm->tm_mon+1 != m || tm->tm_mday != d)
				return 2;
		}
		else
			return 2;
	}
	else if (fm->tmReleaseDate)
		return 2;

	// tags
	if ( pTagEdGroup->has_changes() )
		return 2;

	// nice name
	if (Trimmed( pTagEdNiceName->value() ) != fm->nicename)
		return 2;

	// max description length (only consider this a change if pressing OK, ie. pValidationFailedOnPage != NULL,
	// otherwise dialog is closing with Cancel in which case we only care if the text was actually changed so
	// that we don't get "Discard changes" dialog when nothing was edited for incorrect fm.ini files with too
	// long description)
	if (g_pDescrBuffer->length() > MAX_DESCR_LEN)
	{
		if (pValidationFailedOnPage)
		{
			*pValidationFailedOnPage = TABPAGE_DESCR;
			fl_alert($("Description is %d long, max length is %d."), g_pDescrBuffer->length(), MAX_DESCR_LEN);
			return 2;
		}
	}

	// check vals that don't require re-filtering

	// info file
	if (!g_bTagEdFakeFM)
	{
		if (!g_infoFiles.empty()
			&& fm->infofile != g_mnuInfoFiles[ pTagEdInfoFile->value() ].label())
			return TRUE;
	}
	else
	{
		if ( Trimmed( pTagEdFakeInfoFile->value() ).size() )
			return TRUE;
	}

	// mod excludes
	if (FromUTF( Trimmed( pTagEdModExclude->value() ) ) != fm->modexclude)
		return TRUE;

	// release date verified
	if (!(fm->flags & FMEntry::FLAG_UnverifiedRelDate) != !pTagEdUnverified->value())
		return TRUE;

	// notes
	char *s = g_pNotesBuffer->text();
	string notes = EscapeString(s);
	if (s)
		free(s);
	if (fm->notes != notes)
		return TRUE;

	// description
	s = g_pDescrBuffer->text();
	notes = EscapeString(s);
	if (s)
		free(s);
	if (fm->descr != notes)
		return TRUE;

	return FALSE;
}

static void CloseTagEditor()
{
	BOOL bModified = g_cfg.windowsize_taged[0] != pTagEdWnd->w() || g_cfg.windowsize_taged[1] != pTagEdWnd->h();

	g_cfg.windowsize_taged[0] = pTagEdWnd->w();
	g_cfg.windowsize_taged[1] = pTagEdWnd->h();

	if (bModified)
		g_cfg.OnModified();

	pTagEdWnd->hide();
}

static void OnCancelTagEditor(Fl_Button *o, void *p)
{
	if (TagEditorHasChanges() && !fl_choice($("Discard changes?"), fl_cancel, fl_ok, NULL) )
		return;

	CloseTagEditor();
}

static void OnOkTagEditor(Fl_Button *o, void *p)
{
	// check if add tag input field has contents (to avoid common mistake of forgetting to actually add the tag)
	const char *val = pTagEdInput->value();
	if (val && *val)
	{
		string s = Trimmed(val);
		if (!s.empty() && !fl_choice($("The Add Tag input field contains \"%s\" that wasn't added as a tag.\nDiscard?"), fl_cancel, fl_ok, NULL, s.c_str()) )
		{
			pTagEdTabs->value( pTagEdTabs->child(TABPAGE_TAGS) );
			Fl::focus(pTagEdInput);
			return;
		}
	}

	int nValidationFailedOnPage = -1;
	BOOL bModified = TagEditorHasChanges(&nValidationFailedOnPage);
	if (nValidationFailedOnPage >= 0)
	{
		pTagEdTabs->value( pTagEdTabs->child(nValidationFailedOnPage) );
		return;
	}

	if (!bModified)
	{
		CloseTagEditor();
		return;
	}

	FMEntry *fm = g_pTagEdFM;

	//
	// TABPAGE_MISC
	//

	fm->SetNiceName( Trimmed( pTagEdNiceName->value() ).c_str() );

	// release date
	if ( *pTagEdRelYear->value() )
	{
		int y = atoi( pTagEdRelYear->value() );
		int m = atoi( pTagEdRelMonth->value() );
		int d = atoi( pTagEdRelDay->value() );

		struct tm t = {0};
		t.tm_year = y - 1900;
		t.tm_mon = m - 1;
		t.tm_mday = d;
		t.tm_hour = 12;
		fm->tmReleaseDate = _mkgmtime(&t);
	}
	else
		fm->tmReleaseDate = 0;

	if ( pTagEdUnverified->value() )
		fm->flags |= FMEntry::FLAG_UnverifiedRelDate;
	else
		fm->flags &= ~FMEntry::FLAG_UnverifiedRelDate;

	// conver mod excludes from UTF
	string modexcludes = Trimmed( pTagEdModExclude->value() );
	modexcludes = FromUTF(modexcludes);
	fm->SetModExclude( modexcludes.c_str() );

	if (!g_bTagEdFakeFM)
	{
		if ( !g_infoFiles.empty() )
			fm->infofile = g_mnuInfoFiles[ pTagEdInfoFile->value() ].label();
	}
	else
	{
		string s = Trimmed( pTagEdFakeInfoFile->value() );
		if ( s.size() )
			fm->infofile = s.c_str();
	}

	//
	// TABPAGE_TAGS
	//

	if (pTagEdGroup->apply_changes() && !g_bTagEdFakeFM)
		RemoveDeadTagFilters(FALSE);

	//
	// TABPAGE_DESCR
	//

	char *s = g_pDescrBuffer->text();
	string notes = EscapeString(s);
	if (s)
		free(s);
	fm->SetDescr( notes.c_str() );

	//
	// TABPAGE_NOTES
	//

	s = g_pNotesBuffer->text();
	notes = EscapeString(s);
	if (s)
		free(s);
	fm->SetNotes( notes.c_str() );

	//

	if (!g_bTagEdFakeFM)
	{
		fm->OnModified();

		if (bModified > 1)
		{
			if (!g_cfg.bAutoRefreshFilteredDb)
			{
				pFMList->refresh_rowsize(fm);
				pFMList->redraw();
			}
			else
				RefreshListControl(pFMList->selected(), FALSE);
		}
		else
			pFMList->redraw();
	}

	if (g_bTagEdFakeFM)
	{
		// ending dialog in create FM.INI mode, save fm.ini (return dialog if that fails or save dialog is cancelled)
		if ( !ExportFmIni(fm, TRUE) )
		{
			g_bTagEdFakeFMForcedHasChanges = TRUE;
			return;
		}
	}

	CloseTagEditor();
}


static void InitTagEdDialog(Fl_Window *pDlg, FMEntry *fm)
{
	static char sTitle[256];
	if (*fm->name)
		_snprintf_s(sTitle, sizeof(sTitle), _TRUNCATE, "%s -- %s", pDlg->label(), fm->GetFriendlyName());
	else
		strcpy(sTitle, $("Create FM.INI"));
	pDlg->label(sTitle);

	g_pTagEdFM = fm;

	if (g_cfg.windowsize_taged[0] && g_cfg.windowsize_taged[0])
	{
		// no way to access min size in Fl_Window, so we have to manually define vals, suck!!
		const int MINW = 540;
		const int MINH = 180;
		if (g_cfg.windowsize_taged[0] < MINW)
			g_cfg.windowsize_taged[0] = MINW;
		if (g_cfg.windowsize_taged[1] < MINH)
			g_cfg.windowsize_taged[1] = MINH;

		pDlg->size(g_cfg.windowsize_taged[0], g_cfg.windowsize_taged[1]);
	}

	// center over main window
	pDlg->position(
		pMainWnd->x() + ((pMainWnd->w() - pDlg->w()) / 2),
		pMainWnd->y() + ((pMainWnd->h() - pDlg->h()) / 2)
		);

	// OS specific init of window
	WndInitOS(pDlg);

	//
	// TABPAGE_MISC
	//

	pTagEdNiceName->maximum_size(128);
	if ( !fm->nicename.empty() )
		pTagEdNiceName->value( fm->nicename.c_str() );
	pTagEdName->value(fm->name_utf.c_str());
	if ( !fm->archive.empty() )
	{
		pTagEdArchive->value( fm->archive.c_str() );
		// deactivate control if unverified archive
		if ( !fm->IsArchived() )
			pTagEdArchive->deactivate();
	}
	else
		pTagEdArchive->deactivate();

	pTagEdRelYear->maximum_size(4);
	pTagEdRelMonth->maximum_size(2);
	pTagEdRelDay->maximum_size(2);
	if (fm->tmReleaseDate)
	{
		struct tm *tm = gmtime(&fm->tmReleaseDate);

		char s[16];

		sprintf(s, "%04d", tm->tm_year + 1900);
		pTagEdRelYear->value(s);

		sprintf(s, "%02d", tm->tm_mon + 1);
		pTagEdRelMonth->value(s);

		sprintf(s, "%02d", tm->tm_mday);
		pTagEdRelDay->value(s);

		if (fm->flags & FMEntry::FLAG_UnverifiedRelDate)
			pTagEdUnverified->value(1);
	}

	if ( !fm->modexclude.empty() )
	{
		string s = ToUTF(fm->modexclude);
		pTagEdModExclude->value( s.c_str() );
	}

	if (!g_bTagEdFakeFM)
	{
		// populate info file list

		int nSel = 0;
		if ( GetDocFiles(fm, g_infoFiles) )
		{
			if ( !fm->infofile.empty() )
			{
				nSel = -1;

				for (int i=0; i<(int)g_infoFiles.size(); i++)
					if ( !_stricmp(g_infoFiles[i].c_str(), fm->infofile.c_str()) )
					{
						nSel = i;
						break;
					}

				// if the file no longer exists then default to the first file
				if (nSel == -1)
				{
					fm->infofile = g_infoFiles[0].c_str();
					fm->flags &= ~FMEntry::FLAG_NoInfoFile;
					fm->OnModified();
					nSel = 0;
				}
			}

			const int MAX_MENU_ITEMS = g_infoFiles.size() + 1;
			int menu_items = 0;
			g_mnuInfoFiles = new Fl_Menu_Item[MAX_MENU_ITEMS];
			Fl_Menu_Item *menu = g_mnuInfoFiles;

			for (int i=0; i<(int)g_infoFiles.size(); i++)
			{
				MENU_ITEM(g_infoFiles[i].c_str(), i);
			}
			MENU_END();
		}
		else
		{
			const int MAX_MENU_ITEMS = 2;
			int menu_items = 0;
			g_mnuInfoFiles = new Fl_Menu_Item[MAX_MENU_ITEMS];
			Fl_Menu_Item *menu = g_mnuInfoFiles;

			MENU_ITEM($("<none>"), 0);
			MENU_END();

			// if is installed and no info file is found but previously it had one, then clear it
			if (fm->IsAvail() && !(fm->flags & FMEntry::FLAG_NoInfoFile))
			{
				fm->infofile.clear();
				fm->flags |= FMEntry::FLAG_NoInfoFile;
				fm->OnModified();
			}
		}
		pTagEdInfoFile->menu(g_mnuInfoFiles);
		pTagEdInfoFile->value(g_mnuInfoFiles + nSel);
		if ((int)g_infoFiles.size() <= 1)
			pTagEdInfoFile->deactivate();
		if ( g_infoFiles.empty() )
			pTagEdViewInfo->deactivate();
	}
	else
	{
		// replace pTagEdInfoFile/pTagEdViewInfo controls with simple text edit box
		pTagEdInfoFile->deactivate();
		pTagEdViewInfo->deactivate();
		pTagEdInfoFile->hide();
		pTagEdViewInfo->hide();

		int X, Y, W, H;
		X = pTagEdInfoFile->x();
		Y = pTagEdInfoFile->y();
		W = pTagEdViewInfo->x() + pTagEdViewInfo->w() - pTagEdInfoFile->x();
		H = pTagEdViewInfo->h();

		Fl_Input *o = pTagEdFakeInfoFile = new Fl_Input(X, Y, W, H, pTagEdInfoFile->label());
		o->tooltip( pTagEdInfoFile->tooltip() );
		o->labelsize( pTagEdInfoFile->labelsize() );
		o->textsize( pTagEdInfoFile->textsize() );
		o->align( pTagEdInfoFile->align() );
		pTagEdInfoFile->parent()->insert(*o, pTagEdInfoFile);
	}

	//
	// TABPAGE_TAGS
	//

	// probably way too much for tags, but at least it's not unlimited
	pTagEdInput->maximum_size(128);

	pTagEdAddTag->deactivate();

	pTagEdPresets->value(g_mnuTagPresetsLabel);

	pTagEdGroup->init(fm);

	//
	// TABPAGE_DESCR
	//

	pTagEdDescr->box(FLAT_MARGIN_BOX);
	if (g_cfg.bWrapDescrEditor)
		pTagEdDescr->wrap_mode(1, 0);

	pTagEdDescr->buffer(g_pDescrBuffer);
	if ( !fm->descr.empty() )
	{
		string s = UnescapeString( fm->descr.c_str() );
		g_pDescrBuffer->text( s.c_str() );
	}

	//
	// TABPAGE_NOTES
	//

	pTagEdNotes->box(FLAT_MARGIN_BOX);
	if (g_cfg.bWrapNotesEditor)
		pTagEdNotes->wrap_mode(1, 0);

	pTagEdNotes->buffer(g_pNotesBuffer);
	if ( !fm->notes.empty() )
	{
		string s = UnescapeString( fm->notes.c_str() );
		g_pNotesBuffer->text( s.c_str() );
	}

	//

	// disable all controls that aren't relevant for standalone FM.INI creation
	if (g_bTagEdFakeFM)
	{
		pTagEdName->deactivate();
		pTagEdUnverified->deactivate();
		pTagEdModExclude->deactivate();
		pTagEdNotes->deactivate();
	}
}

static void DoTagEditor(FMEntry *fm, int page)
{
	ASSERT(fm != NULL);

	g_bTagEdFakeFM = !*fm->name;
	g_bTagEdFakeFMForcedHasChanges = FALSE;

	Fl_FM_TagEd_Window *pDlg = MakeTagEditor(g_cfg.uiscale);
	ASSERT(pDlg != NULL);
	if (!pDlg)
		return;

	g_pNotesBuffer = new Fl_Text_Buffer;
	g_pDescrBuffer = new Fl_Text_Buffer;

	InitTagEdDialog(pDlg, fm);

	RefreshTagDb();

	pDlg->show();

	ASSERT((unsigned int)page < (unsigned int)pTagEdTabs->children());

	pTagEdTabs->value( pTagEdTabs->child(page) );

	if (page == TABPAGE_TAGS)
		Fl::focus(pTagEdInput);

	while ( pDlg->visible() )
	{
		Fl_Widget *o = Fl::readqueue();
#ifdef _WIN32
		if (!o) Fl::wait();
#else
		if (!o) Fl::wait(0);
#endif
	}

	delete (Fl_Widget*)pDlg;

	g_pTagEdFM = NULL;

	if (g_pNotesBuffer)
	{
		delete g_pNotesBuffer;
		g_pNotesBuffer = NULL;
	}
	if (g_pDescrBuffer)
	{
		delete g_pDescrBuffer;
		g_pDescrBuffer = NULL;
	}

	if (g_mnuInfoFiles)
	{
		delete[] g_mnuInfoFiles;
		g_mnuInfoFiles = NULL;
	}
	g_infoFiles.clear();
	g_infoFiles.reserve(0);

	// make sure main window is brought to front and activated again
	// (if the html popup was opened it can cause it to lose activation)
	pMainWnd->show();
}


/////////////////////////////////////////////////////////////////////
// MAIN WINDOW

static void CaptureUIState()
{
	// dump window stat stuff to config

	BOOL bWindowMax = MainWndIsMaximizedOS(pMainWnd);
	BOOL bModified = g_cfg.bWindowMax != bWindowMax
		|| g_cfg.windowpos[0] != pMainWnd->x() || g_cfg.windowpos[1] != pMainWnd->y()
		|| g_cfg.windowsize[0] != pMainWnd->w() || g_cfg.windowsize[1] != pMainWnd->h();

	g_cfg.bWindowMax = bWindowMax;

	if (!g_cfg.bWindowMax)
	{
		g_cfg.windowpos[0] = pMainWnd->x();
		g_cfg.windowpos[1] = pMainWnd->y();

		g_cfg.windowsize[0] = pMainWnd->w();
		g_cfg.windowsize[1] = pMainWnd->h();
	}

	if ( pFMList->save_col_state(g_cfg.colsizes) )
		bModified = TRUE;

	if (bModified)
		g_cfg.OnModified();
}

static void CloseMainWindow()
{
	if (pMainWnd->shown() && !pMainWnd->visible())
		MainWndRestoreOS(pMainWnd);

	CaptureUIState();

	pMainWnd->hide();
}


static void OnClose(Fl_Widget *o, void *p)
{
	CloseMainWindow();
}

static void OnResetFilters(Fl_Button *o, void *p)
{
	g_cfg.ResetFilters();
}

static void OnRefreshFilters(Fl_Button *o, void *p)
{
	RefreshFilteredDb();
}

static void DoFilterNameChanged(Fl_FM_Filter_Input *o)
{
	const char *val = o->value();

	if (val && *val)
		g_cfg.SetFilterName(val);
	else
		g_cfg.SetFilterName(NULL);
}

static void DelayedFilterNameChange(void *p)
{
	DoFilterNameChanged((Fl_FM_Filter_Input*)p);
}

static void OnFilterName(Fl_FM_Filter_Input *o, void *p)
{
	// this gets called when app is closing, avoid doing anything in that case
	if ( !pMainWnd->visible() )
		return;

	Fl::remove_timeout(DelayedFilterNameChange);

	if ( o->callback_reason_edit() )
	{
		// (re-)set expiration timer
		Fl::add_timeout(1.0, DelayedFilterNameChange, o);
		return;
	}

	DoFilterNameChanged(o);
}

static void OnFilterToggle(Fl_FM_Filter_Button *o, void *p)
{
	const BOOL bEnabled = o->value();

	switch ((intptr_t)p)
	{
	// status not played
	case 1: g_cfg.SetFilterShow(FSHOW_NotPlayed, bEnabled); break;
	// status completed
	case 2: g_cfg.SetFilterShow(FSHOW_Completed, bEnabled); break;
	// status in progress
	case 3: g_cfg.SetFilterShow(FSHOW_InProgress, bEnabled); break;
	// not available
	case 4: g_cfg.SetFilterShow(FSHOW_NotAvail, bEnabled); break;
	// archived
	case 5: g_cfg.SetFilterShow(FSHOW_ArchivedOnly, bEnabled); break;
	}
}

static void OnFilterRating(Fl_Choice *o, void *p)
{
	const int n = o->value();

	g_cfg.SetFilterRating(n - 1);
}

static void OnFilterPriority(Fl_Choice *o, void *p)
{
	const int n = o->value();

	g_cfg.SetFilterPriority(n);
}

static void OnFilterRelFrom(Fl_FM_Filter_Input *o, void *p)
{
	// this could get called when app is closing, avoid doing anything in that case
	if ( !pMainWnd->visible() )
		return;

	const char *val = o->value();
	if (val && *val)
	{
		g_cfg.SetFilterRelease(atoi(val), g_cfg.filtReleaseTo);

		char s[32];
		if (g_cfg.filtReleaseFrom >= 0)
			sprintf(s, "%02d", g_cfg.filtReleaseFrom);
		else
			*s = 0;
		pFilterRelDateFrom->value(s);
		pFilterRelDateFrom->position(pFilterRelDateFrom->size(), 0);
	}
	else
		g_cfg.SetFilterRelease(-1, g_cfg.filtReleaseTo);
}

static void OnFilterRelTo(Fl_FM_Filter_Input *o, void *p)
{
	// this could get called when app is closing, avoid doing anything in that case
	if ( !pMainWnd->visible() )
		return;

	const char *val = o->value();
	if (val && *val)
	{
		g_cfg.SetFilterRelease(g_cfg.filtReleaseFrom, atoi(val));

		char s[32];
		if (g_cfg.filtReleaseTo >= 0)
			sprintf(s, "%02d", g_cfg.filtReleaseTo);
		else
			*s = 0;
		pFilterRelDateTo->value(s);
		pFilterRelDateTo->position(pFilterRelDateTo->size(), 0);
	}
	else
		g_cfg.SetFilterRelease(g_cfg.filtReleaseFrom, -1);
}

static void OnPlayOM(Fl_Button *o, void *p)
{
	g_appReturn = kSelFMRet_Cancel;

	g_pFMSelData->bRunAfterGame = (g_cfg.returnAfterGame[g_bRunningEditor] == RET_ALWAYS);
	if (g_pFMSelData->bRunAfterGame)
	{
		g_cfg.dwLastProcessID = GetProcessIdOS();
		g_cfg.OnModified();
	}

	CloseMainWindow();
}

static void PlayFM(BOOL bSetInProgress)
{
	FMEntry *fm = pFMList->selected();

	if (!fm)
	{
		fl_message($("No FM selected."));
		return;
	}

	if ( !fm->IsInstalled() )
	{
		// if archived then ask to do an install and start playing if successful
		if (!fm->IsArchived() || !InstallFM(fm))
			return;
	}

	// if g_pFMSelData->sLanguage is set it means that "fm_language" was defined in a config file, if that's the case
	// it's treated like a forced FM language setting (mainly to allow FM authors to have an easy way to force different
	// languages without having to change dark's language setting), it's ignored if not supported by the FM
	if (*g_pFMSelData->sLanguage)
	{
		// check if the requested language is supported by the FM
		std::list<string> langlist;
		GetLanguages(fm, langlist);

		if ( IsLanguageInList(langlist, g_pFMSelData->sLanguage) )
			g_pFMSelData->bForceLanguage = TRUE;
		else
		{
			// language not supported, use fallback
			if ( !langlist.empty() )
				strcpy_safe(g_pFMSelData->sLanguage, g_pFMSelData->nLanguageLen, langlist.front().c_str());
			else
				*g_pFMSelData->sLanguage = 0;
			g_pFMSelData->bForceLanguage = FALSE;
		}
	}
	else
	{
		// determine FM default language (used if the FM doesn't support the language set in dark by the "language" cfg var)
		string lang;
		if ( GetFallbackLanguage(fm, lang) )
			strcpy_safe(g_pFMSelData->sLanguage, g_pFMSelData->nLanguageLen, lang.c_str());
		else
			*g_pFMSelData->sLanguage = 0;
		g_pFMSelData->bForceLanguage = FALSE;
	}

	fm->OnStart(bSetInProgress);

	strcpy_safe(g_pFMSelData->sName, g_pFMSelData->nMaxNameLen, fm->name);

	if ( !fm->modexclude.empty() )
		strcpy_safe(g_pFMSelData->sModExcludePaths, g_pFMSelData->nMaxModExcludeLen, fm->modexclude.c_str());

	g_pFMSelData->bRunAfterGame = (g_cfg.returnAfterGame[g_bRunningEditor] != RET_NEVER);
	if (g_pFMSelData->bRunAfterGame)
		g_cfg.dwLastProcessID = GetProcessIdOS();

	g_appReturn = kSelFMRet_OK;

	CloseMainWindow();
}

static void OnPlayFM(Fl_FM_Return_Button *o, void *p)
{
	PlayFM(FALSE);
}

static void OnStartFM(Fl_Button *o, void *p)
{
	PlayFM(TRUE);
}

static void OnExit(Fl_Button *o, void *p)
{
	g_appReturn = kSelFMRet_ExitGame;
	CloseMainWindow();
}

static void OnListSelChange(FMEntry *fm)
{
	if (!fm || !fm->IsAvail() || (!fm->IsInstalled() && g_sTempDir.empty()))
	{
		pBtnPlayFM->deactivate();
		pBtnStartFM->deactivate();
	}
	else
	{
		pBtnPlayFM->activate();
		if (fm->status == FMEntry::STATUS_InProgress)
			pBtnStartFM->deactivate();
		else
			pBtnStartFM->activate();
	}
}


static void ShowBadDirWarning()
{
	if ( g_invalidDirs.empty() )
		return;

	int lines = (int)g_invalidDirs.size();
	if (lines > 16)
		lines = 16;

	string s;
	s.reserve(2048);

	s.append($("Following FM directories were ignored due\nto exceeding the max name length of 30:\n"));
	for (int i=0; i<lines; i++)
	{
		s.append("\n");
		s.append(g_invalidDirs[i]);
	}

	if ((int)g_invalidDirs.size() > lines)
	{
		s.append("\n...");
	}

	fl_alert( s.c_str() );

	g_invalidDirs.clear();
}

static BOOL ValidateFmPath()
{
	// make sure the FM path exists

	struct stat st = {0};

	if (!stat(g_pFMSelData->sRootPath, &st) && (st.st_mode & S_IFDIR))
		return TRUE;

	if ( !fl_choice($("The FM path \"%s\" was not found or a valid directory.\nCreate the directory?"), $("Exit"), fl_yes, NULL, g_pFMSelData->sRootPath) )
		return FALSE;

	if ( !_mkdir(g_pFMSelData->sRootPath) )
		return TRUE;

	fl_alert($("Failed to create the FM path \"%s\", create it or configure a different path and run again."), g_pFMSelData->sRootPath);

	return FALSE;
}

static BOOL ValidateArchiveRepo(BOOL bFirstRun)
{
	if ( g_cfg.archiveRepo.empty() )
	{
		// on first run ask if user wants to configure a repo path
		if (bFirstRun && fl_choice(
			$("Would you like to configure an FM archive path now?\n\n"
			"(If you have a directory where you keep a collection of\n"
			"archived/zipped FMs, FMSel will be able to install FMs\n"
			"from it.)"),
			fl_no, fl_yes, NULL) )
		{
			ConfigArchivePath(TRUE);

			if ( g_cfg.archiveRepo.empty() )
				return TRUE;
		}
		else
		{
			return TRUE;
		}
	}

	// make sure that archive path is found

	struct stat st = {0};

	if (!stat(g_cfg.archiveRepo.c_str(), &st) && (st.st_mode & S_IFDIR))
	{
		// check that repo path isn't the same as the FM path
		if ( IsArchivePathSameAsFmPath( g_cfg.archiveRepo.c_str() ) )
		{
			fl_alert($("The archive path may not be inside the FM path, or vice versa.\nArchive support disabled. Reconfigure the archive path."));
			return FALSE;
		}

		g_cfg.bRepoOK = TRUE;
		return TRUE;
	}

	fl_alert($("Archive repository/collection path \"%s\" was not found or a valid directory."), g_cfg.archiveRepo.c_str());

	return FALSE;
}


static void InvokeAddTagFilter(const char *tag)
{
	pTagFilterGroup->ExternalFilterAdd(tag);
}

static void UpdateFilterControls()
{
	pFilterName->value( g_cfg.filtName.c_str() );
	pFilterRating->value(g_cfg.filtMinRating + 1);
	pFilterPrio->value(g_cfg.filtMinPrio);
	pFilterNotPlayed->value( !!(g_cfg.filtShow & FSHOW_NotPlayed) );
	pFilterCompleted->value( !!(g_cfg.filtShow & FSHOW_Completed) );
	pFilterInProgress->value( !!(g_cfg.filtShow & FSHOW_InProgress) );
	pFilterNotAvail->value( !!(g_cfg.filtShow & FSHOW_NotAvail) );
	pFilterArchived->value( !!(g_cfg.filtShow & FSHOW_ArchivedOnly) );

	char s[32];
	if (g_cfg.filtReleaseFrom >= 0)
		sprintf(s, "%02d", g_cfg.filtReleaseFrom);
	else
		*s = 0;
	pFilterRelDateFrom->value(s);
	if (g_cfg.filtReleaseTo >= 0)
		sprintf(s, "%02d", g_cfg.filtReleaseTo);
	else
		*s = 0;
	pFilterRelDateTo->value(s);

	pTagFilterGroup->OnFilterChanged();
}


static void InitControls()
{
	// set icons in priority filter combo box
	g_mnuFiltPrio[1].image(imgPrio[0]);
	g_mnuFiltPrio[2].image(imgPrio[1]);
	g_mnuFiltPrio[3].image(imgPrio[2]);

	pTagFilterGroup->init();

	UpdateFilterControls();

	pBtnPlayFM->deactivate();
	pBtnStartFM->deactivate();

	pFMList->post_init();

	// initial selection
	FMEntry *fm = g_cfg.lastfm.empty() ? NULL : GetFM( g_cfg.lastfm.c_str() );
	pFMList->select(fm, TRUE);
	OnListSelChange(fm ? fm : pFMList->selected());
}

static void InitMainWnd()
{
	static char sTitle[256];
	sprintf(sTitle, "%s -- [FMSel " FMSEL_VERSION "]", g_pFMSelData->sGameVersion);
	pMainWnd->label(sTitle);

	if (g_cfg.windowsize[0] && g_cfg.windowsize[0])
	{
		// no way to access min size in Fl_Window, so we have to manually define vals, suck!!
		const int MINW = 630;
		const int MINH = 380;
		if (g_cfg.windowsize[0] < MINW)
			g_cfg.windowsize[0] = MINW;
		if (g_cfg.windowsize[1] < MINH)
			g_cfg.windowsize[1] = MINH;

		pMainWnd->size(g_cfg.windowsize[0], g_cfg.windowsize[1]);
	}

	if (g_cfg.windowpos[0] != POS_UNDEF && g_cfg.windowpos[0] != POS_UNDEF)
	{
		pMainWnd->position(g_cfg.windowpos[0], g_cfg.windowpos[1]);
	}
	else
	{
		// center on desktop
		pMainWnd->position(
			Fl::x() + ((Fl::w() - pMainWnd->w()) / 2),
			Fl::y() + ((Fl::h() - pMainWnd->h()) / 2)
			);
	}

	// set proper list header size
	if (g_cfg.uiscale != 4) pFMList->col_header_height(SCALECFG(pFMList->col_header_height()));

	// do OS specific init of main window
	MainWndInitOS(pMainWnd);

	if (g_bRunningEditor)
	{
		pBtnStartFM->hide();
		pBtnPlayFM->label($("Edit FM"));
	}

	pFMList->post_init_header();
}


static void InitTempCache()
{
	// init temp dir used to extract files from archive that can't be accessed as a mem buffer
	// the temp dir is named ".fmsel.cache", in this function we ensure that the dir exists,
	// and delete its contents to clear out temp files from a previous session

	// could put it in the system temp dir, but for now we use the fmdir (for portable apps compliance)
	const char *tmp = g_pFMSelData->sRootPath;
	/*const char *tmp = getenv("TMP");
	if (!tmp || !fl_filename_isdir_ex(tmp, TRUE))
	{
		tmp = getenv("TEMP");
		if (!tmp || !fl_filename_isdir_ex(tmp, TRUE))
		{
			TRACE("Failed to create temp dir, no valid TMP/TEMP environment var");
			return;
		}
	}*/

	g_sTempDir = tmp;
	if ( !isdirsep(g_sTempDir[g_sTempDir.size()-1]) )
		g_sTempDir += DIRSEP;

	g_sTempDir += ".fmsel.cache" DIRSEP;
	if ( fl_filename_isdir_ex(g_sTempDir.c_str(), TRUE) )
	{
		// dir already existed, delete contents
		dirent **files;
		int nFiles = fl_filename_list(g_sTempDir.c_str(), &files, NO_COMP_UTFCONV);
		if (nFiles > 0)
		{
			string s;

			for (int i=0; i<nFiles; i++)
			{
				dirent *f = files[i];

				int len = strlen(f->d_name);
				if ( isdirsep(f->d_name[len-1]) )
				{
					// subdir (or ./ or ../)
					if (strcmp(f->d_name, "./") && strcmp(f->d_name, "../"))
					{
						f->d_name[len-1] = 0;
						s = g_sTempDir + f->d_name;

						if ( DelTree( s.c_str() ) )
						{
							TRACE("deleted temp dir subdir: %s", s.c_str());
						}
						else
						{
							TRACE("failed to delete temp dir subdir: %s", s.c_str());
						}
					}
				}
				else
				{
					s = g_sTempDir + f->d_name;
					if ( !remove_forced( s.c_str() ) )
					{
						TRACE("deleted temp dir file: %s", s.c_str());
					}
					else
					{
						TRACE("failed to delete temp dir file: %s", s.c_str());
					}
				}
			}

			fl_filename_free_list(&files, nFiles);
		}
	}
	else
	{
		// dir doesn't exist, create it
		if ( _mkdir( g_sTempDir.c_str() ) )
		{
			// failed to create
			TRACE("Failed to create temp dir %s", g_sTempDir.c_str());
			g_sTempDir.clear();
		}
		else
		{
			TRACE("created temp dir: %s", g_sTempDir.c_str());
		}
	}

	if ( !g_sTempDir.empty() )
	{
		// generate absolute path for temp dir
		char fullpath[MAX_PATH_BUF];
		fl_filename_absolute_ex(fullpath, sizeof(fullpath), g_sTempDir.c_str(), TRUE);
#ifdef _WIN32
		for (char *s=fullpath; *s; s++)
			if (*s == '/')
				*s = '\\';
#endif
		g_sTempDirAbs = fullpath;
	}
}

void ValidateTempCache()
{
	if ( !g_sTempDir.empty() )
		return;

	// one more try to initialize the temp dir
	InitTempCache();

	if ( !g_sTempDir.empty() )
		return;

	fl_alert(
		$("Failed to create the temp/cache directory \"%s%s.fmsel.cache\".\n"
		"Some functionality like uninstalling FMs will not be available"),
		g_pFMSelData->sRootPath, DIRSEP);
}


extern int FL_NORMAL_SIZE;
#ifdef CUSTOM_FLTK
extern Fl_Callback *fl_message_preshow_cb_;

static void OnPrepareFlMessageBox(Fl_Window *w, void*)
{
	Fl_Window *parent = Fl::modal();
	if (!parent)
		parent = pMainWnd;

	const int cx = parent->x() + (parent->w()/2);
	const int cy = parent->y() + (parent->h()/2);

	int x = cx - (w->w()/2);
	int y = cy - (w->h()/2);

	// neutralize offset caused by mouse in hotspot() (we still use hotspot() so we don't have to deal with the
	// preventing the window from being off-screen junk)
	int mx,my;
	Fl::get_mouse(mx,my);
	x -= mx;
	y -= my;

	w->hotspot(-x, -y);
}
#endif

static void fl_imagetext_label(const Fl_Label* o, int X, int Y, int W, int H, Fl_Align align)
{
	fl_font(o->font, o->size);
	fl_color((Fl_Color)o->color);
	if (o->image)
	{
		const int imgw = o->image->w();
		fl_draw(o->value, X+imgw, Y, W-imgw, H, (Fl_Align)(FL_ALIGN_CENTER|FL_ALIGN_INSIDE));
		o->image->draw(X, Y+((H-o->image->h())/2));
	}
	else
		fl_draw(o->value, X, Y, W, H, align);
}

static void fl_imagetext_measure(const Fl_Label* o, int& W, int& H)
{
	fl_font(o->font, o->size);
	fl_measure(o->value, W, H);
	if (o->image)
	{
		W += o->image->w() + 2;
		if (o->image->h() > H)
			H = o->image->h();
	}
}

static void fl_radioempty_label(const Fl_Label* o, int X, int Y, int W, int H, Fl_Align align)
{
	fl_font(o->font, o->size);
	fl_color((Fl_Color)o->color);

	X += imgMenuRadio.w() + 2;

	fl_draw(o->value, X, Y, W, H, align);
}

static void fl_radioset_label(const Fl_Label* o, int X, int Y, int W, int H, Fl_Align align)
{
	fl_font(o->font, o->size);
	//fl_color((Fl_Color)o->color);
	fl_color(FL_FOREGROUND_COLOR);

	imgMenuRadio.draw(X, Y+((H-imgMenuRadio.h())/2));
	X += imgMenuRadio.w() + 2;

	fl_draw(o->value, X, Y, W, H, align);
}

static void fl_radio_measure(const Fl_Label* o, int& W, int& H)
{
	fl_font(o->font, o->size);
	fl_measure(o->value, W, H);

	W += imgMenuRadio.w() + 4;
	if (imgMenuRadio.h() > H)
		H = imgMenuRadio.h();
}

// this is a stripped down version of fl_register_images() that rgisters the jpeg image type reduce dll bloat
static Fl_Image *fl_check_images_jpeg(const char *name, uchar *header, int)
{
	if (memcmp(header, "\377\330\377", 3) == 0 && header[3] >= 0xc0 && header[3] <= 0xef)
		return new Fl_JPEG_Image(name);

	return 0;
}
static void fl_register_jpeg()
{
	Fl_Shared_Image::add_handler(fl_check_images_jpeg);
}

static void draw_flatmarginbox(int x, int y, int w, int h, Fl_Color c)
{
	fl_color(c);
	fl_rectf(x+3, y+3, w-6, h-6);
	fl_color((Fl_Color)(USER_CLR+26));
	fl_rect(x, y, w, h);
	fl_color((Fl_Color)(USER_CLR+27));
	fl_rect(x+1, y+1, w-2, h-2);
	fl_rect(x+2, y+2, w-4, h-4);
}

static void InitMenuDef(Fl_Menu_Item *menu, int count)
{
	if (FL_NORMAL_SIZE == 12)
		return;

	for (int i=0; i<count; i++)
	{
		if (menu->label() && menu->labelsize() == 12)
			menu->labelsize(FL_NORMAL_SIZE);

		menu++;
	}
}

static void InitFLTK()
{
#ifdef LOCALIZATION_SUPPORT
	// localize standard button labels
	fl_no = $("No");
	fl_yes = $("Yes");
	fl_ok = $("OK");
	fl_cancel = $("Cancel");
	fl_close = $("Close");

	// localize misc other globals

	g_mnuTagPresetsLabel[0].label( $(g_mnuTagPresetsLabel[0].label()) );
	_snprintf_s(TAGPRESET_CAT_CUST, sizeof(TAGPRESET_CAT_CUST), _TRUNCATE, "<%s>", $("custom"));

	// copy original untranslated tag preset names
	for (unsigned int i=0; i<sizeof(g_mnuTagPresets)/sizeof(g_mnuTagPresets[0]); i++)
		g_tagPresetsEnglish[i] = g_mnuTagPresets[i].text;

	// localize tag presets (categories are displayed in both english and localized because manual
	// tag entry still needs to be typed in english)
	for (unsigned int i=0; i<sizeof(g_mnuTagPresets)/sizeof(g_mnuTagPresets[0]); i++)
	{
		if (!g_tagPresetsEnglish[i])
			continue;

		const char *colon = strchr(g_tagPresetsEnglish[i], ':');
		if (colon && colon != g_tagPresetsEnglish[i])
		{
			// tag category

			// get category name without colon
			char cat[512];
			int n = colon - g_tagPresetsEnglish[i];
			ASSERT(n < 512);
			memcpy(cat, g_tagPresetsEnglish[i], n);
			cat[n] = 0;

			// if localized then show both english and localized strings
			const char *catloc = $tag(cat);
			if (catloc != cat)
			{
				char s[1024];
				sprintf(s, "%s: (%s)", catloc, g_tagPresetsEnglish[i]);
				g_mnuTagPresets[i].text = g_tagPresetsExtraAllocated[i] = _strdup(s);
			}
		}
		else if (!g_mnuTagPresets[i].user_data_ || g_mnuTagPresets[i].text != TAGPRESET_CAT_CUST)
			// regular tag
			g_mnuTagPresets[i].text = $tag(g_mnuTagPresets[i].text);
	}
#endif

#ifdef CUSTOM_FLTK
	// set custom pre-show callback for fl message boxes so that we can center them to the dialog instead of mouse
	fl_message_preshow_cb_ = (Fl_Callback*)OnPrepareFlMessageBox;
#endif

	Fl::visual(FL_RGB);

	fl_register_jpeg();

	RegDefColors();

	if (g_cfg.uitheme == 2)
		Fl::scheme("plastic");
	else if (g_cfg.uitheme <= 1)
	{
		if (g_cfg.uitheme == 1) ChangeScheme();
		Fl::scheme("gtk+");
	}
	else
		Fl::scheme("base");

	FL_NORMAL_SIZE = SCALECFG((g_cfg.bLargeFont ? 14 : 12));
	fl_message_font(FL_HELVETICA, FL_NORMAL_SIZE);
	fl_message_icon()->box(FL_FLAT_BOX);
	fl_message_icon()->color(FL_GRAY0+20);

	Fl_Tooltip::size(FL_NORMAL_SIZE);

	Fl::set_labeltype(IMAGETEXT_LABEL, fl_imagetext_label, fl_imagetext_measure);
	Fl::set_labeltype(RADIO_EMPTY_LABEL, fl_radioempty_label, fl_radio_measure);
	Fl::set_labeltype(RADIO_SET_LABEL, fl_radioset_label, fl_radio_measure);
	Fl_FM_Html_Popup::static_init();
	Fl_FM_Tag_Input::static_init();
	Fl_FM_List::static_init();
	Fl::set_boxtype(FLAT_MARGIN_BOX, draw_flatmarginbox, 3, 3, 6, 6);
	Fl::scrollbar_size(SCALECFG(Fl::scrollbar_size()));

	pImgPlayFMGray = imgPlayFM.copy();
	pImgStartFMGray = imgStartFM.copy();
	pImgCheckGray = imgCheck.copy();
	pImgProgressGray = imgProgress.copy();
	pImgNotAvailGray = imgNotAvail.copy();
	pImgArchivedGray = imgArchived.copy();
	pImgViewFileGray = imgViewFile.copy();
	if (pImgPlayFMGray)
		pImgPlayFMGray->inactive();
	if (pImgStartFMGray)
		pImgStartFMGray->inactive();
	if (pImgCheckGray)
		pImgCheckGray->desaturate();
	if (pImgProgressGray)
		pImgProgressGray->desaturate();
	if (pImgNotAvailGray)
		pImgNotAvailGray->desaturate();
	if (pImgArchivedGray)
		pImgArchivedGray->desaturate();
	if (pImgViewFileGray)
		pImgViewFileGray->desaturate();

	InitMenuDef(g_mnuFiltRating, sizeof(g_mnuFiltRating)/sizeof(g_mnuFiltRating[0]));
	InitMenuDef(g_mnuFiltPrio, sizeof(g_mnuFiltPrio)/sizeof(g_mnuFiltPrio[0]));
	InitMenuDef(g_mnuTagPresets, sizeof(g_mnuTagPresets)/sizeof(g_mnuTagPresets[0]));
	g_mnuTagPresetsLabel[0].labelsize(FL_NORMAL_SIZE-1);
}

static void TermFLTK()
{
	if (pImgPlayFMGray)
		delete pImgPlayFMGray;
	if (pImgStartFMGray)
		delete pImgStartFMGray;
	if (pImgCheckGray)
		delete pImgCheckGray;
	if (pImgProgressGray)
		delete pImgProgressGray;
	if (pImgNotAvailGray)
		delete pImgNotAvailGray;
	if (pImgArchivedGray)
		delete pImgArchivedGray;
	if (pImgViewFileGray)
		delete pImgViewFileGray;

	FltkTermOS();
}


/////////////////////////////////////////////////////////////////////
// STARTUP MESSAGE WINDOW

Fl_Window *g_pStartupWnd = NULL;

// display message window that fmsel is scanning FMs which may take a little while
static void ShowStartupMessage()
{
	Fl_Window *w = g_pStartupWnd = new Fl_FM_Progress_Window(250, 40, "");
	w->clear_border();
	w->box(FL_NO_BOX);

	Fl_Box *o = new Fl_Box(0, 0, w->w(), w->h());
	o->box(FL_UP_BOX);
	o->labelfont(FL_HELVETICA_BOLD);
	o->labelsize(12);
	o->align(FL_ALIGN_CENTER);
	o->label($("Scanning FMs..."));

	w->end();

	w->set_modal();

	// center over main window
	w->position(
		pMainWnd->x() + ((pMainWnd->w() - w->w()) / 2),
		pMainWnd->y() + ((pMainWnd->h() - w->h()) / 2)
		);

	w->show();

	ShowBusyCursor(TRUE);

	// pump messages so window becomes visible
	while ( Fl::readqueue() );
	Fl::wait(50.0/1000.0);

	fl_cursor(FL_CURSOR_WAIT);
}

static void HideStartupMessage()
{
	if (g_pStartupWnd)
	{
		// empty message queue
		while ( Fl::readqueue() );
		Fl::wait(50.0/1000.0);

		g_pStartupWnd->hide();

		delete (Fl_Widget*)g_pStartupWnd;
		g_pStartupWnd = NULL;

		ShowBusyCursor(FALSE);
	}
}


/////////////////////////////////////////////////////////////////////
//

// localize a tag string
const char* $tag(const char *tag)
{
#ifdef LOCALIZATION_SUPPORT
	if (localization_supported && tag && *tag)
	{
		ASSERT(localization_initialized);

		// tag translations have a "tag_" prefix in the localization db
		char loc_key[1024];
		sprintf(loc_key, LOCALIZED_TAG_PREFIX"%s", tag);

		// check if translation exists, otherwise return 'tag'
		const char *loc = $(loc_key);
		if (loc != loc_key)
			return loc;
	}
#endif

	return tag;
}

static void PrepareLocalization()
{
	InitLocalization();

#ifdef LOCALIZATION_SUPPORT
	// localize globals
	TAGS_LABEL = $(TAGS_LABEL);
	RATING_COL_LABEL = $(RATING_COL_LABEL);
#endif
}

static void CleanupLocalization()
{
#ifdef LOCALIZATION_SUPPORT
	// delete any localization specific allocs
	for (unsigned int i=0; i<sizeof(g_tagPresetsExtraAllocated)/sizeof(g_tagPresetsExtraAllocated[0]); i++)
	{
		if (g_tagPresetsExtraAllocated[i])
		{
			free((char*)g_tagPresetsExtraAllocated[i]);
			g_tagPresetsExtraAllocated[i] = NULL;
		}
	}
#endif

	TermLocalization();
}


/////////////////////////////////////////////////////////////////////
// FM SEL API

extern "C" int FMSELAPI SelectFM(sFMSelectorData *data)
{
	if (!data || (unsigned int)data->nStructSize < sizeof(sFMSelectorData))
	{
		return kSelFMRet_Cancel;
	}

	g_pFMSelData = data;
	CleanDirSlashes(g_pFMSelData->sRootPath);

	g_bRunningEditor = (data->sGameVersion && *data->sGameVersion) && !strncmp(data->sGameVersion, "DromEd", 6);
	g_bRunningShock = (data->sGameVersion && *data->sGameVersion) && strstr(data->sGameVersion, "Shock");

	PrepareLocalization();

	InitTempCache();

	const BOOL bFirstTime = (LoadDb() == 2);

	// when running after game exit then give the old process a little time to shut down (just to be nice)
	if (data->bExitedGame)
		WaitForProcessExitOS(g_cfg.dwLastProcessID, 2000);
	g_cfg.dwLastProcessID = 0;

	InitFLTK();

	MakeWindow(g_cfg.uiscale);

	if (pMainWnd)
	{
		InitMainWnd();

		pMainWnd->show();

		if (g_cfg.bWindowMax)
			MainWndMaximizeOS(pMainWnd);

		if ( !ValidateFmPath() )
		{
			pMainWnd->hide();
			goto abort;
		}

		ValidateArchiveRepo(bFirstTime);

		ValidateTempCache();

		ShowBusyCursor(TRUE);

		ShowStartupMessage();

		ScanFmDir();
		RefreshFilteredDb(FALSE);

		ShowBusyCursor(FALSE);

		HideStartupMessage();

		InitControls();

		ShowBadDirWarning();

		Fl::run();

		if ( !SaveDb() )
		{
			fl_alert($("Failed to save database (fmsel.ini). Make sure you have write access\n"
				"to the file and FM path and that there's enough free disk space\n"
				"before closing this dialog, otherwise any changes will be lost."));

			// do another attempt
			SaveDb();
		}
		TermDb();

abort:
		MainWndTermOS(pMainWnd);

		delete pMainWnd;

		TermMP3();
		TermArchiveSystem();
		TermFLTK();
		CleanupLocalization();

		return g_appReturn;
	}

	TermMP3();
	TermArchiveSystem();
	TermFLTK();
	TermLocalization();

	return kSelFMRet_Cancel;
}
