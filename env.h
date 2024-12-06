#pragma once
#ifndef RPL_ENV_H
#define RPL_ENV_H

#include "repline.h"
#include "common.h"
#include "term.h"
#include "tty.h"
#include "stringbuf.h"
#include "history.h"
#include "completions.h"
#include "bbcode.h"

//-------------------------------------------------------------
// Environment
//-------------------------------------------------------------

struct rpl_env_s {
	alloc_t *mem;               // potential custom allocator
	rpl_env_t *next;            // next environment (used for proper deallocation)
	term_t *term;               // terminal
	tty_t *tty;                 // keyboard (NULL if stdin is a pipe, file, etc)
	completions_t *completions; // current completions
	history_t *history;         // edit history
	bbcode_t *bbcode;           // print with bbcodes
	const char *prompt_marker;  // the prompt marker (defaults to "> ")
	const char *cprompt_marker; // prompt marker for continuation lines (defaults to `prompt_marker`)
	rpl_highlight_fun_t *highlighter;   // highlight callback
	void *highlighter_arg;      // user state for the highlighter.
	const char *match_braces;   // matching braces, e.g "()[]{}"
	const char *auto_braces;    // auto insertion braces, e.g "()[]{}\"\"''"
	char multiline_eol;         // character used for multiline input ("\") (set to 0 to disable)
	bool initialized;           // are we initialized?
	bool noedit;                // is rich editing possible (tty != NULL)
	bool twoline_prompt;        // print marker on a separate line
	bool singleline_only;       // allow only single line editing?
	bool complete_nopreview;    // do not show completion preview for each selection in the completion menu?
	bool complete_autotab;      // try to keep completing after a completion?
	bool complete_noquote;      // don't quote (file) names but escape single characters
	bool no_multiline_indent;   // indent continuation lines to line up under the initial prompt 
	bool no_help;               // show short help line for history search etc.
	bool no_hint;               // allow hinting?
	bool no_highlight;          // enable highlighting?
	bool no_bracematch;         // enable brace matching?
	bool no_autobrace;          // enable automatic brace insertion?
	bool no_lscolors;           // use LSCOLORS/LS_COLORS to colorize file name completions?
	long hint_delay;            // delay before displaying a hint in milliseconds
};

rpl_private char *rpl_editline(rpl_env_t * env, const char *prompt_text);

rpl_private rpl_env_t *rpl_get_env(void);
rpl_private const char *rpl_env_get_auto_braces(rpl_env_t * env);
rpl_private const char *rpl_env_get_match_braces(rpl_env_t * env);

#endif                          // RPL_ENV_H
