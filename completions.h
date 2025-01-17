#pragma once
#ifndef RPL_COMPLETIONS_H
#define RPL_COMPLETIONS_H

#include "common.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// Completions
//-------------------------------------------------------------
#define RPL_MAX_COMPLETIONS_TO_SHOW  (1000)
#define RPL_MAX_COMPLETIONS_TO_TRY   (RPL_MAX_COMPLETIONS_TO_SHOW/4)

typedef struct completions_s completions_t;
typedef struct editor_s editor_t;

rpl_private completions_t *completions(alloc_t * mem);
rpl_private void completions_free(completions_t * cms);
rpl_private void completions_clear(completions_t * cms);
rpl_private bool completions_add(completions_t * cms, const char *replacement,
                                 const char *display, const char *help);
rpl_private ssize_t completions_count(completions_t * cms);
rpl_private void completions_generate(struct rpl_env_s *env,
                                         editor_t *eb,
                                         ssize_t max);
rpl_private void completions_sort(completions_t * cms);
// rpl_private void completions_set_completer(completions_t * cms,
                                           // rpl_completer_fun_t * completer,
                                           // void *arg);
rpl_private const char *completions_get_display(completions_t * cms,
                                                ssize_t index,
                                                const char **help);
rpl_private const char *completions_get_hint(completions_t * cms, ssize_t index,
                                             const char **help);
// rpl_private void completions_get_completer(completions_t * cms,
                                           // rpl_completer_fun_t ** completer,
                                           // void **arg);

rpl_private ssize_t completions_apply(completions_t * cms, ssize_t index,
                                      stringbuf_t * sbuf, ssize_t pos);

//-------------------------------------------------------------
// Completion environment
//-------------------------------------------------------------
typedef bool (rpl_completion_fun_t) (rpl_env_t * env, void *funenv,
                                     const char *replacement,
                                     const char *display, const char *help,
                                     long delete_before, long delete_after);

struct rpl_completion_env_s {
	rpl_env_t *env;             // the repline environment
	const char *input;          // current full input
	long cursor;                // current cursor position
	void *arg;                  // argument given to `rpl_set_completer`
	void *closure;              // free variables for function composition
	rpl_completion_fun_t *complete; // function that adds a completion
};

#endif                          // RPL_COMPLETIONS_H
