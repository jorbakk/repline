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
} completion_t;

struct completions_s {
	// rpl_completer_fun_t *completer;
	void *completer_arg;
	ssize_t completer_max;
	ssize_t count;
	ssize_t len;
	completion_t *elems;
	alloc_t *mem;
	ssize_t cut_start;
	ssize_t cut_stop;
};


rpl_private completions_t *
completions_new(alloc_t * mem)
{
	completions_t *cms = mem_zalloc_tp(mem, completions_t);
	if (cms == NULL)
		return NULL;
	cms->mem = mem;
	// cms->completer = &default_filename_completer;
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
                 const char *display, const char *help)
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
	/// FIXME is this needed?
	// cm->delete_before = delete_before;
	// cm->delete_after = delete_after;
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
                const char *display, const char *help)
{
	if (cms->completer_max <= 0)
		return false;
	cms->completer_max--;
	//debug_msg("completion: add: %d,%d, %s\n", delete_before, delete_after, replacement);
	if (!completions_contains(cms, replacement)) {
		completions_push(cms, replacement, display, help);
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
	/// FIXME is this needed?
	// if (len < cm->delete_before)
		// return NULL;
	// const char *hint = (cm->replacement + cm->delete_before);
	/// FIXME is this a good replacement?
	const char *hint = (cm->replacement + cms->cut_start);
	if (*hint == 0 || utf8_is_cont((uint8_t) (*hint)))
		return NULL;            // utf8 boundary?
	if (help != NULL) {
		*help = cm->help;
	}
	return hint;
}

// rpl_private void
// completions_set_completer(completions_t * cms, rpl_completer_fun_t * completer,
                          // void *arg)
// {
	// cms->completer = completer;
	// cms->completer_arg = arg;
// }

// rpl_private void
// completions_get_completer(completions_t * cms, rpl_completer_fun_t ** completer,
                          // void **arg)
// {
	// *completer = cms->completer;
	// *arg = cms->completer_arg;
// }

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


rpl_private ssize_t
completions_apply(completions_t *cms, ssize_t index, stringbuf_t *sbuf,
                  ssize_t pos)
{
	completion_t *cm = completions_get(cms, index);
	if (cm == NULL)
		return -1;
	debug_msg("completion: apply: %s at %zd\n", cm->replacement, pos);
	if (cms->cut_start < 0)
		cms->cut_start = 0;
	ssize_t n = cms->cut_start + cms->cut_stop;
	if (rpl_strlen(cm->replacement) == n
	    && strncmp(sbuf_string_at(sbuf, cms->cut_start), cm->replacement,
	               to_size_t(n)) == 0) {
		return -1;
	} else {
		sbuf_delete_from_to(sbuf, cms->cut_start, cms->cut_stop);
		return sbuf_insert_at(sbuf, cm->replacement, cms->cut_start);
	}
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
	return completions_add(env->completions, replacement, display, help);
}

// rpl_public void
// rpl_set_default_completer(rpl_completer_fun_t * completer, void *arg)
// {
	// rpl_env_t *env = rpl_get_env();
	// if (env == NULL)
		// return;
	// completions_set_completer(env->completions, completer, arg);
// }

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


ssize_t
get_first_diffchar(const char *first, const char *second)
{
	ssize_t first_len = strlen(first);
	ssize_t secnd_len = strlen(second);
	ssize_t min_len = first_len < secnd_len ? first_len : secnd_len;
	ssize_t i;
	for (i = 0; i < min_len; ++i) {
		if (first[i] != second[i]) break;
	}
	return i;
}


#define RPL_MAX_PREFIX  (256)

static void
filename_completer(rpl_env_t *env, editor_t *eb)
{
	const char *input = sbuf_string(eb->input);
	if (input == NULL)
		return;
	ssize_t pos = eb->pos;
	/// New way to find the boundaries for cutting out and replacing the word to be
	/// completed.
	stringview_t word = get_word(input, pos, rpl_char_is_white);
	stringview_t fname_prefix = get_word_from_view(word, word.stop - word.start,
	                                          rpl_char_is_dir_separator);
	ssize_t fname_prefix_len = fname_prefix.stop - fname_prefix.start;
	stringview_t dirname = { .start = word.start, .stop = fname_prefix.start };
	env->completions->cut_start = fname_prefix.start - input;
	env->completions->cut_stop = fname_prefix.stop - input;

	char word_str[RPL_MAX_PREFIX];
	snprintf(word_str, word.stop - word.start + 1, "%s", word.start);
	char fname_prefix_str[RPL_MAX_PREFIX];
	snprintf(fname_prefix_str, fname_prefix_len + 1, "%s", fname_prefix.start);
	char dirname_str[RPL_MAX_PREFIX] = {0};
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
	bool first = true;
	char pref_intersec[RPL_MAX_PREFIX] = {0};
	ssize_t diffchar = RPL_MAX_PREFIX;
	if (os_findfirst(env->mem, dirname_str, &d, &entry)) {
		do {
			const char *fname = os_direntry_name(&entry);
			// printf("dir entry: \"%s\"\n", fname);
			if (fname != NULL &&
			    strcmp(fname, ".") != 0 &&
			    strcmp(fname, "..") != 0 &&
			    strlen(fname) >= fname_prefix_len &&
			    strncmp(fname, fname_prefix.start, fname_prefix_len) == 0)
			{
				/// Update common prefix
				if (first) {
					first = false;
					strcpy(pref_intersec, fname);
				} else {
					if (diffchar > 0) {
						diffchar = get_first_diffchar(pref_intersec, fname);
						*(pref_intersec + diffchar) = 0;
					}
				}
				const char *help = "";
				stringbuf_t *fname_str = sbuf_new(env->mem);
				sbuf_append(fname_str, fname);
				char full_path[2 * RPL_MAX_PREFIX];
				sprintf(full_path, "%s%s", dirname_str, fname);
				if (os_is_dir(full_path)) {
					sbuf_append_char(fname_str, rpl_dirsep());
				}
				if (str_find_forward(fname, strlen(fname), 0, &rpl_char_is_white, true) > 0) {
					sbuf_insert_char_at(fname_str, '\'', 0);
					sbuf_append_char(fname_str, '\'');
				};
				cont = completions_add(env->completions,
				                      sbuf_string(fname_str), sbuf_string(fname_str),
				                      help);
				sbuf_free(fname_str);
			}
		} while (cont && os_findnext(d, &entry));
		os_findclose(d);
	}
	ssize_t pref_intersec_len = strlen(pref_intersec);
	if (pref_intersec_len > 0) {
		sbuf_insert_at(eb->input, pref_intersec + fname_prefix_len, eb->pos);
		eb->pos += pref_intersec_len - fname_prefix_len;
		env->completions->cut_stop -= fname_prefix_len - pref_intersec_len;
	}
}


void
completions_generate(struct rpl_env_s *env, editor_t *eb, ssize_t max)
{
	completions_clear(env->completions);
	env->completions->completer_max = max;
	filename_completer(env, eb);
}


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

