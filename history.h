#pragma once
#ifndef RPL_HISTORY_H
#define RPL_HISTORY_H

#include "common.h"

//-------------------------------------------------------------
// History
//-------------------------------------------------------------

struct history_s;
typedef struct history_s history_t;

/// Private API
rpl_private history_t*  history_new(alloc_t* mem);
rpl_private void        history_free(history_t* h);
rpl_private void        history_save(const history_t* h);
rpl_private ssize_t     history_count_with_prefix(const history_t* h, const char *prefix);
rpl_private const char* history_get_with_prefix(const history_t* h, ssize_t n, const char* prefix);

/// Called from public repline API:
rpl_private void        history_clear(history_t* h);
rpl_private bool        history_enable_duplicates(history_t* h, bool enable);
rpl_private void        history_load_from(history_t* h, const char* fname, long max_entries);
rpl_private bool        history_push(history_t* h, const char* entry);
rpl_private void        history_remove_last(history_t* h);

#endif // RPL_HISTORY_H
