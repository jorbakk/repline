#pragma once
#ifndef RPL_COMMON_H
#define RPL_COMMON_H

//-------------------------------------------------------------
// Headers and defines
//-------------------------------------------------------------

#include <sys/types.h>          // ssize_t
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "repline.h"            // rpl_malloc_fun_t, rpl_color_t etc.

#ifdef __cplusplus
#define rpl_extern_c   extern "C"
#else
#define rpl_extern_c
#endif

#if defined(RPL_SEPARATE_OBJS)
#define rpl_public     rpl_extern_c
#if defined(__GNUC__)           // includes clang and icc
#define rpl_private    __attribute__((visibility("hidden")))
#else
#define rpl_private
#endif
#else
#define rpl_private     static
#define rpl_public      rpl_extern_c
#endif

#define rpl_unused(x)    (void)(x)

//-------------------------------------------------------------
// ssize_t
//-------------------------------------------------------------

#if defined(_MSC_VER)
typedef intptr_t ssize_t;
#endif

#define ssizeof(tp)   (ssize_t)(sizeof(tp))
static inline size_t
to_size_t(ssize_t sz)
{
	return (sz >= 0 ? (size_t)sz : 0);
}

static inline ssize_t
to_ssize_t(size_t sz)
{
	return (sz <= SIZE_MAX / 2 ? (ssize_t) sz : 0);
}

rpl_private void rpl_memmove(void *dest, const void *src, ssize_t n);
rpl_private void rpl_memcpy(void *dest, const void *src, ssize_t n);
rpl_private void rpl_memset(void *dest, uint8_t value, ssize_t n);
rpl_private bool rpl_memnmove(void *dest, ssize_t dest_size, const void *src,
                              ssize_t n);

rpl_private ssize_t rpl_strlen(const char *s);
rpl_private bool rpl_strcpy(char *dest, ssize_t dest_size /* including 0 */ ,
                            const char *src);
rpl_private bool rpl_strncpy(char *dest, ssize_t dest_size /* including 0 */ ,
                             const char *src, ssize_t n);

rpl_private bool rpl_contains(const char *big, const char *s);
rpl_private bool rpl_icontains(const char *big, const char *s);
rpl_private char rpl_tolower(char c);
rpl_private void rpl_str_tolower(char *s);
rpl_private int rpl_stricmp(const char *s1, const char *s2);
rpl_private int rpl_strnicmp(const char *s1, const char *s2, ssize_t n);

//---------------------------------------------------------------------
// Unicode
//
// We use "qutf-8" (quite like utf-8) encoding and decoding. 
// Internally we always use valid utf-8. If we encounter invalid
// utf-8 bytes (or bytes >= 0x80 from any other encoding) we encode
// these as special code points in the "raw plane" (0xEE000 - 0xEE0FF).
// When decoding we are then able to restore such raw bytes as-is.
// See <https://github.com/koka-lang/koka/blob/master/kklib/include/kklib/string.h>
//---------------------------------------------------------------------

typedef uint32_t unicode_t;

rpl_private void unicode_to_qutf8(unicode_t u, uint8_t buf[5]);
rpl_private unicode_t unicode_from_qutf8(const uint8_t * s, ssize_t len, ssize_t * nread);  // validating

rpl_private unicode_t unicode_from_raw(uint8_t c);
rpl_private bool unicode_is_raw(unicode_t u, uint8_t * c);

rpl_private bool utf8_is_cont(uint8_t c);

//-------------------------------------------------------------
// Colors
//-------------------------------------------------------------

// A color is either RGB or an ANSI code.
// (RGB colors have bit 24 set to distinguish them from the ANSI color palette colors.)
// (Repline will automatically convert from RGB on terminals that do not support full colors)
typedef uint32_t rpl_color_t;

// Create a color from a 24-bit color value.
rpl_private rpl_color_t rpl_rgb(uint32_t hex);

// Create a color from a 8-bit red/green/blue components.
// The value of each component is capped between 0 and 255.
rpl_private rpl_color_t rpl_rgbx(ssize_t r, ssize_t g, ssize_t b);

#define RPL_COLOR_NONE     (0)
#define RPL_RGB(rgb)       (0x1000000 | (uint32_t)(rgb))    // rpl_rgb(rgb)  // define to it can be used as a constant

// ANSI colors.
// The actual colors used is usually determined by the terminal theme
// See <https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit>
#define RPL_ANSI_BLACK     (30)
#define RPL_ANSI_MAROON    (31)
#define RPL_ANSI_GREEN     (32)
#define RPL_ANSI_OLIVE     (33)
#define RPL_ANSI_NAVY      (34)
#define RPL_ANSI_PURPLE    (35)
#define RPL_ANSI_TEAL      (36)
#define RPL_ANSI_SILVER    (37)
#define RPL_ANSI_DEFAULT   (39)

#define RPL_ANSI_GRAY      (90)
#define RPL_ANSI_RED       (91)
#define RPL_ANSI_LIME      (92)
#define RPL_ANSI_YELLOW    (93)
#define RPL_ANSI_BLUE      (94)
#define RPL_ANSI_FUCHSIA   (95)
#define RPL_ANSI_AQUA      (96)
#define RPL_ANSI_WHITE     (97)

#define RPL_ANSI_DARKGRAY  RPL_ANSI_GRAY
#define RPL_ANSI_LIGHTGRAY RPL_ANSI_SILVER
#define RPL_ANSI_MAGENTA   RPL_ANSI_FUCHSIA
#define RPL_ANSI_CYAN      RPL_ANSI_AQUA

//-------------------------------------------------------------
// Debug
//-------------------------------------------------------------

#if defined(RPL_NO_DEBUG_MSG)
#define debug_msg(fmt,...)   (void)(0)
#else
rpl_private void debug_msg(const char *fmt, ...);
#endif

//-------------------------------------------------------------
// Abstract environment
//-------------------------------------------------------------
struct rpl_env_s;
typedef struct rpl_env_s rpl_env_t;

//-------------------------------------------------------------
// Allocation
//-------------------------------------------------------------

typedef struct alloc_s {
	rpl_malloc_fun_t *malloc;
	rpl_realloc_fun_t *realloc;
	rpl_free_fun_t *free;
} alloc_t;

rpl_private void *mem_malloc(alloc_t * mem, ssize_t sz);
rpl_private void *mem_zalloc(alloc_t * mem, ssize_t sz);
rpl_private void *mem_realloc(alloc_t * mem, void *p, ssize_t newsz);
rpl_private void mem_free(alloc_t * mem, const void *p);
rpl_private char *mem_strdup(alloc_t * mem, const char *s);
rpl_private char *mem_strndup(alloc_t * mem, const char *s, ssize_t n);

#define mem_zalloc_tp(mem,tp)        (tp*)mem_zalloc(mem,ssizeof(tp))
#define mem_malloc_tp_n(mem,tp,n)    (tp*)mem_malloc(mem,(n)*ssizeof(tp))
#define mem_zalloc_tp_n(mem,tp,n)    (tp*)mem_zalloc(mem,(n)*ssizeof(tp))
#define mem_realloc_tp(mem,tp,p,n)   (tp*)mem_realloc(mem,p,(n)*ssizeof(tp))

#endif                          // RPL_COMMON_H
