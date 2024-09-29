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

#ifndef _MP3_H_
#define _MP3_H_

#include <stdio.h>

#ifdef _WIN32
#define MP3LIB "libmp3lame.dll"
#else
#define MP3LIB "libmp3lame.so.0"
#endif


#pragma pack(4)

typedef struct sMp3data
{
	int header_parsed;
	int channels;
	int sample_rate;
	int bit_rate;
	int mode;
	int mode_ext;
	int frame_size;
	unsigned long nsamp;
	int total_frames;
	int frame_num;
} sMp3data;

#pragma pack()


#ifdef _WIN32
#undef CDECL
#define CDECL __cdecl
#else
#define CDECL
#endif

typedef void *hip_t;

// DLL function pointers resolved by InitMP3
extern hip_t (CDECL *lame_decode_init)(void);
extern int (CDECL *lame_decode_exit)(hip_t gfp);
extern int (CDECL *lame_decode1_headersB)(hip_t gfp, unsigned char *mp3buf, int len, short pcm_l[], short pcm_r[], sMp3data* mp3data, int *enc_delay, int *enc_padding);

// lame_decode1_headers wrapper
inline int lame_decode1_headers(hip_t gfp, unsigned char *mp3buf, int len, short pcm_l[], short pcm_r[], sMp3data* mp3data)
{
	int foo1, foo2;
	return lame_decode1_headersB(gfp, mp3buf, len, pcm_l, pcm_r, mp3data, &foo1, &foo2);
}

bool InitMP3();
void TermMP3();
bool InitMp3File(FILE *f);
bool InitWavFile(FILE *f);
bool FinalizeWavFile(FILE *f, int nSampleRate, int nChannels);

#ifdef OGG_SUPPORT
bool ConvertOggFile(FILE *f, FILE *fwav);
const char* GetOggVersion();
#endif

#endif // _MP3_H_
