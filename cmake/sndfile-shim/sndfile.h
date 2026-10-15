/*
 * Minimal libsndfile replacement for the Emscripten build of FluidSynth.
 *
 * FluidSynth decodes the Ogg Vorbis samples of SF3 soundfonts through libsndfile (sf_open_virtual +
 * sf_readf_short). This shim implements exactly that with stb_vorbis, and stubs the rest of the API FluidSynth
 * references (file rendering), which the app never uses.
 */
#ifndef SNDFILE_SHIM_H
#define SNDFILE_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t sf_count_t;
typedef struct SNDFILE_tag SNDFILE;

typedef struct SF_INFO
{
    sf_count_t frames;
    int samplerate;
    int channels;
    int format;
    int sections;
    int seekable;
} SF_INFO;

typedef struct SF_FORMAT_INFO
{
    int format;
    const char *name;
    const char *extension;
} SF_FORMAT_INFO;

typedef sf_count_t (*sf_vio_get_filelen)(void *user_data);
typedef sf_count_t (*sf_vio_seek)(sf_count_t offset, int whence, void *user_data);
typedef sf_count_t (*sf_vio_read)(void *ptr, sf_count_t count, void *user_data);
typedef sf_count_t (*sf_vio_write)(const void *ptr, sf_count_t count, void *user_data);
typedef sf_count_t (*sf_vio_tell)(void *user_data);

typedef struct SF_VIRTUAL_IO
{
    sf_vio_get_filelen get_filelen;
    sf_vio_seek seek;
    sf_vio_read read;
    sf_vio_write write;
    sf_vio_tell tell;
} SF_VIRTUAL_IO;

enum
{
    SF_FALSE = 0,
    SF_TRUE = 1,

    SFM_READ = 0x10,
    SFM_WRITE = 0x20,
    SFM_RDWR = 0x30,

    SF_FORMAT_WAV = 0x010000,
    SF_FORMAT_OGG = 0x200000,
    SF_FORMAT_PCM_S8 = 0x0001,
    SF_FORMAT_PCM_16 = 0x0002,
    SF_FORMAT_PCM_24 = 0x0003,
    SF_FORMAT_PCM_32 = 0x0004,
    SF_FORMAT_PCM_U8 = 0x0005,
    SF_FORMAT_FLOAT = 0x0006,
    SF_FORMAT_DOUBLE = 0x0007,
    SF_FORMAT_VORBIS = 0x0060,

    SF_ENDIAN_FILE = 0x00000000,
    SF_ENDIAN_LITTLE = 0x10000000,
    SF_ENDIAN_BIG = 0x20000000,
    SF_ENDIAN_CPU = 0x30000000,

    SF_FORMAT_SUBMASK = 0x0000FFFF,
    SF_FORMAT_TYPEMASK = 0x0FFF0000,
    SF_FORMAT_ENDMASK = 0x30000000,

    SFC_SET_NORM_FLOAT = 0x1002,
    SFC_SET_SCALE_FLOAT_INT_READ = 0x1014,
    SFC_GET_FORMAT_MAJOR_COUNT = 0x1030,
    SFC_GET_FORMAT_MAJOR = 0x1031,
    SFC_GET_FORMAT_SUBTYPE_COUNT = 0x1032,
    SFC_GET_FORMAT_SUBTYPE = 0x1033,
    SFC_SET_CLIPPING = 0x10C0,
    SFC_SET_VBR_ENCODING_QUALITY = 0x1300,
};

SNDFILE *sf_open(const char *path, int mode, SF_INFO *sfinfo);
SNDFILE *sf_open_virtual(SF_VIRTUAL_IO *sfvirtual, int mode, SF_INFO *sfinfo, void *user_data);
sf_count_t sf_readf_short(SNDFILE *sndfile, short *ptr, sf_count_t frames);
sf_count_t sf_writef_float(SNDFILE *sndfile, const float *ptr, sf_count_t frames);
sf_count_t sf_writef_short(SNDFILE *sndfile, const short *ptr, sf_count_t frames);
int sf_command(SNDFILE *sndfile, int command, void *data, int datasize);
int sf_format_check(const SF_INFO *info);
int sf_close(SNDFILE *sndfile);
const char *sf_strerror(SNDFILE *sndfile);
const char *sf_error_number(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* SNDFILE_SHIM_H */
