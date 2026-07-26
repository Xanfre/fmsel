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

#if defined(AUDIO_SUPPORT) && !defined(_MP3_H_)
#define _MP3_H_

#include <stdio.h>

const char* GetWavVersion();
#ifdef MP3_SUPPORT
bool ConvertMp3File(const char *name, const char *wavname);
const char* GetMp3Version();
#endif
#ifdef OGG_SUPPORT
bool ConvertOggFile(const char *name, const char *wavname);
const char* GetOggVersion();
#endif
#ifdef OPUS_SUPPORT
bool ConvertOpusFile(const char *name, const char *wavname);
const char* GetOpusVersion();
#endif
#ifdef FLAC_SUPPORT
bool ConvertFlacFile(const char *name, const char *wavname);
const char* GetFlacVersion();
#endif

#endif // AUDIO_SUPPORT && !_MP3_H_
