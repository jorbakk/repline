#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "repline.h"
#include "common.h"
#include "env.h"
#include "stringbuf.h"
#include "completions.h"


//-------------------------------------------------------------
// Completions
//-------------------------------------------------------------

typedef struct completion_s {
  const char* replacement;
  const char* display;
  const char* help;
  ssize_t     delete_before;
  ssize_t     delete_after;
} completion_t;

struct completions_s {
  rpl_completer_fun_t* completer;
  void* completer_arg;
  ssize_t completer_max;
  ssize_t count;
  ssize_t len;
  completion_t* elems;
  alloc_t* mem;
};

static void default_filename_completer( rpl_completion_env_t* cenv, const char* prefix );

rpl_private completions_t* completions_new(alloc_t* mem) {
  completions_t* cms = mem_zalloc_tp(mem, completions_t);
  if (cms == NULL) return NULL;
  cms->mem = mem;
  cms->completer = &default_filename_completer;
  return cms;
}

rpl_private void completions_free(completions_t* cms) {
  if (cms == NULL) return;
  completions_clear(cms);  
  if (cms->elems != NULL) {
    mem_free(cms->mem, cms->elems);
    cms->elems = NULL;
    cms->count = 0;
    cms->len = 0;
  }
  mem_free(cms->mem, cms); // free ourselves
}


rpl_private void completions_clear(completions_t* cms) {  
  while (cms->count > 0) {
    completion_t* cm = cms->elems + cms->count - 1;
    mem_free( cms->mem, cm->display);
    mem_free( cms->mem, cm->replacement);
    mem_free( cms->mem, cm->help);
    memset(cm,0,sizeof(*cm));
    cms->count--;    
  }
}

static void completions_push(completions_t* cms, const char* replacement, const char* display, const char* help, ssize_t delete_before, ssize_t delete_after) 
{
  if (cms->count >= cms->len) {
    ssize_t newlen = (cms->len <= 0 ? 32 : cms->len*2);
    completion_t* newelems = mem_realloc_tp(cms->mem, completion_t, cms->elems, newlen );
    if (newelems == NULL) return;
    cms->elems = newelems;
    cms->len   = newlen;
  }
  assert(cms->count < cms->len);
  completion_t* cm  = cms->elems + cms->count;
  cm->replacement   = mem_strdup(cms->mem,replacement);
  cm->display       = mem_strdup(cms->mem,display);
  cm->help          = mem_strdup(cms->mem,help);
  cm->delete_before = delete_before;
  cm->delete_after  = delete_after;
  cms->count++;
}

rpl_private ssize_t completions_count(completions_t* cms) {
  return cms->count;
}

static bool completions_contains(completions_t* cms, const char* replacement) {
  for( ssize_t i = 0; i < cms->count; i++ ) {
    const completion_t* c = cms->elems + i;
    if (strcmp(replacement,c->replacement) == 0) { return true; }
  }
  return false;
} 

rpl_private bool completions_add(completions_t* cms, const char* replacement, const char* display, const char* help, ssize_t delete_before, ssize_t delete_after) {
  if (cms->completer_max <= 0) return false;
  cms->completer_max--;
  //debug_msg("completion: add: %d,%d, %s\n", delete_before, delete_after, replacement);
  if (!completions_contains(cms,replacement)) {
    completions_push(cms, replacement, display, help, delete_before, delete_after);
  }
  return true;
}

static completion_t* completions_get(completions_t* cms, ssize_t index) {
  if (index < 0 || cms->count <= 0 || index >= cms->count) return NULL;
  return &cms->elems[index];
}

rpl_private const char* completions_get_display( completions_t* cms, ssize_t index, const char** help ) {
  if (help != NULL) { *help = NULL;  }
  completion_t* cm = completions_get(cms, index);
  if (cm == NULL) return NULL;
  if (help != NULL) { *help = cm->help; }
  return (cm->display != NULL ? cm->display : cm->replacement);
}

rpl_private const char* completions_get_help( completions_t* cms, ssize_t index ) {
  completion_t* cm = completions_get(cms, index);
  if (cm == NULL) return NULL;
  return cm->help;
}

rpl_private const char* completions_get_hint(completions_t* cms, ssize_t index, const char** help) {
  if (help != NULL) { *help = NULL; }
  completion_t* cm = completions_get(cms, index);
  if (cm == NULL) return NULL;
  ssize_t len = rpl_strlen(cm->replacement);
  if (len < cm->delete_before) return NULL;
  const char* hint = (cm->replacement + cm->delete_before);
  if (*hint == 0 || utf8_is_cont((uint8_t)(*hint))) return NULL;  // utf8 boundary?
  if (help != NULL) { *help = cm->help; }
  return hint;
}

rpl_private void completions_set_completer(completions_t* cms, rpl_completer_fun_t* completer, void* arg) {
  cms->completer = completer;
  cms->completer_arg = arg;
}

rpl_private void completions_get_completer(completions_t* cms, rpl_completer_fun_t** completer, void** arg) {
  *completer = cms->completer;
  *arg = cms->completer_arg;
}


rpl_public void* rpl_completion_arg( const rpl_completion_env_t* cenv ) {
  return (cenv == NULL ? NULL : cenv->env->completions->completer_arg);
}

rpl_public bool rpl_has_completions( const rpl_completion_env_t* cenv ) {
  return (cenv == NULL ? false : cenv->env->completions->count > 0);
}

rpl_public bool rpl_stop_completing( const rpl_completion_env_t* cenv) {
  return (cenv == NULL ? true : cenv->env->completions->completer_max <= 0);
}


static ssize_t completion_apply( completion_t* cm, stringbuf_t* sbuf, ssize_t pos ) {
  if (cm == NULL) return -1;  
  debug_msg( "completion: apply: %s at %zd\n", cm->replacement, pos);
  ssize_t start = pos - cm->delete_before;
  if (start < 0) start = 0;
  ssize_t n = cm->delete_before + cm->delete_after;
  if (rpl_strlen(cm->replacement) == n && strncmp(sbuf_string_at(sbuf,start), cm->replacement, to_size_t(n)) == 0) {
    // no changes
    return -1;
  }
  else {
    sbuf_delete_from_to( sbuf, start, pos + cm->delete_after );
    return sbuf_insert_at(sbuf, cm->replacement, start); 
  }
}

rpl_private ssize_t completions_apply( completions_t* cms, ssize_t index, stringbuf_t* sbuf, ssize_t pos ) {
  completion_t* cm = completions_get(cms, index);
  return completion_apply( cm, sbuf, pos );
}


static int completion_compare(const void* p1, const void* p2) {
  if (p1 == NULL || p2 == NULL) return 0;
  const completion_t* cm1 = (const completion_t*)p1;
  const completion_t* cm2 = (const completion_t*)p2;  
  return rpl_stricmp(cm1->replacement, cm2->replacement);
}

rpl_private void completions_sort(completions_t* cms) {
  if (cms->count <= 0) return;
  qsort(cms->elems, to_size_t(cms->count), sizeof(cms->elems[0]), &completion_compare);
}

#define RPL_MAX_PREFIX  (256)

// find longest common prefix and complete with that.
rpl_private ssize_t completions_apply_longest_prefix(completions_t* cms, stringbuf_t* sbuf, ssize_t pos) {
  if (cms->count <= 1) {
    return completions_apply(cms,0,sbuf,pos);
  }

  // set initial prefix to the first entry
  completion_t* cm = completions_get(cms, 0);
  if (cm == NULL) return -1;

  char prefix[RPL_MAX_PREFIX+1];
  ssize_t delete_before = cm->delete_before;
  rpl_strncpy( prefix, RPL_MAX_PREFIX+1, cm->replacement, RPL_MAX_PREFIX );
  prefix[RPL_MAX_PREFIX] = 0;
  
  // and visit all others to find the longest common prefix
  for(ssize_t i = 1; i < cms->count; i++) {
    cm = completions_get(cms,i);
    if (cm->delete_before != delete_before) {  // deletions must match delete_before
      prefix[0] = 0;
      break;
    }
    // check if it is still a prefix
    const char* r = cm->replacement;    
    ssize_t j;
    for(j = 0; prefix[j] != 0 && r[j] != 0; j++) {
      if (prefix[j] != r[j]) break;
    }
    prefix[j] = 0;
    if (j <= 0) break;
  }

  // check the length
  ssize_t len = rpl_strlen(prefix);
  if (len <= 0 || len < delete_before) return -1;

  // we found a prefix :-)
  completion_t cprefix;
  memset(&cprefix,0,sizeof(cprefix));
  cprefix.delete_before = delete_before;
  cprefix.replacement   = prefix;
  ssize_t newpos = completion_apply( &cprefix, sbuf, pos);
  if (newpos < 0) return newpos;  

  // adjust all delete_before for the new replacement
  for( ssize_t i = 0; i < cms->count; i++) {
    cm = completions_get(cms,i);
    cm->delete_before = len;
  }

  return newpos;
}


//-------------------------------------------------------------
// Completer functions
//-------------------------------------------------------------

rpl_public bool rpl_add_completions(rpl_completion_env_t* cenv, const char* prefix, const char** completions) {
  for (const char** pc = completions; *pc != NULL; pc++) {
    // if (rpl_istarts_with(*pc, prefix)) {
    if (rpl_starts_with(*pc, prefix)) {
      if (!rpl_add_completion_ex(cenv, *pc, NULL, NULL)) return false;
    }
  }
  return true;
}

rpl_public bool rpl_add_completion(rpl_completion_env_t* cenv, const char* replacement) {
  return rpl_add_completion_ex(cenv, replacement, NULL, NULL);
}

rpl_public bool rpl_add_completion_ex( rpl_completion_env_t* cenv, const char* replacement, const char* display, const char* help ) {
  return rpl_add_completion_prim(cenv,replacement,display,help,0,0);
}

rpl_public bool rpl_add_completion_prim(rpl_completion_env_t* cenv, const char* replacement, const char* display, const char* help, long delete_before, long delete_after) {
  return (*cenv->complete)(cenv->env, cenv->closure, replacement, display, help, delete_before, delete_after );
}

static bool prim_add_completion(rpl_env_t* env, void* funenv, const char* replacement, const char* display, const char* help, long delete_before, long delete_after) {
  rpl_unused(funenv);
  return completions_add(env->completions, replacement, display, help, delete_before, delete_after);
}

rpl_public void rpl_set_default_completer(rpl_completer_fun_t* completer, void* arg) {
  rpl_env_t* env = rpl_get_env(); if (env == NULL) return;
  completions_set_completer(env->completions, completer, arg);
}

rpl_private ssize_t completions_generate(struct rpl_env_s* env, completions_t* cms, const char* input, ssize_t pos, ssize_t max) {
  completions_clear(cms);
  if (cms->completer == NULL || input == NULL || rpl_strlen(input) < pos) return 0;

  // set up env
  rpl_completion_env_t cenv;
  cenv.env = env;
  cenv.input = input,
  cenv.cursor = (long)pos;
  cenv.arg = cms->completer_arg;
  cenv.complete = &prim_add_completion;
  cenv.closure  = NULL;
  const char* prefix = mem_strndup(cms->mem, input, pos);
  cms->completer_max = max;
  
  // and complete
  cms->completer(&cenv,prefix);

  // restore
  mem_free(cms->mem,prefix);
  return completions_count(cms);
}

// The default completer is no completion is set
static void default_filename_completer( rpl_completion_env_t* cenv, const char* prefix ) {
  #ifdef _WIN32
  const char sep = '\\';
  #else
  const char sep = '/';
  #endif
  rpl_complete_filename( cenv, prefix, sep, ".", NULL);
}
