#pragma once
#ifndef RPL_HIGHLIGHT_H
#define RPL_HIGHLIGHT_H

#include "common.h"
#include "attr.h"
#include "term.h"
#include "bbcode.h"

//-------------------------------------------------------------
// Syntax highlighting
//-------------------------------------------------------------

rpl_private void highlight( alloc_t* mem, bbcode_t* bb, const char* s, attrbuf_t* attrs, rpl_highlight_fun_t* highlighter, void* arg );
rpl_private void highlight_match_braces(const char* s, attrbuf_t* attrs, ssize_t cursor_pos, const char* braces, attr_t match_attr, attr_t error_attr);
rpl_private ssize_t find_matching_brace(const char* s, ssize_t cursor_pos, const char* braces, bool* is_balanced);

#endif // RPL_HIGHLIGHT_H
