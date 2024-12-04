#pragma once
#ifndef RPL_BBCODE_H
#define RPL_BBCODE_H

#include <stdarg.h>
#include "common.h"
#include "term.h"

struct bbcode_s;
typedef struct bbcode_s bbcode_t;

rpl_private bbcode_t *bbcode_new(alloc_t * mem, term_t * term);
rpl_private void bbcode_free(bbcode_t * bb);

rpl_private void bbcode_style_add(bbcode_t * bb, const char *style_name,
                                  attr_t attr);
rpl_private void bbcode_style_def(bbcode_t * bb, const char *style_name,
                                  const char *s);
rpl_private void bbcode_style_open(bbcode_t * bb, const char *fmt);
rpl_private void bbcode_style_close(bbcode_t * bb, const char *fmt);
rpl_private attr_t bbcode_style(bbcode_t * bb, const char *style_name);

rpl_private void bbcode_print(bbcode_t * bb, const char *s);
rpl_private void bbcode_println(bbcode_t * bb, const char *s);
rpl_private void bbcode_printf(bbcode_t * bb, const char *fmt, ...);
rpl_private void bbcode_vprintf(bbcode_t * bb, const char *fmt, va_list args);

rpl_private ssize_t bbcode_column_width(bbcode_t * bb, const char *s);

// allows `attr_out == NULL`.
rpl_private void bbcode_append(bbcode_t * bb, const char *s, stringbuf_t * out,
                               attrbuf_t * attr_out);

#endif                          // RPL_BBCODE_H
