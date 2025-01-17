//-------------------------------------------------------------
// Usually we include all sources one file so no internal 
// symbols are public in the libray.
// 
// You can compile the entire library just as: 
// $ gcc -c src/repline.c 
//-------------------------------------------------------------
#if !defined(RPL_SEPARATE_OBJS)
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS    // for msvc
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS // for msvc
#endif
#define _XOPEN_SOURCE   700     // for wcwidth
#define _DEFAULT_SOURCE         // ensure usleep stays visible with _XOPEN_SOURCE >= 700
#include "attr.c"
#include "bbcode.c"
#include "editline.c"
#include "highlight.c"
#include "undo.c"
#ifdef RPL_HIST_IMPL_SQLITE
#include "history_sqlite.c"
#else
#include "history.c"
#endif
#include "completers.c"
#include "completions.c"
#include "term.c"
#include "tty_esc.c"
#include "tty.c"
#include "stringbuf.c"
#include "common.c"
#endif

//-------------------------------------------------------------
// includes
//-------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "repline.h"
#include "common.h"
#include "env.h"

//-------------------------------------------------------------
// Readline
//-------------------------------------------------------------

static char *rpl_getline(alloc_t * mem);

rpl_public char *
rpl_readline(const char *prompt_text)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	if (!env->noedit) {
		// terminal editing enabled
		return rpl_editline(env, prompt_text);  // in editline.c
	} else {
		// no editing capability (pipe, dumb terminal, etc)
		if (env->tty != NULL && env->term != NULL) {
			// if the terminal is not interactive, but we are reading from the tty (keyboard), we display a prompt
			term_start_raw(env->term);  // set utf8 mode on windows
			if (prompt_text != NULL) {
				term_write(env->term, prompt_text);
			}
			term_write(env->term, env->prompt_marker);
			term_end_raw(env->term, false);
		}
		// read directly from stdin
		return rpl_getline(env->mem);
	}
}

//-------------------------------------------------------------
// Read a line from the stdin stream if there is no editing 
// support (like from a pipe, file, or dumb terminal).
//-------------------------------------------------------------

static char *
rpl_getline(alloc_t * mem)
{
	// read until eof or newline
	stringbuf_t *sb = sbuf_new(mem);
	int c;
	while (true) {
		c = fgetc(stdin);
		if (c == EOF || c == '\n') {
			break;
		} else {
			sbuf_append_char(sb, (char)c);
		}
	}
	return sbuf_free_dup(sb);
}

//-------------------------------------------------------------
// Formatted output
//-------------------------------------------------------------

rpl_public void
rpl_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	rpl_vprintf(fmt, ap);
	va_end(ap);
}

rpl_public void
rpl_vprintf(const char *fmt, va_list args)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_vprintf(env->bbcode, fmt, args);
}

rpl_public void
rpl_print(const char *s)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_print(env->bbcode, s);
}

rpl_public void
rpl_println(const char *s)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_println(env->bbcode, s);
}

void
rpl_style_def(const char *name, const char *fmt)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_style_def(env->bbcode, name, fmt);
}

void
rpl_style_open(const char *fmt)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_style_open(env->bbcode, fmt);
}

void
rpl_style_close(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->bbcode == NULL)
		return;
	bbcode_style_close(env->bbcode, NULL);
}

//-------------------------------------------------------------
// Interface
//-------------------------------------------------------------

rpl_public bool
rpl_async_stop(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	if (env->tty == NULL)
		return false;
	return tty_async_stop(env->tty);
}

static void
set_prompt_marker(rpl_env_t * env, const char *prompt_marker,
                  const char *cprompt_marker)
{
	if (prompt_marker == NULL)
		prompt_marker = "> ";
	if (cprompt_marker == NULL)
		cprompt_marker = prompt_marker;
	mem_free(env->mem, env->prompt_marker);
	mem_free(env->mem, env->cprompt_marker);
	env->prompt_marker = mem_strdup(env->mem, prompt_marker);
	env->cprompt_marker = mem_strdup(env->mem, cprompt_marker);
}

rpl_public const char *
rpl_get_prompt_marker(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	return env->prompt_marker;
}

rpl_public const char *
rpl_get_continuation_prompt_marker(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	return env->cprompt_marker;
}

rpl_public void
rpl_set_prompt_marker(const char *prompt_marker, const char *cprompt_marker)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	set_prompt_marker(env, prompt_marker, cprompt_marker);
}

rpl_public bool
rpl_enable_twoline_prompt(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->twoline_prompt;
	env->twoline_prompt = enable;
	return !prev;
}

rpl_public bool
rpl_enable_multiline(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->singleline_only;
	env->singleline_only = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_beep(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	return term_enable_beep(env->term, enable);
}

rpl_public bool
rpl_enable_color(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	return term_enable_color(env->term, enable);
}

rpl_public void
rpl_set_history(const char *fname, long max_entries)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	history_load_from(env->history, fname, max_entries);
}

rpl_public void
rpl_history_remove_last(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	history_remove_last(env->history);
}

rpl_public void
rpl_history_add(const char *entry)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	history_push(env->history, entry);
}

rpl_public void
rpl_history_clear(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	history_clear(env->history);
}

rpl_public void
rpl_history_close(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	history_close(env->history);
}

rpl_public bool
rpl_enable_completion_preview(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->complete_nopreview;
	env->complete_nopreview = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_completion_always_quote(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->complete_noquote;
	env->complete_noquote = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_multiline_indent(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->no_multiline_indent;
	env->no_multiline_indent = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_hint(bool enable)
{
#ifndef RPL_HIST_IMPL_SQLITE
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->no_hint;
	env->no_hint = !enable;
	return !prev;
#else
	rpl_unused(enable);
	return false;
#endif
}

rpl_public long
rpl_set_hint_delay(long delay_ms)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	long prev = env->hint_delay;
	env->hint_delay = (delay_ms < 0 ? 0 : (delay_ms > 5000 ? 5000 : delay_ms));
	return prev;
}

rpl_public void
rpl_set_tty_esc_delay(long initial_delay_ms, long followup_delay_ms)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->tty == NULL)
		return;
	tty_set_esc_delay(env->tty, initial_delay_ms, followup_delay_ms);
}

rpl_public bool
rpl_enable_highlight(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->no_highlight;
	env->no_highlight = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_inline_help(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->no_help;
	env->no_help = !enable;
	return !prev;
}

rpl_public bool
rpl_enable_brace_matching(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->no_bracematch;
	env->no_bracematch = !enable;
	return !prev;
}

rpl_public void
rpl_set_matching_braces(const char *brace_pairs)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	mem_free(env->mem, env->match_braces);
	env->match_braces = NULL;
	if (brace_pairs != NULL) {
		ssize_t len = rpl_strlen(brace_pairs);
		if (len > 0 && (len % 2) == 0) {
			env->match_braces = mem_strdup(env->mem, brace_pairs);
		}
	}
}

rpl_public bool
rpl_enable_brace_insertion(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return false;
	bool prev = env->autobrace;
	env->autobrace = enable;
	return prev;
}

rpl_public void
rpl_set_insertion_braces(const char *brace_pairs)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	mem_free(env->mem, env->auto_braces);
	env->auto_braces = NULL;
	if (brace_pairs != NULL) {
		ssize_t len = rpl_strlen(brace_pairs);
		if (len > 0 && (len % 2) == 0) {
			env->auto_braces = mem_strdup(env->mem, brace_pairs);
		}
	}
}

rpl_private const char *
rpl_env_get_match_braces(rpl_env_t * env)
{
	return (env->match_braces == NULL ? "()[]{}" : env->match_braces);
}

rpl_private const char *
rpl_env_get_auto_braces(rpl_env_t * env)
{
	return (env->auto_braces == NULL ? "()[]{}\"\"''" : env->auto_braces);
}

rpl_public void
rpl_set_default_highlighter(rpl_highlight_fun_t * highlighter, void *arg)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	env->highlighter = highlighter;
	env->highlighter_arg = arg;
}

rpl_public void
rpl_free(void *p)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	mem_free(env->mem, p);
}

rpl_public void *
rpl_malloc(size_t sz)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	return mem_malloc(env->mem, to_ssize_t(sz));
}

rpl_public const char *
rpl_strdup(const char *s)
{
	if (s == NULL)
		return NULL;
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	ssize_t len = rpl_strlen(s);
	char *p = mem_malloc_tp_n(env->mem, char, len + 1);
	if (p == NULL)
		return NULL;
	rpl_memcpy(p, s, len);
	p[len] = 0;
	return p;
}

//-------------------------------------------------------------
// Terminal
//-------------------------------------------------------------

rpl_public void
rpl_term_init(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_start_raw(env->term);
}

rpl_public void
rpl_term_done(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_end_raw(env->term, false);
}

rpl_public void
rpl_term_flush(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_flush(env->term);
}

rpl_public void
rpl_term_write(const char *s)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_write(env->term, s);
}

rpl_public void
rpl_term_writeln(const char *s)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_writeln(env->term, s);
}

rpl_public void
rpl_term_writef(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	rpl_term_vwritef(fmt, ap);
	va_end(ap);
}

rpl_public void
rpl_term_vwritef(const char *fmt, va_list args)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_vwritef(env->term, fmt, args);
}

rpl_public void
rpl_term_reset(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL)
		return;
	term_attr_reset(env->term);
}

rpl_public void
rpl_term_style(const char *style)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	if (env->term == NULL || env->bbcode == NULL)
		return;
	term_set_attr(env->term, bbcode_style(env->bbcode, style));
}

rpl_public int
rpl_term_get_color_bits(void)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return 4;
	return term_get_color_bits(env->term);
}

rpl_public void
rpl_term_bold(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	term_bold(env->term, enable);
}

rpl_public void
rpl_term_underline(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	term_underline(env->term, enable);
}

rpl_public void
rpl_term_italic(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	term_italic(env->term, enable);
}

rpl_public void
rpl_term_reverse(bool enable)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	term_reverse(env->term, enable);
}

rpl_public void
rpl_term_color_ansi(bool foreground, int ansi_color)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	rpl_color_t color = color_from_ansi256(ansi_color);
	if (foreground) {
		term_color(env->term, color);
	} else {
		term_bgcolor(env->term, color);
	}
}

rpl_public void
rpl_term_color_rgb(bool foreground, uint32_t hcolor)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL || env->term == NULL)
		return;
	rpl_color_t color = rpl_rgb(hcolor);
	if (foreground) {
		term_color(env->term, color);
	} else {
		term_bgcolor(env->term, color);
	}
}

//-------------------------------------------------------------
// Readline with temporary completer and highlighter
//-------------------------------------------------------------

#if 0
rpl_public char *
rpl_readline_ex(const char *prompt_text,
                rpl_completer_fun_t * completer, void *completer_arg,
                rpl_highlight_fun_t * highlighter, void *highlighter_arg)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return NULL;
	// save previous
	rpl_completer_fun_t *prev_completer;
	void *prev_completer_arg;
	completions_get_completer(env->completions, &prev_completer,
	                          &prev_completer_arg);
	rpl_highlight_fun_t *prev_highlighter = env->highlighter;
	void *prev_highlighter_arg = env->highlighter_arg;
	// call with current
	if (completer != NULL) {
		rpl_set_default_completer(completer, completer_arg);
	}
	if (highlighter != NULL) {
		rpl_set_default_highlighter(highlighter, highlighter_arg);
	}
	char *res = rpl_readline(prompt_text);
	// restore previous
	rpl_set_default_completer(prev_completer, prev_completer_arg);
	rpl_set_default_highlighter(prev_highlighter, prev_highlighter_arg);
	return res;
}
#endif

//-------------------------------------------------------------
// Initialize
//-------------------------------------------------------------

static void rpl_atexit(void);

static void
rpl_env_free(rpl_env_t * env)
{
	if (env == NULL)
		return;
	history_save(env->history);
	history_free(env->history);
	completions_free(env->completions);
	bbcode_free(env->bbcode);
	term_free(env->term);
	tty_free(env->tty);
	mem_free(env->mem, env->cprompt_marker);
	mem_free(env->mem, env->prompt_marker);
	mem_free(env->mem, env->match_braces);
	mem_free(env->mem, env->auto_braces);
	env->prompt_marker = NULL;

	// and deallocate ourselves
	alloc_t *mem = env->mem;
	mem_free(mem, env);

	// and finally the custom memory allocation structure
	mem_free(mem, mem);
}

static rpl_env_t *
rpl_env_create(rpl_malloc_fun_t * _malloc, rpl_realloc_fun_t * _realloc,
               rpl_free_fun_t * _free)
{
	if (_malloc == NULL)
		_malloc = &malloc;
	if (_realloc == NULL)
		_realloc = &realloc;
	if (_free == NULL)
		_free = &free;
	// allocate
	alloc_t *mem = (alloc_t *) _malloc(sizeof(alloc_t));
	if (mem == NULL)
		return NULL;
	mem->malloc = _malloc;
	mem->realloc = _realloc;
	mem->free = _free;
	rpl_env_t *env = mem_zalloc_tp(mem, rpl_env_t);
	if (env == NULL) {
		mem->free(mem);
		return NULL;
	}
	env->mem = mem;

	// Initialize
	env->tty = tty_new(env->mem, -1);   // can return NULL
	env->term = term_new(env->mem, env->tty, false, false, -1);
	env->history = history_new(env->mem);
	env->completions = completions_new(env->mem);
	env->bbcode = bbcode_new(env->mem, env->term);
#ifndef RPL_HIST_IMPL_SQLITE
	env->hint_delay = 400;
#endif

	if (env->tty == NULL || env->term == NULL ||
	    env->completions == NULL || env->history == NULL || env->bbcode == NULL
	    || !term_is_interactive(env->term)) {
		env->noedit = true;
	}
	env->multiline_eol = '\\';

	bbcode_style_def(env->bbcode, "rpl-prompt", "ansi-green");
	bbcode_style_def(env->bbcode, "rpl-info", "ansi-darkgray");
	bbcode_style_def(env->bbcode, "rpl-diminish", "ansi-lightgray");
	bbcode_style_def(env->bbcode, "rpl-emphasis", "#ffffd7");
	bbcode_style_def(env->bbcode, "rpl-hint", "ansi-darkgray");
	bbcode_style_def(env->bbcode, "rpl-error", "#d70000");
	bbcode_style_def(env->bbcode, "rpl-bracematch", "ansi-white");  //  color = #F7DC6F" );

	bbcode_style_def(env->bbcode, "keyword", "#569cd6");
	bbcode_style_def(env->bbcode, "control", "#c586c0");
	bbcode_style_def(env->bbcode, "number", "#b5cea8");
	bbcode_style_def(env->bbcode, "string", "#ce9178");
	bbcode_style_def(env->bbcode, "comment", "#6A9955");
	bbcode_style_def(env->bbcode, "type", "darkcyan");
	bbcode_style_def(env->bbcode, "constant", "#569cd6");

	set_prompt_marker(env, NULL, NULL);
	return env;
}

static rpl_env_t *rpenv;

static void
rpl_atexit(void)
{
	if (rpenv != NULL) {
		rpl_env_free(rpenv);
		rpenv = NULL;
	}
}

rpl_private rpl_env_t *
rpl_get_env(void)
{
	if (rpenv == NULL) {
		rpenv = rpl_env_create(NULL, NULL, NULL);
		if (rpenv != NULL) {
			atexit(&rpl_atexit);
		}
	}
	return rpenv;
}

rpl_public void
rpl_init_custom_malloc(rpl_malloc_fun_t * _malloc, rpl_realloc_fun_t * _realloc,
                       rpl_free_fun_t * _free)
{
	assert(rpenv == NULL);
	if (rpenv != NULL) {
		rpl_env_free(rpenv);
		rpenv = rpl_env_create(_malloc, _realloc, _free);
	} else {
		rpenv = rpl_env_create(_malloc, _realloc, _free);
		if (rpenv != NULL) {
			atexit(&rpl_atexit);
		}
	}
}
