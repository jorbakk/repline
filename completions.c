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
	const char *replacement;
	const char *display;
	const char *help;
	ssize_t delete_before;
	ssize_t delete_after;
} completion_t;

struct completions_s {
	rpl_completer_fun_t *completer;
	void *completer_arg;
	ssize_t completer_max;
	ssize_t count;
	ssize_t len;
	completion_t *elems;
	alloc_t *mem;
	ssize_t cut_start;
	ssize_t cut_stop;
};

static void default_filename_completer(rpl_completion_env_t * cenv,
                                       const char *prefix);

rpl_private completions_t *
completions_new(alloc_t * mem)
{
	completions_t *cms = mem_zalloc_tp(mem, completions_t);
	if (cms == NULL)
		return NULL;
	cms->mem = mem;
	cms->completer = &default_filename_completer;
	return cms;
}

rpl_private void
completions_free(completions_t * cms)
{
	if (cms == NULL)
		return;
	completions_clear(cms);
	if (cms->elems != NULL) {
		mem_free(cms->mem, cms->elems);
		cms->elems = NULL;
		cms->count = 0;
		cms->len = 0;
	}
	mem_free(cms->mem, cms);    // free ourselves
}

rpl_private void
completions_clear(completions_t * cms)
{
	while (cms->count > 0) {
		completion_t *cm = cms->elems + cms->count - 1;
		mem_free(cms->mem, cm->display);
		mem_free(cms->mem, cm->replacement);
		mem_free(cms->mem, cm->help);
		memset(cm, 0, sizeof(*cm));
		cms->count--;
	}
}


static void
completions_push(completions_t * cms, const char *replacement,
                 const char *display, const char *help, ssize_t delete_before,
                 ssize_t delete_after)
{
	if (cms->count >= cms->len) {
		ssize_t newlen = (cms->len <= 0 ? 32 : cms->len * 2);
		completion_t *newelems =
		    mem_realloc_tp(cms->mem, completion_t, cms->elems, newlen);
		if (newelems == NULL)
			return;
		cms->elems = newelems;
		cms->len = newlen;
	}
	assert(cms->count < cms->len);
	completion_t *cm = cms->elems + cms->count;
	cm->replacement = mem_strdup(cms->mem, replacement);
	cm->display = mem_strdup(cms->mem, display);
	cm->help = mem_strdup(cms->mem, help);
	cm->delete_before = delete_before;
	cm->delete_after = delete_after;
	cms->count++;
}

rpl_private ssize_t
completions_count(completions_t * cms)
{
	return cms->count;
}

static bool
completions_contains(completions_t * cms, const char *replacement)
{
	for (ssize_t i = 0; i < cms->count; i++) {
		const completion_t *c = cms->elems + i;
		if (strcmp(replacement, c->replacement) == 0) {
			return true;
		}
	}
	return false;
}

rpl_private bool
completions_add(completions_t * cms, const char *replacement,
                const char *display, const char *help, ssize_t delete_before,
                ssize_t delete_after)
{
	if (cms->completer_max <= 0)
		return false;
	cms->completer_max--;
	//debug_msg("completion: add: %d,%d, %s\n", delete_before, delete_after, replacement);
	if (!completions_contains(cms, replacement)) {
		completions_push(cms, replacement, display, help, delete_before,
		                 delete_after);
	}
	return true;
}

static completion_t *
completions_get(completions_t * cms, ssize_t index)
{
	if (index < 0 || cms->count <= 0 || index >= cms->count)
		return NULL;
	return &cms->elems[index];
}

rpl_private const char *
completions_get_display(completions_t * cms, ssize_t index, const char **help)
{
	if (help != NULL) {
		*help = NULL;
	}
	completion_t *cm = completions_get(cms, index);
	if (cm == NULL)
		return NULL;
	if (help != NULL) {
		*help = cm->help;
	}
	return (cm->display != NULL ? cm->display : cm->replacement);
}

rpl_private const char *
completions_get_help(completions_t * cms, ssize_t index)
{
	completion_t *cm = completions_get(cms, index);
	if (cm == NULL)
		return NULL;
	return cm->help;
}

rpl_private const char *
completions_get_hint(completions_t * cms, ssize_t index, const char **help)
{
	if (help != NULL) {
		*help = NULL;
	}
	completion_t *cm = completions_get(cms, index);
	if (cm == NULL)
		return NULL;
	ssize_t len = rpl_strlen(cm->replacement);
	if (len < cm->delete_before)
		return NULL;
	const char *hint = (cm->replacement + cm->delete_before);
	if (*hint == 0 || utf8_is_cont((uint8_t) (*hint)))
		return NULL;            // utf8 boundary?
	if (help != NULL) {
		*help = cm->help;
	}
	return hint;
}

rpl_private void
completions_set_completer(completions_t * cms, rpl_completer_fun_t * completer,
                          void *arg)
{
	cms->completer = completer;
	cms->completer_arg = arg;
}

rpl_private void
completions_get_completer(completions_t * cms, rpl_completer_fun_t ** completer,
                          void **arg)
{
	*completer = cms->completer;
	*arg = cms->completer_arg;
}

rpl_public void *
rpl_completion_arg(const rpl_completion_env_t * cenv)
{
	return (cenv == NULL ? NULL : cenv->env->completions->completer_arg);
}

rpl_public bool
rpl_has_completions(const rpl_completion_env_t * cenv)
{
	return (cenv == NULL ? false : cenv->env->completions->count > 0);
}

rpl_public bool
rpl_stop_completing(const rpl_completion_env_t * cenv)
{
	return (cenv == NULL ? true : cenv->env->completions->completer_max <= 0);
}

#ifdef NEW_COMPLETIONS
static ssize_t
new_completion_apply(completion_t *cm, stringbuf_t *sbuf, ssize_t pos)
{
	if (cm == NULL)
		return -1;
	debug_msg("completion: apply: %s at %zd\n", cm->replacement, pos);
	// ssize_t start = pos - cm->delete_before;
	ssize_t start = cm->delete_before;
	if (start < 0)
		start = 0;
	ssize_t n = cm->delete_before + cm->delete_after;
	if (rpl_strlen(cm->replacement) == n
	    && strncmp(sbuf_string_at(sbuf, start), cm->replacement,
	               to_size_t(n)) == 0) {
		// no changes
		return -1;
	} else {
		// sbuf_delete_from_to(sbuf, start, pos + cm->delete_after);
		sbuf_delete_from_to(sbuf, start, cm->delete_after);
		return sbuf_insert_at(sbuf, cm->replacement, start);
	}
}
#else
static ssize_t
completion_apply(completion_t * cm, stringbuf_t * sbuf, ssize_t pos)
{
	if (cm == NULL)
		return -1;
	debug_msg("completion: apply: %s at %zd\n", cm->replacement, pos);
	ssize_t start = pos - cm->delete_before;
	if (start < 0)
		start = 0;
	ssize_t n = cm->delete_before + cm->delete_after;
	if (rpl_strlen(cm->replacement) == n
	    && strncmp(sbuf_string_at(sbuf, start), cm->replacement,
	               to_size_t(n)) == 0) {
		// no changes
		return -1;
	} else {
		sbuf_delete_from_to(sbuf, start, pos + cm->delete_after);
		return sbuf_insert_at(sbuf, cm->replacement, start);
	}
}
#endif


rpl_private ssize_t
completions_apply(completions_t * cms, ssize_t index, stringbuf_t * sbuf,
                  ssize_t pos)
{
	completion_t *cm = completions_get(cms, index);
#ifdef NEW_COMPLETIONS
	return new_completion_apply(cm, sbuf, pos);
#else
	return completion_apply(cm, sbuf, pos);
#endif
}


/// ------------------ no changes from here ...
static int
completion_compare(const void *p1, const void *p2)
{
	if (p1 == NULL || p2 == NULL)
		return 0;
	const completion_t *cm1 = (const completion_t *)p1;
	const completion_t *cm2 = (const completion_t *)p2;
	return rpl_stricmp(cm1->replacement, cm2->replacement);
}

rpl_private void
completions_sort(completions_t * cms)
{
	if (cms->count <= 0)
		return;
	qsort(cms->elems, to_size_t(cms->count), sizeof(cms->elems[0]),
	      &completion_compare);
}

#define RPL_MAX_PREFIX  (256)

// find longest common prefix and complete with that.
rpl_private ssize_t
completions_apply_longest_prefix(completions_t * cms, stringbuf_t * sbuf,
                                 ssize_t pos)
{
	if (cms->count <= 1) {
		return completions_apply(cms, 0, sbuf, pos);
	}
	// set initial prefix to the first entry
	completion_t *cm = completions_get(cms, 0);
	if (cm == NULL)
		return -1;

	char prefix[RPL_MAX_PREFIX + 1];
	ssize_t delete_before = cm->delete_before;
	rpl_strncpy(prefix, RPL_MAX_PREFIX + 1, cm->replacement, RPL_MAX_PREFIX);
	prefix[RPL_MAX_PREFIX] = 0;

	// and visit all others to find the longest common prefix
	for (ssize_t i = 1; i < cms->count; i++) {
		cm = completions_get(cms, i);
		if (cm->delete_before != delete_before) {   // deletions must match delete_before
			prefix[0] = 0;
			break;
		}
		// check if it is still a prefix
		const char *r = cm->replacement;
		ssize_t j;
		for (j = 0; prefix[j] != 0 && r[j] != 0; j++) {
			if (prefix[j] != r[j])
				break;
		}
		prefix[j] = 0;
		if (j <= 0)
			break;
	}

	// check the length
	ssize_t len = rpl_strlen(prefix);
	if (len <= 0 || len < delete_before)
		return -1;

	// we found a prefix :-)
	completion_t cprefix;
	memset(&cprefix, 0, sizeof(cprefix));
	cprefix.delete_before = delete_before;
	cprefix.replacement = prefix;
#ifdef NEW_COMPLETIONS
	ssize_t newpos = new_completion_apply(&cprefix, sbuf, pos);
#else
	ssize_t newpos = completion_apply(&cprefix, sbuf, pos);
#endif
	if (newpos < 0)
		return newpos;

	// adjust all delete_before for the new replacement
	for (ssize_t i = 0; i < cms->count; i++) {
		cm = completions_get(cms, i);
		cm->delete_before = len;
	}

	return newpos;
}

//-------------------------------------------------------------
// Completer functions
//-------------------------------------------------------------

rpl_public bool
rpl_add_completions(rpl_completion_env_t * cenv, const char *prefix,
                    const char **completions)
{
	for (const char **pc = completions; *pc != NULL; pc++) {
		// if (rpl_istarts_with(*pc, prefix)) {
		if (rpl_starts_with(*pc, prefix)) {
			if (!rpl_add_completion_ex(cenv, *pc, NULL, NULL))
				return false;
		}
	}
	return true;
}

rpl_public bool
rpl_add_completion(rpl_completion_env_t * cenv, const char *replacement)
{
	return rpl_add_completion_ex(cenv, replacement, NULL, NULL);
}

rpl_public bool
rpl_add_completion_ex(rpl_completion_env_t * cenv, const char *replacement,
                      const char *display, const char *help)
{
	return rpl_add_completion_prim(cenv, replacement, display, help, 0, 0);
}

rpl_public bool
rpl_add_completion_prim(rpl_completion_env_t * cenv, const char *replacement,
                        const char *display, const char *help,
                        long delete_before, long delete_after)
{
	return (*cenv->complete) (cenv->env, cenv->closure, replacement, display,
	                          help, delete_before, delete_after);
}

static bool
prim_add_completion(rpl_env_t * env, void *funenv, const char *replacement,
                    const char *display, const char *help, long delete_before,
                    long delete_after)
{
	rpl_unused(funenv);
	return completions_add(env->completions, replacement, display, help,
	                       delete_before, delete_after);
}

rpl_public void
rpl_set_default_completer(rpl_completer_fun_t * completer, void *arg)
{
	rpl_env_t *env = rpl_get_env();
	if (env == NULL)
		return;
	completions_set_completer(env->completions, completer, arg);
}

#ifdef NEW_COMPLETIONS
typedef struct stringview_s {
	char *start, *stop;
} stringview_t;


/// Returns a string view that covers the word where the cursor (pos) is in.
/// Paramters
///   input: null-terminated string that will be parsed
///   pos: cursor position
///   delim: function that defines the word boundary characters
stringview_t
get_word(const char *input, ssize_t pos, rpl_is_char_class_fun_t *delim)
{
	size_t len = strlen(input);
	ssize_t word_start = str_find_backward(input, len, pos, delim, false);
	word_start = word_start < 0 ? 0 : word_start;
	ssize_t word_stop  = str_find_forward(input, len, pos, delim, false);
	word_stop = word_stop < 0 ? len : word_stop;
	stringview_t ret = { .start = (char *)input + word_start, .stop = (char *)input + word_stop };
	return ret;
}


/// Returns a string view that covers the word where the cursor (pos) is in.
/// Paramters
///   input: string view that will be parsed
///   pos: cursor position
///   delim: function that defines the word boundary characters
stringview_t
get_word_from_view(stringview_t input, ssize_t pos, rpl_is_char_class_fun_t *delim)
{
	ssize_t word_start = str_find_backward(input.start, input.stop - input.start, pos, delim, false);
	word_start = word_start < 0 ? 0 : word_start;
	ssize_t word_stop  = str_find_forward(input.start, input.stop - input.start, pos, delim, false);
	word_stop = word_stop < 0 ? input.stop - input.start : word_stop;
	stringview_t ret = { .start = input.start + word_start, .stop = input.start + word_stop };
	return ret;
}


static void
new_filename_completer(rpl_env_t *env, const char *input, ssize_t pos)
{
	if (input == NULL)
		return;

	/// New way to find the boundaries for cutting out and replacing the word to be
	/// completed.
	stringview_t word = get_word(input, pos, rpl_char_is_white);
	stringview_t fname_prefix = get_word_from_view(word, word.stop - word.start,
	                                          rpl_char_is_dir_separator);
	stringview_t dirname = { .start = word.start, .stop = fname_prefix.start };
	env->completions->cut_start = fname_prefix.start - input;
	env->completions->cut_stop = fname_prefix.stop - input;

	/// Print the boundaries for debugging purposes
	char word_str[128];
	snprintf(word_str, word.stop - word.start + 1, "%s", word.start);
	char fname_prefix_str[128];
	snprintf(fname_prefix_str, fname_prefix.stop - fname_prefix.start + 1, "%s", fname_prefix.start);
	char dirname_str[128] = {0};
	if (dirname.start == dirname.stop) {
		dirname_str[0] = '.';
		dirname_str[1] = rpl_dirsep();
	} else {
		snprintf(dirname_str, dirname.stop - dirname.start + 1, "%s", dirname.start);
	}
	// printf("rest input: \"%s\", word: \"%s\", dirname: \"%s\", fname_prefix: \"%s\"\n",
	      // input + pos, word_str, dirname_str, fname_prefix_str);

	dir_cursor d = 0;
	dir_entry entry;
	bool cont = true;
	if (os_findfirst(env->mem, dirname_str, &d, &entry)) {
		do {
			const char *fname = os_direntry_name(&entry);
			// printf("dir entry: \"%s\"\n", fname);
			if (fname != NULL &&
			    strcmp(fname, ".") != 0 &&
			    strcmp(fname, "..") != 0 &&
			    strlen(fname) >= fname_prefix.stop - fname_prefix.start &&
			    strncmp(fname, fname_prefix.start, fname_prefix.stop - fname_prefix.start) == 0)
			{
				const char *help = "";
                int delete_before = env->completions->cut_start;
                int delete_after = env->completions->cut_stop;
                /// Append '/' if fname is a directory
				stringbuf_t *fname_str = sbuf_new(env->mem);
				sbuf_append(fname_str, fname);
				char full_path[256];
				sprintf(full_path, "%s%s", dirname_str, fname);
				if (os_is_dir(full_path)) {
					sbuf_append_char(fname_str, rpl_dirsep());
				}
				cont = completions_add(env->completions,
				                      sbuf_string(fname_str), sbuf_string(fname_str),
				                      help, delete_before, delete_after);
				sbuf_free(fname_str);
			}
		} while (cont && os_findnext(d, &entry));
		os_findclose(d);
	}
}


void
new_completions_generate(struct rpl_env_s *env, const char *input, ssize_t pos, ssize_t max)
{
	completions_clear(env->completions);
	env->completions->completer_max = max;
	new_filename_completer(env, input, pos);
}

#else

rpl_private ssize_t
completions_generate(struct rpl_env_s *env, completions_t *cms,
                     const char *input, ssize_t pos, ssize_t max)
{
	completions_clear(cms);
	if (cms->completer == NULL || input == NULL || rpl_strlen(input) < pos)
		return 0;

	// set up env
	rpl_completion_env_t cenv;
	cenv.env = env;
	cenv.input = input;
	cenv.cursor = (long)pos;
	cenv.arg = cms->completer_arg;
	cenv.complete = &prim_add_completion;
	cenv.closure = NULL;
	const char *prefix = mem_strndup(cms->mem, input, pos);
	cms->completer_max = max;

	// and complete
	cms->completer(&cenv, prefix);

	// restore
	mem_free(cms->mem, prefix);
	return completions_count(cms);
}
#endif


extern char **environ;

void
printenv()
{
	char **s = environ;
	for (; *s; s++) {
		printf("%s\n", *s);
	}
	printf("\n");
}


// The default completer is environment-variable-expansion and filename-completion
static void
default_filename_completer(rpl_completion_env_t * cenv, const char *prefix)
{
	// printenv();
	// printf("\ndefault filename completer\n");

#ifdef _WIN32
	const char sep = '\\';
#else
	const char sep = '/';
#endif
	rpl_complete_filename(cenv, prefix, sep, ".", NULL);

	// char *expanded = rpl_expand_envar(cenv, prefix);
	// rpl_complete_filename(cenv, expanded, sep, ".", NULL);
	// rpl_free(expanded);

	// rpl_complete_envar(cenv, prefix);
}
