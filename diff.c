#include "ll-merge.h"
	struct strbuf *header;
	if (textconv_one)
		free((char *)data_one);
	if (textconv_two)
		free((char *)data_two);
	if (ecbdata->header) {
		fprintf(ecbdata->file, "%s", ecbdata->header->buf);
		strbuf_reset(ecbdata->header);
		ecbdata->header = NULL;
	}
	int conflict_marker_size;
static int is_conflict_marker(const char *line, int marker_size, unsigned long len)
	if (len < marker_size + 1)
	case '=': case '>': case '<': case '|':
	for (cnt = 1; cnt < marker_size; cnt++)
	/* line[1] thru line[marker_size-1] are same as firstchar */
	if (len < marker_size + 1 || !isspace(line[marker_size]))
	int marker_size = data->conflict_marker_size;
		if (is_conflict_marker(line + 1, marker_size, len - 1)) {
	struct strbuf header = STRBUF_INIT;
				one->sha1, two->sha1, two->dirty_submodule,
	strbuf_addf(&header, "%sdiff --git %s %s%s\n", set, a_one, b_two, reset);
		strbuf_addf(&header, "%snew file mode %06o%s\n", set, two->mode, reset);
			strbuf_addf(&header, "%s%s%s\n", set, xfrm_msg, reset);
		strbuf_addf(&header, "%sdeleted file mode %06o%s\n", set, one->mode, reset);
			strbuf_addf(&header, "%s%s%s\n", set, xfrm_msg, reset);
			strbuf_addf(&header, "%sold mode %06o%s\n", set, one->mode, reset);
			strbuf_addf(&header, "%snew mode %06o%s\n", set, two->mode, reset);
			strbuf_addf(&header, "%s%s%s\n", set, xfrm_msg, reset);

			fprintf(o->file, "%s", header.buf);
			strbuf_reset(&header);
		fprintf(o->file, "%s", header.buf);
		strbuf_reset(&header);
		if (!DIFF_XDL_TST(o, WHITESPACE_FLAGS)) {
			fprintf(o->file, "%s", header.buf);
			strbuf_reset(&header);
		}

		ecbdata.header = header.len ? &header : NULL;
	strbuf_release(&header);
	data.conflict_marker_size = ll_merge_marker_size(attr_path);
	if ((ce->ce_flags & CE_VALID) || ce_skip_worktree(ce))
	char *data = xmalloc(100), *dirty = "";

	/* Are we looking at the work tree? */
	if (s->dirty_submodule)
		dirty = "-dirty";

		       "Subproject commit %s%s\n", sha1_to_hex(s->sha1), dirty);
	retval = run_command_v_opt(spawn_arg, RUN_USING_SHELL);
	/*
	 * Most of the time we can say "there are changes"
	 * only by checking if there are changed paths, but
	 * --ignore-whitespace* options force us to look
	 * inside contents.
	 */

	if (DIFF_XDL_TST(options, IGNORE_WHITESPACE) ||
	    DIFF_XDL_TST(options, IGNORE_WHITESPACE_CHANGE) ||
	    DIFF_XDL_TST(options, IGNORE_WHITESPACE_AT_EOL))
		DIFF_OPT_SET(options, DIFF_FROM_CONTENTS);
	else
		DIFF_OPT_CLR(options, DIFF_FROM_CONTENTS);

	/*
	 * When patches are generated, submodules diffed against the work tree
	 * must be checked for dirtiness too so it can be shown in the output
	 */
	if (options->output_format & DIFF_FORMAT_PATCH)
		DIFF_OPT_SET(options, DIRTY_SUBMODULES);
	if (DIFF_OPT_TST(options, QUICK)) {
	else if (!prefixcmp(arg, "--color=")) {
		int value = git_config_colorbool(NULL, arg+8, -1);
		if (value == 0)
			DIFF_OPT_CLR(options, COLOR_DIFF);
		else if (value > 0)
			DIFF_OPT_SET(options, COLOR_DIFF);
		else
			return error("option `color' expects \"always\", \"auto\", or \"never\"");
	}
		DIFF_OPT_SET(options, QUICK);
	    !hashcmp(one->sha1, two->sha1) &&
	    !one->dirty_submodule && !two->dirty_submodule)
			 p->one->dirty_submodule ||
			 p->two->dirty_submodule ||
	if (output_format & DIFF_FORMAT_NO_OUTPUT &&
	    DIFF_OPT_TST(options, EXIT_WITH_STATUS) &&
	    DIFF_OPT_TST(options, DIFF_FROM_CONTENTS)) {
		/*
		 * run diff_flush_patch for the exit status. setting
		 * options->file to /dev/null should be safe, becaue we
		 * aren't supposed to produce any output anyway.
		 */
		if (options->close_file)
			fclose(options->file);
		options->file = fopen("/dev/null", "w");
		if (!options->file)
			die_errno("Could not open /dev/null");
		options->close_file = 1;
		for (i = 0; i < q->nr; i++) {
			struct diff_filepair *p = q->queue[i];
			if (check_pair_status(p))
				diff_flush_patch(p, options);
			if (options->found_changes)
				break;
		}
	}


	/*
	 * Report the content-level differences with HAS_CHANGES;
	 * diff_addremove/diff_change does not set the bit when
	 * DIFF_FROM_CONTENTS is in effect (e.g. with -w).
	 */
	if (DIFF_OPT_TST(options, DIFF_FROM_CONTENTS)) {
		if (options->found_changes)
			DIFF_OPT_SET(options, HAS_CHANGES);
		else
			DIFF_OPT_CLR(options, HAS_CHANGES);
	}
		 * 1. Entries that come from stat info dirtiness
static int diffnamecmp(const void *a_, const void *b_)
{
	const struct diff_filepair *a = *((const struct diff_filepair **)a_);
	const struct diff_filepair *b = *((const struct diff_filepair **)b_);
	const char *name_a, *name_b;

	name_a = a->one ? a->one->path : a->two->path;
	name_b = b->one ? b->one->path : b->two->path;
	return strcmp(name_a, name_b);
}

void diffcore_fix_diff_index(struct diff_options *options)
{
	struct diff_queue_struct *q = &diff_queued_diff;
	qsort(q->queue, q->nr, sizeof(q->queue[0]), diffnamecmp);
}

	if (diff_queued_diff.nr && !DIFF_OPT_TST(options, DIFF_FROM_CONTENTS))
		    const char *concatpath, unsigned dirty_submodule)
	if (addremove != '-') {
		two->dirty_submodule = dirty_submodule;
	}
	if (!DIFF_OPT_TST(options, DIFF_FROM_CONTENTS))
		DIFF_OPT_SET(options, HAS_CHANGES);
		 const char *concatpath,
		 unsigned old_dirty_submodule, unsigned new_dirty_submodule)
		tmp = old_dirty_submodule; old_dirty_submodule = new_dirty_submodule;
			new_dirty_submodule = tmp;
	one->dirty_submodule = old_dirty_submodule;
	two->dirty_submodule = new_dirty_submodule;
	if (!DIFF_OPT_TST(options, DIFF_FROM_CONTENTS))
		DIFF_OPT_SET(options, HAS_CHANGES);
	int err = 0;
	child.use_shell = 1;
	if (start_command(&child)) {

	if (strbuf_read(&buf, child.out, 0) < 0)
		err = error("error reading from textconv command '%s'", pgm);

	if (finish_command(&child) || err) {
		strbuf_release(&buf);
		remove_tempfile();
		return NULL;
	}