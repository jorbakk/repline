#pragma once
#ifndef RPL_TERM_H
#define RPL_TERM_H

#include "common.h"
#include "tty.h"
#include "stringbuf.h"
#include "attr.h"

struct term_s;
typedef struct term_s term_t;

typedef enum buffer_mode_e {
  UNBUFFERED,
  LINEBUFFERED,
  BUFFERED,
} buffer_mode_t;

// Primitives
rpl_private term_t* term_new(alloc_t* mem, tty_t* tty, bool nocolor, bool silent, int fd_out);
rpl_private void term_free(term_t* term);

rpl_private bool term_is_interactive(const term_t* term);
rpl_private void term_start_raw(term_t* term);
rpl_private void term_end_raw(term_t* term, bool force);

rpl_private bool term_enable_beep(term_t* term, bool enable);
rpl_private bool term_enable_color(term_t* term, bool enable);

rpl_private void term_flush(term_t* term);
rpl_private buffer_mode_t term_set_buffer_mode(term_t* term, buffer_mode_t mode);

rpl_private void term_write_n(term_t* term, const char* s, ssize_t n);
rpl_private void term_write(term_t* term, const char* s);
rpl_private void term_writeln(term_t* term, const char* s);
rpl_private void term_write_char(term_t* term, char c);

rpl_private void term_write_repeat(term_t* term, const char* s, ssize_t count );
rpl_private void term_beep(term_t* term);

rpl_private bool term_update_dim(term_t* term);

rpl_private ssize_t term_get_width(term_t* term);
rpl_private ssize_t term_get_height(term_t* term);
rpl_private int  term_get_color_bits(term_t* term);

// Helpers
rpl_private void term_writef(term_t* term, const char* fmt, ...);
rpl_private void term_vwritef(term_t* term, const char* fmt, va_list args);

rpl_private void term_left(term_t* term, ssize_t n);
rpl_private void term_right(term_t* term, ssize_t n);
rpl_private void term_up(term_t* term, ssize_t n);
rpl_private void term_down(term_t* term, ssize_t n);
rpl_private void term_start_of_line(term_t* term );
rpl_private void term_clear_line(term_t* term);
rpl_private void term_clear_to_end_of_line(term_t* term);
// rpl_private void term_clear_lines_to_end(term_t* term);


rpl_private void term_attr_reset(term_t* term);
rpl_private void term_underline(term_t* term, bool on);
rpl_private void term_reverse(term_t* term, bool on);
rpl_private void term_bold(term_t* term, bool on);
rpl_private void term_italic(term_t* term, bool on);

rpl_private void term_color(term_t* term, rpl_color_t color);
rpl_private void term_bgcolor(term_t* term, rpl_color_t color);

// Formatted output

rpl_private attr_t term_get_attr( const term_t* term );
rpl_private void   term_set_attr( term_t* term, attr_t attr );
rpl_private void   term_write_formatted( term_t* term, const char* s, const attr_t* attrs );
rpl_private void   term_write_formatted_n( term_t* term, const char* s, const attr_t* attrs, ssize_t n );

rpl_private rpl_color_t color_from_ansi256(ssize_t i);

#endif // RPL_TERM_H
