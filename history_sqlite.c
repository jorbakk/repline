#include <stdio.h>
#include <stdlib.h>
/// TODO include header on windows for getting the pid of the process
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>

#include "repline.h"
#include "common.h"
#include "history.h"
#include "stringbuf.h"

struct db_t {
	sqlite3 *dbh;
	sqlite3_stmt **stmts;
	size_t stmtcnt;
};

struct db_query_t {
	unsigned int id;
	const char *query;
};

struct history_s {
	const char *fname;          // history file
	struct db_t db;
	alloc_t *mem;
};

static const char *db_tables[] = {
	"create table if not exists cmds (cid integer, ts integer, pid integer, cmd text)",
	"create index if not exists cmdididx on cmds(cid, ts, pid)",
	"create index if not exists cmdtxtidx on cmds(pid, cmd)",
	"create index if not exists cmdidx on cmds(cmd)",
	NULL
};

enum db_rc {
	DB_ERROR = 0,
	DB_OK,
	DB_ROW,
};

enum db_stmt {
	DB_INS_CMD,
	DB_MAX_ID_CMD,
	DB_COUNT_CMD,
	DB_GET_PREV_CNT,
	DB_GET_PREV_CMD,
	DB_GET_PREF_CNT,
	DB_GET_PREF_CMD,
	DB_GET_DBL_PIDS,
	DB_GET_CMD_ID,
	DB_DEL_CMD_ID,
	DB_DEL_ALL,
	DB_UPD_TS,
	DB_SET_PID_NULL,
	DB_STMT_CNT,
};

static const struct db_query_t db_queries[] = {
	{DB_INS_CMD, "insert into cmds values (?,?,?,?)"},
	{DB_MAX_ID_CMD, "select max(cid) from cmds"},
	{DB_COUNT_CMD, "select count(cid) from cmds"},
	{DB_GET_PREV_CNT,
	 "select count(cmd) from cmds where pid = ?"},
	{DB_GET_PREV_CMD,
	 "select cmd from cmds where pid = ? order by pid desc, ts desc, cid desc limit 1 offset ?"},
	{DB_GET_PREF_CNT, "select count(cmd) from cmds where cmd like ?"},
	{DB_GET_PREF_CMD,
	 "select cmd, max(ts) as mts, max(cid) as mcid from cmds where cmd like ? group by cmd order by mts desc, mcid desc limit 1 offset ?"},
	{DB_GET_DBL_PIDS,
	 "select cpid.cid, cpid.ts, cnull.cid, cnull.ts from cmds as cpid, cmds as cnull where cpid.pid = ? and cnull.pid is NULL and cpid.cmd = cnull.cmd"},
	{DB_GET_CMD_ID, "select cid, pid from cmds where cmd = ? and pid = ? order by pid desc, ts desc, cid desc limit 1"},
	{DB_DEL_CMD_ID, "delete from cmds where cid = ?"},
	{DB_DEL_ALL, "delete from cmds"},
	{DB_UPD_TS, "update cmds set ts = ? where cid = ?"},
	{DB_SET_PID_NULL, "update cmds set pid = NULL where pid = ?"},
	{DB_STMT_CNT, ""},
};

int
db_rc(int rc)
{
	switch (rc) {
	case SQLITE_ERROR:
		return DB_ERROR;
	case SQLITE_OK:
		return DB_OK;
	case SQLITE_ROW:
		return DB_ROW;
	}
	return DB_ERROR;
}

static bool
db_exec_str(struct db_t *db, const char *query)
{
	debug_msg("%s\n", query);
	char *err_msg = 0;
	int rc = sqlite3_exec(db->dbh, query, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		debug_msg("SQL error %s in statement:\n", err_msg);
		sqlite3_free(err_msg);
		return false;
	}
	return true;
}

static int
db_open(struct db_t *db, const char *fname)
{
	int rc = db_rc(sqlite3_open(fname, &db->dbh));
	if (rc != DB_OK) {
		debug_msg("Cannot open database %s: %s\n", fname,
		          sqlite3_errmsg(db->dbh));
	}
	/// Turn on case sensitive queries with like operator when searching the history
	db_exec_str(db, "PRAGMA case_sensitive_like = true;");
	return rc;
}

static int
db_close(struct db_t *db)
{
	int rc = sqlite3_close(db->dbh);
	if (rc != SQLITE_OK)
		return DB_ERROR;
	return DB_OK;
}

static bool
create_tables(struct db_t *db)
{
	for (int i = 0; db_tables[i]; i++) {
		db_exec_str(db, db_tables[i]);
	}
	return true;
}

static int
db_prepare_stmts(struct db_t *db, const struct db_query_t db_queries[],
                 size_t n)
{
	int rc = DB_OK;
	db->stmts = calloc(n, sizeof(sqlite3_stmt *));
	db->stmtcnt = n;
	for (size_t i = 0; i < n; i++) {
		if (db_queries[i].id != i) {
			debug_msg("db query strings and ids are inconsistent\n");
			exit(EXIT_FAILURE);
		}
		db->stmts[i] = calloc(1, sizeof(sqlite3_stmt *));
		rc = db_rc(sqlite3_prepare_v2
		           (db->dbh, db_queries[i].query, -1, &db->stmts[i], 0));
		if (rc != DB_OK) {
			debug_msg("failed to prepare statement with stmt idx %ld:\n%s\n", i,
			          db_queries[i].query);
			debug_msg("%s\n", sqlite3_errmsg(db->dbh));
			break;
		}
	}
	return rc;
}

static int
db_free_stmts(struct db_t *db)
{
	int rc = DB_OK;
	for (size_t i = 0; i < db->stmtcnt; i++) {
		rc = db_rc(sqlite3_finalize(db->stmts[i]));
		free(db->stmts[i]);
		if (rc != DB_OK)
			break;
	}
	free(db->stmts);
	return rc;
}

static int
db_exec(const struct db_t *db, int stmt)
{
	return db_rc(sqlite3_step(db->stmts[stmt]));
}

static int
db_in_int(const struct db_t *db, int stmt, int pos, int val)
{
	return db_rc(sqlite3_bind_int(db->stmts[stmt], pos, val));
}

static int
db_in_txt(const struct db_t *db, int stmt, int pos, const char *val)
{
	return db_rc(sqlite3_bind_text(db->stmts[stmt], pos, val, -1, NULL));
}

static int
db_out_int(const struct db_t *db, int stmt, int pos)
{
	return sqlite3_column_int(db->stmts[stmt], pos - 1);
}

static const unsigned char *
db_out_txt(const struct db_t *db, int stmt, int pos)
{
	return sqlite3_column_text(db->stmts[stmt], pos - 1);
}

static int
db_reset(const struct db_t *db, int stmt)
{
	return db_rc(sqlite3_reset(db->stmts[stmt]));
}

rpl_private history_t *
history_new(alloc_t * mem)
{
	history_t *h = mem_zalloc_tp(mem, history_t);
	h->mem = mem;
	return h;
}

rpl_private void
history_free(history_t * h)
{
	if (h == NULL)
		return;
	mem_free(h->mem, h->fname);
	h->fname = NULL;
	mem_free(h->mem, h);        // free ourselves
}

rpl_private ssize_t
history_count_with_prefix(const history_t * h, const char *prefix)
{
	if (strlen(prefix) == 0) {
		db_in_int(&h->db, DB_GET_PREV_CNT, 1, getpid());
		db_exec(&h->db, DB_GET_PREV_CNT);
		int count = db_out_int(&h->db, DB_GET_PREV_CNT, 1);
		db_reset(&h->db, DB_GET_PREV_CNT);
		return count;
	}
	char *prefix_param = mem_malloc(h->mem, rpl_strlen(prefix) + 3);
	sprintf(prefix_param, "%s%%", prefix);
	db_in_txt(&h->db, DB_GET_PREF_CNT, 1, prefix_param);
	db_exec(&h->db, DB_GET_PREF_CNT);
	int count = db_out_int(&h->db, DB_GET_PREF_CNT, 1);
	db_reset(&h->db, DB_GET_PREF_CNT);
	mem_free(h->mem, prefix_param);
	return count;
}

//-------------------------------------------------------------
// push/clear
//-------------------------------------------------------------

static time_t
get_current_ts(void)
{
	struct timespec curtime;
	clock_gettime(CLOCK_REALTIME, &curtime);
	return curtime.tv_sec;
}

rpl_private bool
history_push(history_t * h, const char *entry)
{
	if (entry == NULL || rpl_strlen(entry) == 0)
		return false;

	db_in_txt(&h->db, DB_GET_CMD_ID, 1, entry);
	db_in_int(&h->db, DB_GET_CMD_ID, 2, getpid());
	int cid = -1;
	int pid = -1;
	if (db_exec(&h->db, DB_GET_CMD_ID) == DB_ROW) {;
		cid = db_out_int(&h->db, DB_GET_CMD_ID, 1);
		pid = db_out_int(&h->db, DB_GET_CMD_ID, 2);
	}
	db_reset(&h->db, DB_GET_CMD_ID);
	/// Update timestamp only if the command is entered by the same process.
	/// Otherwise, time stamps will be updated in history_close().
	if (cid != -1 && pid == getpid()) {
		debug_msg("duplicate history entry (cid=%d, pid=%d), updating timestamp: %s\n", cid, pid, entry);
		db_in_int(&h->db, DB_UPD_TS, 1, get_current_ts());
		db_in_int(&h->db, DB_UPD_TS, 2, cid);
		db_exec(&h->db, DB_UPD_TS);
		db_reset(&h->db, DB_UPD_TS);
		return false;
	}
	debug_msg("new history entry (cid=%d, pid=%d): %s\n", cid, pid, entry);
	/// If command is new or in global history, create a new entry in history for this command,
	/// tagged with the current pid.
	db_exec(&h->db, DB_MAX_ID_CMD);
	int new_cid = db_out_int(&h->db, DB_MAX_ID_CMD, 1) + 1;
	db_reset(&h->db, DB_MAX_ID_CMD);

	db_in_int(&h->db, DB_INS_CMD, 1, new_cid);
	db_in_int(&h->db, DB_INS_CMD, 2, get_current_ts());
	db_in_int(&h->db, DB_INS_CMD, 3, getpid());
	db_in_txt(&h->db, DB_INS_CMD, 4, entry);
	db_exec(&h->db, DB_INS_CMD);
	db_reset(&h->db, DB_INS_CMD);
	return true;
}

rpl_private void
history_remove_last(history_t * h)
{
	db_exec(&h->db, DB_MAX_ID_CMD);
	int last_cid = db_out_int(&h->db, DB_MAX_ID_CMD, 1);
	db_reset(&h->db, DB_MAX_ID_CMD);

	db_in_int(&h->db, DB_DEL_CMD_ID, 1, last_cid);
	db_exec(&h->db, DB_DEL_CMD_ID);
	db_reset(&h->db, DB_DEL_CMD_ID);
}

rpl_private void
history_clear(history_t * h)
{
	db_exec(&h->db, DB_DEL_ALL);
	db_reset(&h->db, DB_DEL_ALL);
}

rpl_private void
history_close(history_t * h)
{
	/// Get all double entries with pid == NULL and pid == getpid()
	db_exec_str(&h->db, "BEGIN TRANSACTION");
	db_in_int(&h->db, DB_GET_DBL_PIDS, 1, getpid());
	while (db_exec(&h->db, DB_GET_DBL_PIDS) == DB_ROW) {
		int cpid_cid = db_out_int(&h->db, DB_GET_DBL_PIDS, 1);
		unsigned int cpid_ts = db_out_int(&h->db, DB_GET_DBL_PIDS, 2);
		int cnull_cid = db_out_int(&h->db, DB_GET_DBL_PIDS, 3);
		unsigned int cnull_ts = db_out_int(&h->db, DB_GET_DBL_PIDS, 4);
		debug_msg("merge double entry cid: %d, ts: %d with cid: %d, ts: %d\n",
		          cpid_cid, cpid_ts, cnull_cid, cnull_ts);
		int max_ts = cpid_ts > cnull_ts ? cpid_ts : cnull_ts;
		db_in_int(&h->db, DB_UPD_TS, 1, max_ts);
		db_in_int(&h->db, DB_UPD_TS, 2, cnull_cid);
		db_exec(&h->db, DB_UPD_TS);
		db_reset(&h->db, DB_UPD_TS);
		db_in_int(&h->db, DB_DEL_CMD_ID, 1, cpid_cid);
		db_exec(&h->db, DB_DEL_CMD_ID);
		db_reset(&h->db, DB_DEL_CMD_ID);
	}
	db_reset(&h->db, DB_GET_DBL_PIDS);
	/// Set pid of history of the current process to NULL
	db_in_int(&h->db, DB_SET_PID_NULL, 1, getpid());
	db_exec(&h->db, DB_SET_PID_NULL);
	db_reset(&h->db, DB_SET_PID_NULL);
	db_exec_str(&h->db, "COMMIT");
	db_close(&h->db);
}

/// Parameter n is the history command index from latest to oldest, starting with 1
/// NOTE need to free the returned string at the callsite with this backend
rpl_private const char *
history_get_with_prefix(const history_t * h, ssize_t n, const char *prefix)
{
	if (n <= 0)
		return NULL;
	if (strlen(prefix) == 0) {
		db_in_int(&h->db, DB_GET_PREV_CNT, 1, getpid());
		db_exec(&h->db, DB_GET_PREV_CNT);
		int cnt = db_out_int(&h->db, DB_GET_PREV_CNT, 1);
		db_reset(&h->db, DB_GET_PREV_CNT);
		if (n < 0 || n > cnt) return NULL;
		db_in_int(&h->db, DB_GET_PREV_CMD, 1, getpid());
		db_in_int(&h->db, DB_GET_PREV_CMD, 2, n - 1);
		int ret = db_exec(&h->db, DB_GET_PREV_CMD);
		if (ret != DB_ROW) {
			db_reset(&h->db, DB_GET_PREV_CMD);
			return NULL;
		}
		const char *entry =
		    mem_strdup(h->mem,
		               (const char *)db_out_txt(&h->db, DB_GET_PREV_CMD, 1));
		db_reset(&h->db, DB_GET_PREV_CMD);
		return entry;
	}
	char *prefix_param = mem_malloc(h->mem, rpl_strlen(prefix) + 3);
	sprintf(prefix_param, "%s%%", prefix);
	db_in_txt(&h->db, DB_GET_PREF_CNT, 1, prefix_param);
	db_exec(&h->db, DB_GET_PREF_CNT);
	int cnt = db_out_int(&h->db, DB_GET_PREF_CNT, 1);
	db_reset(&h->db, DB_GET_PREF_CNT);
	if (n < 0 || n > cnt) {
		mem_free(h->mem, prefix_param);
		return NULL;
	}
	db_in_txt(&h->db, DB_GET_PREF_CMD, 1, prefix_param);
	db_in_int(&h->db, DB_GET_PREF_CMD, 2, n - 1);
	int ret = db_exec(&h->db, DB_GET_PREF_CMD);
	if (ret != DB_ROW) {
		db_reset(&h->db, DB_GET_PREF_CMD);
		mem_free(h->mem, prefix_param);
		return NULL;
	}
	const char *entry =
	    mem_strdup(h->mem,
	               (const char *)db_out_txt(&h->db, DB_GET_PREF_CMD, 1));
	db_reset(&h->db, DB_GET_PREF_CMD);
	mem_free(h->mem, prefix_param);
	return entry;
}

//-------------------------------------------------------------
// save/load history to file
//-------------------------------------------------------------

rpl_private void
history_load_from(history_t * h, const char *fname, long max_entries)
{
	rpl_unused(max_entries);
	h->fname = mem_strdup(h->mem, fname);
	if (db_open(&h->db, h->fname) != DB_OK)
		return;
	if (!create_tables(&h->db))
		return;
	db_prepare_stmts(&h->db, db_queries, DB_STMT_CNT);
}

/// function history_load() is not needed ...
rpl_private void
history_load(history_t * h)
{
	rpl_unused(h);
}

/// function history_save() is not needed with this backend
rpl_private void
history_save(const history_t * h)
{
	rpl_unused(h);
}
