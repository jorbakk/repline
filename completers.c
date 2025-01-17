#include <string.h>
#include <stdio.h>

#include "repline.h"
#include "common.h"
#include "env.h"
#include "stringbuf.h"
#include "completions.h"

//-------------------------------------------------------------
// Complete file names
// Listing files
//-------------------------------------------------------------
#include <stdlib.h>

typedef enum file_type_e {
	// must follow BSD style LSCOLORS order
	FT_DEFAULT = 0,
	FT_DIR,
	FT_SYM,
	FT_SOCK,
	FT_PIPE,
	FT_BLOCK,
	FT_CHAR,
	FT_SETUID,
	FT_SETGID,
	FT_DIR_OW_STICKY,
	FT_DIR_OW,
	FT_DIR_STICKY,
	FT_EXE,
	FT_LAST
} file_type_t;

static int cli_color;           // 1 enabled, 0 not initialized, -1 disabled
static const char *lscolors = "exfxcxdxbxegedabagacad"; // default BSD setting
static const char *ls_colors;
static const char *ls_colors_names[] =
    { "no=", "di=", "ln=", "so=", "pi=", "bd=", "cd=", "su=", "sg=", "tw=",
"ow=", "st=", "ex=", NULL };

static bool
ls_colors_init(void)
{
	if (cli_color != 0)
		return (cli_color >= 1);
	// colors enabled?
	const char *s = getenv("CLICOLOR");
	if (s == NULL || (strcmp(s, "1") != 0 && strcmp(s, "") != 0)) {
		cli_color = -1;
		return false;
	}
	cli_color = 1;
	s = getenv("LS_COLORS");
	if (s != NULL) {
		ls_colors = s;
	}
	s = getenv("LSCOLORS");
	if (s != NULL) {
		lscolors = s;
	}
	return true;
}

static bool
ls_valid_esc(ssize_t c)
{
	return ((c == 0 || c == 1 || c == 4 || c == 7 || c == 22 || c == 24
	         || c == 27) || (c >= 30 && c <= 37) || (c >= 40 && c <= 47)
	        || (c >= 90 && c <= 97) || (c >= 100 && c <= 107));
}

static bool
ls_colors_from_key(stringbuf_t * sb, const char *key)
{
	// find key
	ssize_t keylen = rpl_strlen(key);
	if (keylen <= 0)
		return false;
	const char *p = strstr(ls_colors, key);
	if (p == NULL)
		return false;
	p += keylen;
	if (key[keylen - 1] != '=') {
		if (*p != '=')
			return false;
		p++;
	}
	ssize_t len = 0;
	while (p[len] != 0 && p[len] != ':') {
		len++;
	}
	if (len <= 0)
		return false;
	sbuf_append(sb, "[ansi-sgr=\"");
	sbuf_append_n(sb, p, len);
	sbuf_append(sb, "\"]");
	return true;
}

static int
ls_colors_from_char(char c)
{
	if (c >= 'a' && c <= 'h') {
		return (c - 'a');
	} else if (c >= 'A' && c <= 'H') {
		return (c - 'A') + 8;
	} else if (c == 'x') {
		return 256;
	} else
		return 256;             // default
}

static bool
ls_colors_append(stringbuf_t * sb, file_type_t ft, const char *ext)
{
	if (!ls_colors_init())
		return false;
	if (ls_colors != NULL) {
		// GNU style
		if (ft == FT_DEFAULT && ext != NULL) {
			// first try extension match
			if (ls_colors_from_key(sb, ext))
				return true;
		}
		if (ft >= FT_DEFAULT && ft < FT_LAST) {
			// then a filetype match
			const char *key = ls_colors_names[ft];
			if (ls_colors_from_key(sb, key))
				return true;
		}
	} else if (lscolors != NULL) {
		// BSD style
		char fg = 'x';
		char bg = 'x';
		if (rpl_strlen(lscolors) > (2 * (ssize_t) ft) + 1) {
			fg = lscolors[2 * ft];
			bg = lscolors[2 * ft + 1];
		}
		sbuf_appendf(sb, "[ansi-color=%d ansi-bgcolor=%d]",
		             ls_colors_from_char(fg), ls_colors_from_char(bg));
		return true;
	}
	return false;
}

static void
ls_colorize(bool no_lscolor, stringbuf_t * sb, file_type_t ft, const char *name,
            const char *ext, char dirsep)
{
	bool close = (no_lscolor ? false : ls_colors_append(sb, ft, ext));
	sbuf_append(sb, "[!pre]");
	sbuf_append(sb, name);
	if (dirsep != 0)
		sbuf_append_char(sb, dirsep);
	sbuf_append(sb, "[/pre]");
	if (close) {
		sbuf_append(sb, "[/]");
	}
}

#if defined(_WIN32)
#include <io.h>
#include <sys/stat.h>

static bool
os_is_dir(const char *cpath)
{
	struct _stat64 st = { 0 };
	_stat64(cpath, &st);
	return ((st.st_mode & _S_IFDIR) != 0);
}

static file_type_t
os_get_filetype(const char *cpath)
{
	struct _stat64 st = { 0 };
	_stat64(cpath, &st);
	if (((st.st_mode) & _S_IFDIR) != 0)
		return FT_DIR;
	if (((st.st_mode) & _S_IFCHR) != 0)
		return FT_CHAR;
	if (((st.st_mode) & _S_IFIFO) != 0)
		return FT_PIPE;
	if (((st.st_mode) & _S_IEXEC) != 0)
		return FT_EXE;
	return FT_DEFAULT;
}

#define dir_cursor intptr_t
#define dir_entry  struct __finddata64_t

static bool
os_findfirst(alloc_t * mem, const char *path, dir_cursor * d, dir_entry * entry)
{
	stringbuf_t *spath = sbuf_new(mem);
	if (spath == NULL)
		return false;
	sbuf_append(spath, path);
	sbuf_append(spath, "\\*");
	*d = _findfirsti64(sbuf_string(spath), entry);
	mem_free(mem, spath);
	return (*d != -1);
}

static bool
os_findnext(dir_cursor d, dir_entry * entry)
{
	return (_findnexti64(d, entry) == 0);
}

static void
os_findclose(dir_cursor d)
{
	_findclose(d);
}

static const char *
os_direntry_name(dir_entry * entry)
{
	return entry->name;
}

static bool
os_path_is_absolute(const char *path)
{
	if (path != NULL && path[0] != 0 && path[1] == ':'
	    && (path[2] == '\\' || path[2] == '/' || path[2] == 0)) {
		char drive = path[0];
		return ((drive >= 'A' && drive <= 'Z')
		        || (drive >= 'a' && drive <= 'z'));
	} else
		return false;
}

rpl_private char
rpl_dirsep(void)
{
	return '\\';
}
#else

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static bool
os_is_dir(const char *cpath)
{
	struct stat st;
	memset(&st, 0, sizeof(st));
	stat(cpath, &st);
	return (S_ISDIR(st.st_mode));
}

static file_type_t
os_get_filetype(const char *cpath)
{
	struct stat st;
	memset(&st, 0, sizeof(st));
	lstat(cpath, &st);
	switch ((st.st_mode) & S_IFMT) {
	case S_IFSOCK:
		return FT_SOCK;
	case S_IFLNK:{
			return FT_SYM;
		}
	case S_IFIFO:
		return FT_PIPE;
	case S_IFCHR:
		return FT_CHAR;
	case S_IFBLK:
		return FT_BLOCK;
	case S_IFDIR:{
			if ((st.st_mode & S_ISUID) != 0)
				return FT_SETUID;
			if ((st.st_mode & S_ISGID) != 0)
				return FT_SETGID;
			if ((st.st_mode & S_IWGRP) != 0 && (st.st_mode & S_ISVTX) != 0)
				return FT_DIR_OW_STICKY;
			if ((st.st_mode & S_IWGRP))
				return FT_DIR_OW;
			if ((st.st_mode & S_ISVTX))
				return FT_DIR_STICKY;
			return FT_DIR;
		}
	case S_IFREG:
	default:{
			if ((st.st_mode & S_IXUSR) != 0)
				return FT_EXE;
			return FT_DEFAULT;
		}
	}
}

#define dir_cursor DIR*
#define dir_entry  struct dirent*

static bool
os_findnext(dir_cursor d, dir_entry * entry)
{
	*entry = readdir(d);
	return (*entry != NULL);
}

static bool
os_findfirst(alloc_t * mem, const char *cpath, dir_cursor * d,
             dir_entry * entry)
{
	rpl_unused(mem);
	*d = opendir(cpath);
	if (*d == NULL) {
		return false;
	} else {
		return os_findnext(*d, entry);
	}
}

static void
os_findclose(dir_cursor d)
{
	closedir(d);
}

static const char *
os_direntry_name(dir_entry * entry)
{
	return (*entry)->d_name;
}

static bool
os_path_is_absolute(const char *path)
{
	return (path != NULL && path[0] == '/');
}

rpl_private char
rpl_dirsep(void)
{
	return '/';
}
#endif

//-------------------------------------------------------------
// File completion 
//-------------------------------------------------------------

static bool
ends_with_n(const char *name, ssize_t name_len, const char *ending, ssize_t len)
{
	if (name_len < len)
		return false;
	if (ending == NULL || len <= 0)
		return true;
	for (ssize_t i = 1; i <= len; i++) {
		char c1 = name[name_len - i];
		char c2 = ending[len - i];
#ifdef _WIN32
		if (rpl_tolower(c1) != rpl_tolower(c2))
			return false;
#else
		if (c1 != c2)
			return false;
#endif
	}
	return true;
}

static bool
match_extension(const char *name, const char *extensions)
{
	if (extensions == NULL || extensions[0] == 0)
		return true;
	if (name == NULL)
		return false;
	ssize_t name_len = rpl_strlen(name);
	ssize_t len = rpl_strlen(extensions);
	ssize_t cur = 0;
	//debug_msg("match extensions: %s ~ %s", name, extensions);
	for (ssize_t end = 0; end <= len; end++) {
		if (extensions[end] == ';' || extensions[end] == 0) {
			if (ends_with_n(name, name_len, extensions + cur, (end - cur))) {
				return true;
			}
			cur = end + 1;
		}
	}
	return false;
}


typedef struct filename_closure_s {
	const char *roots;
	const char *extensions;
	char dir_sep;
} filename_closure_t;


rpl_public char *
rpl_expand_envar(rpl_completion_env_t * cenv, const char *prefix)
{
	stringbuf_t *sbuf_prefix = sbuf_new(cenv->env->mem);
	sbuf_append(sbuf_prefix, prefix);
	debug_msg("\norig: %s\n", sbuf_string(sbuf_prefix));
	sbuf_expand_envars(sbuf_prefix);
	debug_msg("expanded: %s\n", sbuf_string(sbuf_prefix));
	char *ret = sbuf_free_dup(sbuf_prefix);
	return ret;
}

