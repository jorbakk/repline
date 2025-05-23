//-------------------------------------------------------------
// Completion menu: this file is included in editline.c
//-------------------------------------------------------------

// return true if anything changed
static bool
edit_complete(rpl_env_t *env, editor_t *eb, ssize_t idx)
{
	editor_start_modify(eb);
	ssize_t newpos = completions_apply(env->completions, idx, eb->input, eb->pos);
	if (newpos < 0) {
		editor_undo_restore(eb, false);
		return false;
	}
	eb->pos = newpos;
	sbuf_clear(eb->hint);
	edit_refresh(env, eb);
	return true;
}


rpl_private void
sbuf_append_tagged(stringbuf_t *sb, const char *tag, const char *content)
{
	sbuf_appendf(sb, "[%s]", tag);
	sbuf_append(sb, content);
	sbuf_append(sb, "[/]");
}

static void
editor_append_completion(rpl_env_t *env, editor_t *eb, ssize_t idx,
                         ssize_t width, bool numbered, bool selected)
{
	const char *help = NULL;
	const char *display = completions_get_display(env->completions, idx, &help);
	if (display == NULL)
		return;
	if (numbered) {
		sbuf_appendf(eb->extra, "[rpl-info]%s%zd [/]",
		             (selected ? (tty_is_utf8(env->tty) ? "\xE2\x86\x92" : "*")
		              : " "), 1 + idx);
		width -= 3;
	}

	if (width > 0) {
		sbuf_appendf(eb->extra, "[width=\"%zd;left; ;on\"]", width);
	}
	if (selected) {
		sbuf_append(eb->extra, "[rpl-emphasis]");
	}
	sbuf_append(eb->extra, display);
	if (selected) {
		sbuf_append(eb->extra, "[/rpl-emphasis]");
	}
	if (help != NULL) {
		sbuf_append(eb->extra, "  ");
		sbuf_append_tagged(eb->extra, "rpl-info", help);
	}
	if (width > 0) {
		sbuf_append(eb->extra, "[/width]");
	}
}

// 2 and 3 column output up to 80 wide
#define RPL_DISPLAY2_MAX    34
#define RPL_DISPLAY2_COL    (3+RPL_DISPLAY2_MAX)
#define RPL_DISPLAY2_WIDTH  (2*RPL_DISPLAY2_COL + 2)    // 75

#define RPL_DISPLAY3_MAX    21
#define RPL_DISPLAY3_COL    (3+RPL_DISPLAY3_MAX)
#define RPL_DISPLAY3_WIDTH  (3*RPL_DISPLAY3_COL + 2*2)  // 76

static void
editor_append_completion2(rpl_env_t *env, editor_t *eb, ssize_t col_width,
                          ssize_t idx1, ssize_t idx2, ssize_t selected)
{
	editor_append_completion(env, eb, idx1, col_width, true,
	                         (idx1 == selected));
	sbuf_append(eb->extra, "  ");
	editor_append_completion(env, eb, idx2, col_width, true,
	                         (idx2 == selected));
}

static void
editor_append_completion3(rpl_env_t *env, editor_t *eb, ssize_t col_width,
                          ssize_t idx1, ssize_t idx2, ssize_t idx3,
                          ssize_t selected)
{
	editor_append_completion(env, eb, idx1, col_width, true,
	                         (idx1 == selected));
	sbuf_append(eb->extra, "  ");
	editor_append_completion(env, eb, idx2, col_width, true,
	                         (idx2 == selected));
	sbuf_append(eb->extra, "  ");
	editor_append_completion(env, eb, idx3, col_width, true,
	                         (idx3 == selected));
}

static ssize_t
edit_completions_max_width(rpl_env_t *env, ssize_t count)
{
	ssize_t max_width = 0;
	for (ssize_t i = 0; i < count; i++) {
		const char *help = NULL;
		ssize_t w =
		    bbcode_column_width(env->bbcode,
		                        completions_get_display(env->completions, i,
		                                                &help));
		if (help != NULL) {
			w += 2 + bbcode_column_width(env->bbcode, help);
		}
		if (w > max_width) {
			max_width = w;
		}
	}
	return max_width;
}

static void
edit_completion_menu(rpl_env_t *env, editor_t *eb, bool more_available)
{
	ssize_t count = completions_count(env->completions);
	ssize_t count_displayed = count;
	assert(count > 1);
	ssize_t selected = (env->complete_nopreview ? 0 : -1);  // select first or none
	ssize_t percolumn = count;

 again:
	// show first 9 (or 8) completions
	sbuf_clear(eb->extra);
	ssize_t twidth = term_get_width(env->term) - 1;
	ssize_t colwidth;
	if (count > 3
	    && ((colwidth = 3 + edit_completions_max_width(env, 9)) * 3 + 2 * 2) <
	    twidth) {
		// display as a 3 column block
		count_displayed = (count > 9 ? 9 : count);
		percolumn = 3;
		for (ssize_t rw = 0; rw < percolumn; rw++) {
			if (rw > 0)
				sbuf_append(eb->extra, "\n");
			editor_append_completion3(env, eb, colwidth, rw, percolumn + rw,
			                          (2 * percolumn) + rw, selected);
		}
	} else if (count > 4
	           && ((colwidth = 3 + edit_completions_max_width(env, 8)) * 2 +
	               2) < twidth) {
		// display as a 2 column block if some entries are too wide for three columns
		count_displayed = (count > 8 ? 8 : count);
		percolumn = (count_displayed <= 6 ? 3 : 4);
		for (ssize_t rw = 0; rw < percolumn; rw++) {
			if (rw > 0)
				sbuf_append(eb->extra, "\n");
			editor_append_completion2(env, eb, colwidth, rw, percolumn + rw,
			                          selected);
		}
	} else {
		// display as a list
		count_displayed = (count > 9 ? 9 : count);
		percolumn = count_displayed;
		for (ssize_t i = 0; i < count_displayed; i++) {
			if (i > 0)
				sbuf_append(eb->extra, "\n");
			editor_append_completion(env, eb, i, -1, true /* numbered */ ,
			                         selected == i);
		}
	}
	if (count > count_displayed) {
		if (more_available) {
			sbuf_append(eb->extra,
			            "\n[rpl-info](press page-down (or ctrl-j) to see all further completions)[/]");
		} else {
			sbuf_appendf(eb->extra,
			             "\n[rpl-info](press page-down (or ctrl-j) to see all %zd completions)[/]",
			             count);
		}
	}
	if (!env->complete_nopreview && selected >= 0
	    && selected <= count_displayed) {
		edit_complete(env, eb, selected);
		editor_undo_restore(eb, false);
	} else {
		edit_refresh(env, eb);
	}

	// read here; if not a valid key, push it back and return to main event loop
	code_t c = tty_read(env->tty);
	if (tty_term_resize_event(env->tty)) {
		edit_resize(env, eb);
	}
	sbuf_clear(eb->extra);

	// direct selection?
	if (c >= WITH_ALT('1') && c <= WITH_ALT('9')) {
		ssize_t i = (c - WITH_ALT('1'));
		if (i < count) {
			selected = i;
			c = KEY_ENTER;
		}
	}

	// process commands
	if (c == KEY_DOWN || c == KEY_TAB) {
		selected++;
		if (selected >= count_displayed) {
			//term_beep(env->term);
			selected = 0;
		}
		sbuf_clear(eb->hint);
		goto again;
	} else if (c == KEY_UP || c == KEY_SHIFT_TAB) {
		selected--;
		if (selected < 0) {
			selected = count_displayed - 1;
			//term_beep(env->term);
		}
		goto again;
	} else if (c == KEY_F1) {
		edit_show_help(env, eb);
		goto again;
	} else if (c == KEY_ESC) {
		completions_clear(env->completions);
		edit_refresh(env, eb);
		c = 0;                  // ignore and return
	} else if (selected >= 0 && (c == KEY_ENTER || c == KEY_RIGHT || c == KEY_END)) {   /* || c == KEY_TAB */
		// select the current entry
		assert(selected < count);
		c = 0;
		edit_complete(env, eb, selected);
	} else if (!env->complete_nopreview && !code_is_virt_key(c)) {
		// if in preview mode, select the current entry and exit the menu
		assert(selected < count);
		edit_complete(env, eb, selected);
	} else if ((c == KEY_PAGEDOWN || c == KEY_LINEFEED) && count > 9) {
		// show all completions
		c = 0;
		if (more_available) {
			// generate all entries (up to the max (= 1000))
		    completions_generate(env, eb,
		                         RPL_MAX_COMPLETIONS_TO_SHOW);
			count = completions_count(env->completions);
		}
		rowcol_t rc;
		edit_get_rowcol(env, eb, &rc);
		edit_clear(env, eb);
		edit_write_prompt(env, eb, 0, false, true);
		term_writeln(env->term, "");
		for (ssize_t i = 0; i < count; i++) {
			const char *display =
			    completions_get_display(env->completions, i, NULL);
			if (display != NULL) {
				bbcode_println(env->bbcode, display);
			}
		}
		if (count >= RPL_MAX_COMPLETIONS_TO_SHOW) {
			bbcode_println(env->bbcode, "[rpl-info]... and more.[/]");
		} else {
			bbcode_printf(env->bbcode,
			              "[rpl-info](%zd possible completions)[/]\n", count);
		}
		for (ssize_t i = 0; i < rc.row + 1; i++) {
			term_write(env->term, " \n");
		}
		eb->cur_rows = 0;
		edit_refresh(env, eb);
	} else {
		edit_refresh(env, eb);
	}
	// done
	completions_clear(env->completions);
	if (c != 0)
		tty_code_pushback(env->tty, c);
}


static void
edit_generate_completions(rpl_env_t *env, editor_t *eb)
{
	// printf("edit buffer before: '%s', pos: %ld\n", sbuf_string(eb->input), eb->pos);
	completions_generate(env, eb, RPL_MAX_COMPLETIONS_TO_TRY);
	// print_completions(env);
	bool more_available = (completions_count(env->completions) >= RPL_MAX_COMPLETIONS_TO_TRY);
	if (completions_count(env->completions) <= 0) {
		// no completions
		term_beep(env->term);
	} else if (completions_count(env->completions) == 1) {
		// complete if only one match
		edit_complete(env, eb, 0);
	} else {
		sbuf_clear(eb->hint);
		completions_sort(env->completions);
		edit_completion_menu(env, eb, more_available);
	}
	// printf("edit buffer after: \"%s\", pos: %ld\n\n", sbuf_string(eb->input), eb->pos);
}
