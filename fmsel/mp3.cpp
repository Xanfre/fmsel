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
#include <io.h>
#else
#define _strnicmp strncasecmp
#endif
#include <string.h>
#include "mp3.h"
#include "os.h"
#include "lang.h"

#ifdef OGG_SUPPORT
#pragma pack(8)
#include <vorbis/vorbisfile.h>
#pragma pack()
#endif

#include <FL/fl_ask.H>


#if defined(OGG_SUPPORT) && defined(_MSC_VER)
#pragma comment(lib, "libogg_static.lib")
#pragma comment(lib, "libvorbis_static.lib")
#pragma comment(lib, "libvorbisfile_static.lib")
#endif


// links to lame binaries can be found on http://lame.sourceforge.net/links.php#Binaries
// for Windows the recommended download is libmp3lame from http://www.rarewares.org/mp3-lame-libraries.php

// allthough lame is primary an encoder and the decoding functions use the mpg123 lib, I chose lame over
// mpg123 because of the more user friendly availability of cross platform binaries, and also lame wraps
// some functionality like dealing with VBR that we don't have to mess with


typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned int FOURCC;
#define WAVFMT_PCM 1
#define WAVFMT_MP3 0x55
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#pragma pack(1)

struct WAVHEADER
{
	FOURCC fcRIFF;
	DWORD dwFileSize;
	FOURCC fcWAVE;
	FOURCC fcFMT;
	DWORD dwWaveSize;

	// WAVEFORMAT
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;

	FOURCC fcDATA;
	DWORD dwDataSize;
};

struct WAVEFORMATEX
{
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
};

#pragma pack()


static void *g_hDll = NULL;

hip_t (CDECL *lame_decode_init)(void) = NULL;
int (CDECL *lame_decode_exit)(hip_t gfp) = NULL;
int (CDECL *lame_decode1_headersB)(hip_t gfp, unsigned char *mp3buf, int len, short pcm_l[], short pcm_r[], sMp3data *mp3data, int *enc_delay, int *enc_padding) = NULL;


bool InitMP3()
{
	if (g_hDll)
		return true;

#ifdef _WIN32
	// strip extension
	char dllname[256];
	strcpy(dllname, MP3LIB);
	char *ext = strrchr(dllname, '.');
	if (ext)
		*ext = 0;

	g_hDll = LoadDynamicLibOS(dllname);
#else
	g_hDll = LoadDynamicLibOS(MP3LIB);
#endif
	if (!g_hDll)
		return false;

	lame_decode_init = (hip_t (CDECL*)(void)) GetDynamicLibProcOS(g_hDll, "hip_decode_init");
	lame_decode_exit = (int (CDECL*)(hip_t)) GetDynamicLibProcOS(g_hDll, "hip_decode_exit");
	lame_decode1_headersB = (int (CDECL*)(hip_t,unsigned char*,int,short[],short[],sMp3data*,int*,int*)) GetDynamicLibProcOS(g_hDll, "hip_decode1_headersB");

	if (!lame_decode_init || !lame_decode_exit || !lame_decode1_headersB)
	{
		CloseDynamicLibOS(g_hDll);
		g_hDll = NULL;
		fl_alert($("Wrong %s version found."), MP3LIB);
		return false;
	}

	return true;
}

void TermMP3()
{
	if (!g_hDll)
		return;

	CloseDynamicLibOS(g_hDll);

	g_hDll = NULL;
}

static bool IsMpFrame(const unsigned char *p)
{
	if (p[0] != 0xff || (p[1] & 0xe0) != 0xe0 || (p[1] & 0x18) == 8)
		return false;

	if (!(p[1] & 6) || (p[2] & 0xf0) == 0xf0 || (p[2] & 0x0c) == 0x0c || (p[3] & 3) == 2)
		return false;

	const char abl2[16] = {0,7,7,7,0,7,0,0,0,0,0,8,8,8,8,8};

	if ((p[1] & 0x18) == 0x18 && (p[1] & 6) == 4 && (abl2[p[2]>>4] & (1 << (p[3] >> 6))))
		return false;

	return true;
}

bool InitMp3File(FILE *f)
{
	if (!f)
		return false;

	// check if file is actually a WAV file with mp3 data
	WAVHEADER hdr = {};
	fread(&hdr, 1, sizeof(DWORD)*5, f);

	if (hdr.fcRIFF != MAKEFOURCC('R','I','F','F'))
	{
		// not WAV

		char *hdata = (char*)&hdr;

		fseek(f, 4, SEEK_SET);

		// skip past (optional) ID3 headers
		if ( !strncmp(hdata, "ID3", 3) )
		{
			do
			{
				fread(hdata, 1, 6, f);
				hdata[2] &= 0x7f;
				hdata[3] &= 0x7f;
				hdata[4] &= 0x7f;
				hdata[5] &= 0x7f;
				const int l = (hdata[2] << 21) | (hdata[3] << 14) | (hdata[4] << 7) | hdata[5];
				fseek(f, l, SEEK_CUR);

				if (fread(hdata, 1, 4, f) < 4)
					break;
			}
			while ( !strncmp(hdata, "ID3", 3) );
		}

		// skip past (optional) AiD header
		if ( !strncmp(hdata, "AiD\1", 4) )
		{
			fread(hdata, 1, 2, f);
			const int l = ((DWORD)(BYTE)hdata[1] << 8) | (DWORD)(BYTE)hdata[0];
			fseek(f, l - 6, SEEK_CUR);

			fread(hdata, 1, 4, f);
		}

		// scan to first frame
		while ( !IsMpFrame((unsigned char*)hdata) )
		{
			hdata[0] = hdata[1];
			hdata[1] = hdata[2];
			hdata[2] = hdata[3];
			if (fread(hdata+3, 1, 1, f) != 1)
				return false;
		}

		// move back file pos to start of frame
		fseek(f, -4, SEEK_CUR);

		return true;
	}

	if (hdr.fcWAVE != MAKEFOURCC('W','A','V','E') || hdr.fcFMT != MAKEFOURCC('f','m','t',' '))
		return false;

	int extra_data = 0;
	WAVEFORMATEX wf = {};

	if (hdr.dwWaveSize >= 18)
	{
		// WAVEFORMATEX

		fread(&wf, 1, 18, f);

		// make sure it's mp3 data
		if (wf.wFormatTag != WAVFMT_MP3)
			return false;

		extra_data = wf.cbSize;
	}
	else
	{
		fread(&hdr.wFormatTag, 1, hdr.dwWaveSize>16 ? 16 : hdr.dwWaveSize, f);

		// make sure it's mp3 data
		if (hdr.wFormatTag != WAVFMT_MP3)
			return false;
	}

	// move file pos to start of data chunk
	fseek(f, 12 + 8 + hdr.dwWaveSize + extra_data, SEEK_SET);

	fread(&hdr.fcDATA, 1, 8, f);
	if (hdr.fcDATA != MAKEFOURCC('d','a','t','a'))
		return false;

	return true;
}

bool InitWavFile(FILE *f)
{
	// reserve file space for wav header
	WAVHEADER hdr = {};
	return fwrite(&hdr, 1, sizeof(hdr), f) == sizeof(hdr);
}

bool FinalizeWavFile(FILE *f, int nSampleRate, int nChannels)
{
	// update reserved wav header with actual data

	const long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	WAVHEADER hdr = {};
	hdr.fcRIFF = MAKEFOURCC('R','I','F','F');
	hdr.dwFileSize = fsize - 8;
	hdr.fcWAVE = MAKEFOURCC('W','A','V','E');

	hdr.fcFMT = MAKEFOURCC('f','m','t',' ');
	hdr.dwWaveSize = 16;
	hdr.wFormatTag = WAVFMT_PCM;
	hdr.nChannels = nChannels;
	hdr.nSamplesPerSec = nSampleRate;
	hdr.wBitsPerSample = 16;
	hdr.nAvgBytesPerSec = hdr.nSamplesPerSec * hdr.nChannels * (hdr.wBitsPerSample / 8);
	hdr.nBlockAlign = nChannels * (hdr.wBitsPerSample / 8);

	hdr.fcDATA = MAKEFOURCC('d','a','t','a');
	hdr.dwDataSize = fsize - sizeof(hdr);

	return fwrite(&hdr, 1, sizeof(hdr), f) == sizeof(hdr);
}


/////////////////////////////////////////////////////////////////////
// OGG SUPPORT

#ifdef OGG_SUPPORT

static size_t OggReadFile(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	return fread(ptr, size, nmemb, (FILE*)datasource);
}

static unsigned int DecompressOgg(OggVorbis_File &vf, FILE *fwav)
{
	const int BUF_SIZE = 128*1024;
	char *buf = new char[BUF_SIZE];

	if (!buf)
		return 0;

	unsigned int decompressed_bytes = 0;

	// read until EOF
	for (;;)
	{
		int stridx;
		long r = ov_read(&vf, buf, BUF_SIZE, 0, sizeof(short), 1, &stridx);

		if (stridx)
			// only interested in bit-stream 0
			continue;

		if (r <= 0)
			break;

		fwrite(buf, 1, r, fwav);

		decompressed_bytes += r;
	}

	delete[] buf;

	return decompressed_bytes;
}

#endif


bool ConvertOggFile(FILE *f, FILE *fwav)
{
#ifdef OGG_SUPPORT
	if (!f)
		return false;

	ov_callbacks callbacks = { OggReadFile, NULL, NULL, NULL };
	OggVorbis_File vf;

	if (ov_open_callbacks(f, &vf, NULL, 0, callbacks) < 0)
		return false;

	InitWavFile(fwav);

	unsigned int outbytes = DecompressOgg(vf, fwav);
	if (!outbytes)
	{
		ov_clear(&vf);
		return false;
	}

	if ( !FinalizeWavFile(fwav, vf.vi->rate, vf.vi->channels) )
	{
		ov_clear(&vf);
		return false;
	}

	ov_clear(&vf);

	return true;
#else
	return false;
#endif
}

const char* GetOggVersion()
{
#ifdef OGG_SUPPORT
	const char *s = vorbis_version_string();
	// skip to "libVorbis" part of the version string
	if (s && !_strnicmp(s, "Xiph.Org ", 9))
		s += 9;
	return s;
#else
	return "libVorbis";
#endif
}
