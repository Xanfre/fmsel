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

#include <string.h>
#ifndef _WIN32
#define _strnicmp strncasecmp
#endif
#include "mp3.h"
#include "os.h"
#include "lang.h"

#pragma pack(8)
#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_CONVERSION_API
#include <dr_wav.h>
#ifdef MP3_SUPPORT
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>
#endif
#ifdef OGG_SUPPORT
#include <vorbis/vorbisfile.h>
#endif
#ifdef OPUS_SUPPORT
#include <opus/opusfile.h>
#endif
#ifdef FLAC_SUPPORT
#include <FLAC/metadata.h>
#include <FLAC/stream_decoder.h>
#endif
#pragma pack()

#include <FL/fl_ask.H>
#include <FL/fl_utf8.h>


#define BUF_SIZE 128*1024

/////////////////////////////////////////////////////////////////////
// WAV SUPPORT

const char* GetWavVersion()
{
	static char drwavver[24] = { 0 };
	if (!*drwavver)
	{
		strcpy(drwavver, "dr_wav ");
		strcat(drwavver, drwav_version_string());
	}
	return drwavver;
}

/////////////////////////////////////////////////////////////////////
// MP3 SUPPORT

#ifdef MP3_SUPPORT
static bool DecompressMp3(drmp3 *dec, drwav *wav)
{
	drmp3_int16 buf[BUF_SIZE / sizeof(drmp3_int16)];

	// read until EOF
	for (;;)
	{
		drmp3_uint64 frames = drmp3_read_pcm_frames_s16(dec, BUF_SIZE / dec->channels / sizeof(drmp3_int16), buf);

		if (frames <= 0)
			break;

		if (frames != (drmp3_uint64)drwav_write_pcm_frames(wav, frames, buf))
			return false;
	}

	return true;
}

bool ConvertMp3File(const char *name, const char *wavname)
{
	if (!name || !wavname)
		return false;

	/*
	// check if file is actually a WAV file with mp3 data
	if (DRWAV_TRUE != drwav_init(&wav, Mp3ReadFile, WavSeekFile, f, NULL))
	{
		if (wav.container == drwav_container_riff && wav.fmt.formatTag == 0x55) // MP3
			fseek(f, wav.dataChunkDataPos, SEEK_SET);
		else
			fseek(f, 0, SEEK_SET);
		drwav_uninit(&wav);
	}
	else
		fseek(f, 0, SEEK_SET);
	*/

	drmp3 dec;
#ifdef _WIN32
	if (DRMP3_TRUE != drmp3_init_file_w(&dec, WidenStrOS(name), NULL))
#else
	if (DRMP3_TRUE != drmp3_init_file(&dec, name, NULL))
#endif
		return false;

	drwav wav;
	drwav_data_format fmt = { drwav_container_riff, DR_WAVE_FORMAT_PCM, (drwav_uint32)dec.channels, (drwav_uint32)dec.sampleRate, 16 };
#ifdef _WIN32
	if (DRWAV_TRUE != drwav_init_file_write_w(&wav, WidenStrOS(wavname).c_str(), &fmt, NULL))
#else
	if (DRWAV_TRUE != drwav_init_file_write(&wav, wavname, &fmt, NULL))
#endif
	{
		drmp3_uninit(&dec);
		return false;
	}

	bool success = DecompressMp3(&dec, &wav);

	drwav_uninit(&wav);
	drmp3_uninit(&dec);

	return success;
}

const char* GetMp3Version()
{
	static char drmp3ver[24] = { 0 };
	if (!*drmp3ver)
	{
		strcpy(drmp3ver, "dr_mp3 ");
		strncat(drmp3ver, drmp3_version_string(), sizeof(drmp3ver) - 8);
	}
	return drmp3ver;
}
#endif

/////////////////////////////////////////////////////////////////////
// OGG SUPPORT

#ifdef OGG_SUPPORT
static bool DecompressOgg(OggVorbis_File *vf, drwav *wav)
{
	char buf[BUF_SIZE];

	// read until EOF
	for (;;)
	{
		int stridx;
		long frames = ov_read(vf, buf, BUF_SIZE, 0, sizeof(short), 1, &stridx) / vf->vi->channels / sizeof(short);

		if (stridx)
			// only interested in bit-stream 0
			continue;

		if (frames <= 0)
			break;

		if (frames != (long)drwav_write_pcm_frames(wav, frames, buf))
			return false;
	}

	return true;
}

bool ConvertOggFile(const char *name, const char *wavname)
{
	if (!name || !wavname)
		return false;

	OggVorbis_File vf;

#ifdef _WIN32
	if (ov_fopen(DemoteStrOS(name).c_str(), &vf) < 0)
#else
	if (ov_fopen(name, &vf) < 0)
#endif
		return false;

	drwav wav;
	drwav_data_format fmt = { drwav_container_riff, DR_WAVE_FORMAT_PCM, (drwav_uint32)vf.vi->channels, (drwav_uint32)vf.vi->rate, 16 };
#ifdef _WIN32
	if (DRWAV_TRUE != drwav_init_file_write_w(&wav, WidenStrOS(wavname).c_str(), &fmt, NULL))
#else
	if (DRWAV_TRUE != drwav_init_file_write(&wav, wavname, &fmt, NULL))
#endif
	{
		ov_clear(&vf);
		return false;
	}

	bool result = DecompressOgg(&vf, &wav);

	drwav_uninit(&wav);
	ov_clear(&vf);

	return result;
}

const char* GetOggVersion()
{
	const char *s = vorbis_version_string();
	// skip to "libVorbis" part of the version string
	if (s && !_strnicmp(s, "Xiph.Org ", 9))
		s += 9;
	return s;
}
#endif

/////////////////////////////////////////////////////////////////////
// OPUS SUPPORT

#ifdef OPUS_SUPPORT
static bool DecompressOpus(OggOpusFile *of, drwav *wav)
{
	opus_int16 buf[BUF_SIZE / sizeof(opus_int16)];

	// read until EOF
	for (;;)
	{
		int lnkidx;
		int frames = op_read(of, buf, BUF_SIZE / sizeof(opus_int16), &lnkidx);

		if (lnkidx)
			// only interested in bit-stream 0
			continue;

		if (frames <= 0)
			break;

		if (frames != (int)drwav_write_pcm_frames(wav, frames, buf))
			return false;
	}

	return true;
}

bool ConvertOpusFile(const char *name, const char *wavname)
{
	if (!name || !wavname)
		return false;

#ifdef _WIN32
	OggOpusFile *of = op_open_file(DemoteStrOS(name).c_str(), NULL);
#else
	OggOpusFile *of = op_open_file(name, NULL);
#endif
	if (NULL == of)
		return false;

	drwav wav;
	drwav_data_format fmt = { drwav_container_riff, DR_WAVE_FORMAT_PCM, (drwav_uint32)op_channel_count(of, 0), 48000, 16 };
#ifdef _WIN32
	if (DRWAV_TRUE != drwav_init_file_write_w(&wav, WidenStrOS(wavname).c_str(), &fmt, NULL))
#else
	if (DRWAV_TRUE != drwav_init_file_write(&wav, wavname, &fmt, NULL))
#endif
	{
		op_free(of);
		return false;
	}

	bool success = DecompressOpus(of, &wav);

	drwav_uninit(&wav);
	op_free(of);

	return success;
}

const char* GetOpusVersion()
{
	return opus_get_version_string();
}
#endif

/////////////////////////////////////////////////////////////////////
// FLAC SUPPORT

#ifdef FLAC_SUPPORT
static FLAC__StreamDecoderWriteStatus FlacWriteBuf(const FLAC__StreamDecoder *dec, const FLAC__Frame *frame, const FLAC__int32 * const buf[], void *data)
{
	uint32_t ch = FLAC__stream_decoder_get_channels(dec);
	uint32_t bps = FLAC__stream_decoder_get_bits_per_sample(dec) >> 3;

	if (ch == 0 || bps == 0 || NULL == data || BUF_SIZE < frame->header.blocksize * ch * bps)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	FLAC__uint8 outBuf[BUF_SIZE];
	FLAC__uint8 *pOutBuf = outBuf;
	drwav_uint64 w = 0;

	switch (FLAC__stream_decoder_get_bits_per_sample(dec))
	{
		case 8:
			for (size_t i = 0; i < frame->header.blocksize; i++)
			{
				for (uint32_t j = 0; j < ch; j++)
				{
					FLAC__int32 s = buf[j][i];
					*(pOutBuf++) = (FLAC__uint8)(s + 128);
				}
				w++;
			}
			break;
		case 16:
			for (size_t i = 0; i < frame->header.blocksize; i++)
			{
				for (uint32_t j = 0; j < ch; j++)
				{
					FLAC__int32 s = buf[j][i];
					*(FLAC__int16*)pOutBuf = (FLAC__int16)s;
					pOutBuf += sizeof(FLAC__int16)/sizeof(FLAC__uint8);
				}
				w++;
			}
			break;
		case 24:
			for (size_t i = 0; i < frame->header.blocksize; i++)
			{
				for (uint32_t j = 0; j < ch; j++)
				{
					FLAC__int32 s = buf[j][i];
					*(pOutBuf++) = (FLAC__uint8)s;
					*(pOutBuf++) = (FLAC__uint8)(s >> 8);
					*(pOutBuf++) = (FLAC__uint8)(s >> 16);
				}
				w++;
			}
			break;
		case 32:
			for (size_t i = 0; i < frame->header.blocksize; i++)
			{
				for (uint32_t j = 0; j < ch; j++)
				{
					FLAC__int32 s = buf[j][i];
					*(FLAC__int32*)pOutBuf = s;
					pOutBuf += sizeof(FLAC__int32)/sizeof(FLAC__uint8);
				}
				w++;
			}
			break;
		default:
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (w != drwav_write_pcm_frames((drwav*)data, w, outBuf))
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void FlacError(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *) {}

bool ConvertFlacFile(const char *name, const char *wavname)
{
	if (!name || !wavname)
		return false;

	char magic[4] = { 0 };
	{
		FILE *f = fl_fopen(name, "rb");
		if (!f)
			return false;

		fread(magic, 4, sizeof(char), f);
		fclose(f);
	}

	FLAC__Metadata_Chain *mc = FLAC__metadata_chain_new();
	if (!mc)
		return false;

	bool init = true;

	if (!strncmp(magic, "fLaC", 4))
	{
#ifdef _WIN32
		if (!FLAC__metadata_chain_read(mc, DemoteStrOS(name).c_str()))
#else
		if (!FLAC__metadata_chain_read(mc, name))
#endif
			init = false;;
	}
	else if (FLAC_API_SUPPORTS_OGG_FLAC)
	{
#ifdef _WIN32
		if (!FLAC__metadata_chain_read_ogg(mc, DemoteStrOS(name).c_str()))
#else
		if (!FLAC__metadata_chain_read_ogg(mc, name))
#endif
			init = false;
	}
	else
		init = false;

	if (!init)
	{
		FLAC__metadata_chain_delete(mc);
		return false;
	}

	FLAC__Metadata_Iterator *mi = FLAC__metadata_iterator_new();
	if (!mi)
	{
		FLAC__metadata_chain_delete(mc);
		return false;
	}

	// streaminfo must always be first
	FLAC__metadata_iterator_init(mi, mc);
	if (FLAC__metadata_iterator_get_block_type(mi) != FLAC__METADATA_TYPE_STREAMINFO)
	{
		FLAC__metadata_chain_delete(mc);
		FLAC__metadata_iterator_delete(mi);
		return false;
	}

	FLAC__StreamMetadata *metadata = FLAC__metadata_iterator_get_block(mi);
	if (metadata->data.stream_info.bits_per_sample < 8
		|| metadata->data.stream_info.bits_per_sample > 32
		|| metadata->data.stream_info.bits_per_sample % 8 != 0)
	{
		FLAC__metadata_chain_delete(mc);
		FLAC__metadata_iterator_delete(mi);
		return false;
	}

	uint32_t ch = metadata->data.stream_info.channels;
	uint32_t sr = metadata->data.stream_info.sample_rate;
	uint32_t bps = metadata->data.stream_info.bits_per_sample;

	FLAC__metadata_chain_delete(mc);
	FLAC__metadata_iterator_delete(mi);

	FLAC__StreamDecoder *dec = FLAC__stream_decoder_new();
	if (!dec)
		return false;

	drwav wav;

	if (!strncmp(magic, "fLaC", 4))
	{
#ifdef _WIN32
		if (FLAC__STREAM_DECODER_INIT_STATUS_OK != FLAC__stream_decoder_init_file(dec, DemoteStrOS(name).c_str(), FlacWriteBuf, NULL, FlacError, &wav))
#else
		if (FLAC__STREAM_DECODER_INIT_STATUS_OK != FLAC__stream_decoder_init_file(dec, name, FlacWriteBuf, NULL, FlacError, &wav))
#endif
			init = false;
	}
	else if (FLAC_API_SUPPORTS_OGG_FLAC)
	{
#ifdef _WIN32
		if (FLAC__STREAM_DECODER_INIT_STATUS_OK != FLAC__stream_decoder_init_ogg_file(dec, DemoteStrOS(name).c_str(), FlacWriteBuf, NULL, FlacError, &wav))
#else
		if (FLAC__STREAM_DECODER_INIT_STATUS_OK != FLAC__stream_decoder_init_ogg_file(dec, name, FlacWriteBuf, NULL, FlacError, &wav))
#endif
			init = false;
	}
	else
		init = false;

	if (!init)
	{
		FLAC__stream_decoder_delete(dec);
		return false;
	}

	drwav_data_format fmt = { drwav_container_riff, DR_WAVE_FORMAT_PCM, ch, sr, bps };
#ifdef _WIN32
	if (DRWAV_TRUE != drwav_init_file_write_w(&wav, WidenStrOS(wavname).c_str(), &fmt, NULL))
#else
	if (DRWAV_TRUE != drwav_init_file_write(&wav, wavname, &fmt, NULL))
#endif
	{
		FLAC__stream_decoder_finish(dec);
		FLAC__stream_decoder_delete(dec);
		return false;
	}

	bool success = FLAC__stream_decoder_process_until_end_of_stream(dec);

	drwav_uninit(&wav);
	FLAC__stream_decoder_finish(dec);
	FLAC__stream_decoder_delete(dec);

	return success;
}

const char* GetFlacVersion()
{
	static char flacver[24] = { 0 };
	if (!*flacver)
	{
		strcpy(flacver, "libFLAC ");
		strncat(flacver, FLAC__VERSION_STRING, sizeof(flacver) - 9);
	}
	return flacver;
}
#endif
