/*
 * Minimal libsndfile replacement for the Emscripten build of FluidSynth: decodes the Ogg Vorbis samples of SF3
 * soundfonts with stb_vorbis. See sndfile.h.
 */
#include "sndfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "stb_vorbis.c"

struct SNDFILE_tag
{
    short *data; /* decoded, interleaved */
    sf_count_t frames;
    int channels;
    sf_count_t pos;
};

static const char *s_error = "no error";

SNDFILE *sf_open_virtual(SF_VIRTUAL_IO *sfvirtual, int mode, SF_INFO *sfinfo, void *user_data)
{
    sf_count_t cb, got = 0;
    unsigned char *buf;
    int channels = 0, rate = 0, frames;
    short *out = NULL;
    SNDFILE *file;

    if (mode != SFM_READ)
    {
        s_error = "only reading is supported";
        return NULL;
    }

    cb = sfvirtual->get_filelen(user_data);
    if (cb <= 0 || cb > 0x7fffffff || (buf = (unsigned char *)malloc((size_t)cb)) == NULL)
    {
        s_error = "bad sample size";
        return NULL;
    }
    sfvirtual->seek(0, SEEK_SET, user_data);
    while (got < cb)
    {
        sf_count_t n = sfvirtual->read(buf + got, cb - got, user_data);
        if (n <= 0)
            break;
        got += n;
    }

    frames = (got == cb) ? stb_vorbis_decode_memory(buf, (int)cb, &channels, &rate, &out) : -1;
    free(buf);
    if (frames < 0 || out == NULL || channels <= 0)
    {
        free(out);
        s_error = "could not decode Ogg Vorbis sample";
        return NULL;
    }

    if ((file = (SNDFILE *)calloc(1, sizeof(SNDFILE))) == NULL)
    {
        free(out);
        s_error = "out of memory";
        return NULL;
    }
    file->data = out;
    file->frames = frames;
    file->channels = channels;

    sfinfo->frames = frames;
    sfinfo->samplerate = rate;
    sfinfo->channels = channels;
    sfinfo->format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
    sfinfo->sections = 1;
    sfinfo->seekable = 1;
    return file;
}

sf_count_t sf_readf_short(SNDFILE *sndfile, short *ptr, sf_count_t frames)
{
    sf_count_t left = sndfile->frames - sndfile->pos;
    if (frames > left)
        frames = left;
    if (frames <= 0)
        return 0;
    memcpy(ptr, sndfile->data + sndfile->pos * sndfile->channels, (size_t)(frames * sndfile->channels) * sizeof(short));
    sndfile->pos += frames;
    return frames;
}

int sf_close(SNDFILE *sndfile)
{
    if (sndfile != NULL)
    {
        free(sndfile->data);
        free(sndfile);
    }
    return 0;
}

int sf_command(SNDFILE *sndfile, int command, void *data, int datasize)
{
    (void)sndfile;
    (void)datasize;
    if ((command == SFC_GET_FORMAT_MAJOR_COUNT || command == SFC_GET_FORMAT_SUBTYPE_COUNT) && data != NULL)
        *(int *)data = 0; /* no file formats to write */
    return 0;
}

const char *sf_strerror(SNDFILE *sndfile)
{
    (void)sndfile;
    return s_error;
}

const char *sf_error_number(int errnum)
{
    (void)errnum;
    return s_error;
}

/* Writing files (FluidSynth's file renderer) is not supported */
SNDFILE *sf_open(const char *path, int mode, SF_INFO *sfinfo)
{
    (void)path;
    (void)mode;
    (void)sfinfo;
    s_error = "file rendering is not supported";
    return NULL;
}

int sf_format_check(const SF_INFO *info)
{
    (void)info;
    return SF_FALSE;
}

sf_count_t sf_writef_float(SNDFILE *sndfile, const float *ptr, sf_count_t frames)
{
    (void)sndfile;
    (void)ptr;
    (void)frames;
    return 0;
}

sf_count_t sf_writef_short(SNDFILE *sndfile, const short *ptr, sf_count_t frames)
{
    (void)sndfile;
    (void)ptr;
    (void)frames;
    return 0;
}
