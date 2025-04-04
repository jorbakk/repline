#include "../repline.c"

rpl_env_t *env;
editor_t  *eb;
int total_count = 0, error_count = 0;


editor_t *
create_editbuf(rpl_env_t *env, const char* prompt_text)
{
	editor_t *eb = calloc(sizeof(editor_t), 1);
	eb->mem = env->mem;
	eb->input = sbuf_new(env->mem);
	eb->extra = sbuf_new(env->mem);
	eb->hint = sbuf_new(env->mem);
	eb->hint_help = sbuf_new(env->mem);
	eb->termw = term_get_width(env->term);
	eb->pos = 0;
	eb->cur_rows = 1;
	eb->cur_row = 0;
	eb->modified = false;
	eb->prompt_text = (prompt_text != NULL ? prompt_text : "");
	eb->history_idx = 0;
	eb->history_widx = 0;
	eb->history_wpos = 0;
	editstate_init(&eb->undo);
	editstate_init(&eb->redo);
	if (eb->input == NULL || eb->extra == NULL || eb->hint == NULL
	    || eb->hint_help == NULL) {
		return NULL;
	}
	return eb;
}


void
print_summary(void)
{
	puts("-----------------------------------------------------------");
	puts("\n===========================================================");
	printf("%d out of %d tests failed\n", error_count, total_count);
	puts("===========================================================\n");
}


void
print_completions(void)
{
	printf("completions count: %ld\n", env->completions->count);
	for (ssize_t c = 0; c < env->completions->count; ++c) {
		printf("%ld: \"%s\" %s\n", c + 1,
		       env->completions->elems[c].replacement,
		       env->completions->elems[c].display);
	}
}


void
setup_ebuf(char *input, int pos)
{
	total_count++;
	sbuf_append(eb->input, input);
	eb->pos = pos;
	puts("-----------------------------------------------------------");
	printf("test: %d\n", total_count);
	// printf("input: \"%s\", pos: %d\n", input, pos);
	printf("input[%2d]   %% %s\n", pos, input);
	printf("              %*c\n", pos + 1, '^');
}


void
clear_ebuf(void)
{
	sbuf_clear(eb->input);
	eb->pos = 0;
}


void
check_completion(int count, int idx, char *str)
{
	bool err = false;
	if (count == env->completions->count) {
		printf("OK completions count: %d\n", count);
	} else {
		err = true;
		printf("ERR completions count: %ld [%d]\n", env->completions->count, count);
	}
	if (strcmp(env->completions->elems[idx].replacement, str) == 0) {
		printf("OK completion with index %d: %s\n", idx, str);
	} else {
		err = true;
		printf("ERR completion with index %d: %s [%s]\n", idx, str,
		  env->completions->elems[idx].replacement);
	}
	if (err) error_count++;
}


void
check_completions_apply(const char *str)
{
	if (strcmp(sbuf_string(eb->input), str) == 0) {
		printf("OK completions apply: %s\n", sbuf_string(eb->input));
	} else {
		error_count++;
		printf("ERR completions apply: %s [%s]\n", sbuf_string(eb->input), str);
	}
}


void
test_file_completion(char *input, int pos, int res_count, int res_idx, char *res)
{
	setup_ebuf(input, pos);
	completions_generate(env, eb, RPL_MAX_COMPLETIONS_TO_TRY);
	// print_completions();
	check_completion(res_count, res_idx, res);
	clear_ebuf();
}


void
test_file_completion_apply(char *input, int pos, char *res)
{
	setup_ebuf(input, pos);
	completions_generate(env, eb, RPL_MAX_COMPLETIONS_TO_TRY);
	completions_apply(env->completions, 0, eb->input, eb->pos);
	check_completions_apply(res);
	clear_ebuf();
}


// rpl_public char *
// expand_envar(rpl_completion_env_t * cenv, const char *prefix)
// {
	// stringbuf_t *sbuf_prefix = sbuf_new(cenv->env->mem);
	// sbuf_append(sbuf_prefix, prefix);
	// printf("\norig: %s\n", sbuf_string(sbuf_prefix));
	// sbuf_expand_envars(sbuf_prefix);
	// printf("expanded: %s\n", sbuf_string(sbuf_prefix));
	// char *ret = sbuf_free_dup(sbuf_prefix);
	// return ret;
// }


void
setup(void)
{
	env = rpl_get_env();
	eb = create_editbuf(env, "%");
	system("mkdir -p testdir");
	char cmd[64] = {0};
	for (int i = 1; i <= 5; i++) {
		sprintf(cmd, "touch testdir/file_%02d", i);
		system(cmd);
	}
}


void
teardown(void)
{
	system("rm -rf testdir");
}


int
main(void)
{
	setup();

	for (int pos = 0; pos < strlen("tes"); pos++) {
		test_file_completion("tes", pos, 1, 0, "testdir/");
	}

	for (int pos = 0; pos < strlen("tes"); pos++) {
		test_file_completion_apply("tes", pos, "testdir/");
	}

	for (int pos = 0; pos < strlen("ls "); pos++) {
		test_file_completion_apply("ls tes", pos, "ls tes");
	}

	for (int pos = strlen("ls "); pos < strlen("ls tes"); pos++) {
		test_file_completion_apply("ls tes", pos, "ls testdir/");
	}

	for (int pos = strlen("testdir"); pos < strlen("testdir/file"); pos++) {
		test_file_completion("testdir/file", pos, 5, 0, "file_01");
	}

	for (int pos = strlen("test"); pos < strlen("testdir/file"); pos++) {
		test_file_completion_apply("testdir/file", pos, "testdir/file_01");
	}

	print_summary();
	// teardown();

	return EXIT_SUCCESS;
}

