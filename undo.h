#pragma once
#ifndef RPL_UNDO_H
#define RPL_UNDO_H

#include "common.h"

//-------------------------------------------------------------
// Edit state
//-------------------------------------------------------------
struct editstate_s;
typedef struct editstate_s editstate_t;

rpl_private void editstate_init(editstate_t ** es);
rpl_private void editstate_done(alloc_t * mem, editstate_t ** es);
rpl_private void editstate_capture(alloc_t * mem, editstate_t ** es,
                                   const char *input, ssize_t pos);
rpl_private bool editstate_restore(alloc_t * mem, editstate_t ** es, const char **input, ssize_t * pos);    // caller needs to free input

#endif                          // RPL_UNDO_H
