#include "rc.h"
#include "fns.h"
#include "io.h"
#include "exec.h"
#include "getflags.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>

#include "repline.h"

#define DEBUG(f, a...) \
	if(0){}else{pfmt(err, "%s: "f,  __FUNCTION__, ## a);flush(err);}
	// if(0){}else{pfmt(err, "\n%s: "f,  __FUNCTION__, ## a);flush(err);}

#define MAX_PROMPT_LEN     (1024)
#define MAX_GITLINE_LEN    (1024)

/// TODO also allow rc scripts to generate prompt and completions

// static void
// complete_filename(rpl_completion_env_t* cenv, const char* prefix, char dir_sep, const char* roots, const char* extensions)
// {
  // if (roots == NULL) roots = ".";
  // if (extensions == NULL) extensions = "";
  // if (dir_sep == 0) dir_sep = rpl_dirsep();
  // filename_closure_t fclosure;
  // fclosure.dir_sep = dir_sep;
  // fclosure.roots = roots;
  // fclosure.extensions = extensions;
  // cenv->arg = &fclosure;
  // rpl_complete_qword_ex( cenv, prefix, &filename_completer, &rpl_char_is_filename_letter, '\\', "'\"");
// }

/*
static bool
get_gitstatus_porcv1(FILE *fp, char *branch, int *modcount)
{
	char gitline[MAX_GITLINE_LEN] = {0};
	if (fgets(gitline, sizeof(gitline), fp)) {
		// DEBUG("GIT LINE: %s\n", gitline);
		if (strstr("fatal: not a git repository", gitline)) return false;
		// DEBUG("FOUND GIT REPO\n");
		char *branch_start = strchr(gitline, ' ');
		char *branch_stop  = strchr(gitline, '.');
		if (!branch_stop) {
			// DEBUG("GIT REPO IS LOCAL ONLY, GITLINE LEN: %d\n", strlen(gitline));
			branch_stop = branch_start + strlen(gitline);
			// branch_stop = strchr(branch_start + 1, ' ') - 1;
			// if (!branch_stop) {
				// branch_stop = branch_start + strlen(gitline);
			// }
		}
		// else branch_stop--;
		branch_start++;
		memcpy(branch, branch_start, branch_stop - branch_start);
		// DEBUG("GIT BRANCH: %s\n", branch);
	} else return false;
	memset(gitline, 0, MAX_GITLINE_LEN);
	// DEBUG("GETTING next line ...\n");
	while (fgets(gitline, sizeof(gitline), fp)) {
		// DEBUG("GIT LINE: %s\n", gitline);
		(*modcount)++;
	}
	return true;
}
*/


static bool
get_gitbranch_detached(char *branch)
{
	FILE *fp = popen("git branch --no-color 2>/dev/null", "r");
	char gitline[MAX_GITLINE_LEN] = {0};
	while (fgets(gitline, sizeof(gitline), fp)) {
		// DEBUG("GIT LINE: %s", gitline);
		if (gitline[0] == '*') {
			ssize_t off = strlen("* (HEAD detached at ");
			memcpy(branch, gitline + off, strlen(gitline) - off - 2);
		}
	}
	pclose(fp);
	return true;
}


static bool
get_gitstatus_porcv2(FILE *fp, char *branch, char *commits, int *modcount)
{
	char gitline[MAX_GITLINE_LEN] = {0};
	while (fgets(gitline, sizeof(gitline), fp)) {
		// DEBUG("GIT LINE: %s", gitline);
		if (strncmp(gitline, "# branch.head", strlen("# branch.head")) == 0) {
			char *branch_start = strchr(gitline + strlen("# branch.head"), ' ');
			ssize_t branch_len  = strlen(gitline) - (branch_start - gitline) - 2;
			if (strncmp(branch_start + 1, "(detached)", strlen("(detached)")) == 0) {
				get_gitbranch_detached(branch);
			}
			else memcpy(branch, branch_start + 1, branch_len);
			// DEBUG("GIT LINE BRANCH: %s [%d]\n", branch, branch_len);
		}
		if (strncmp(gitline, "# branch.ab", strlen("# branch.ab")) == 0) {
			char *plus = strchr(gitline, '+');
			char *minus = strchr(gitline, '-');
			ssize_t commits_len = minus - plus - 2;
			// DEBUG("GIT LINE COMMITS: %s [%d]\n", plus, commits_len);
			memcpy(commits, plus + 1, commits_len);
		}
		if (gitline[0] != '#') (*modcount)++;
	}
	return true;
}


static char *
get_gitstatus(void)
{
	FILE *fp;
	char branch[MAX_GITLINE_LEN] = {0};
	char commits[8] = {0};
	int modcount = 0;
	char *ret = calloc(MAX_PROMPT_LEN, 1);
	int pret = 0;
	/// Maybe it's awkward to call the system shell, here ...
	fp = popen("git status --porcelain=v2 -b 2>/dev/null", "r");
	// fp = popen("/usr/bin/git status --porcelain -b 2>/dev/null", "r");
	// fp = popen("/usr/bin/git status --porcelain -b -z", "r");
	if (!fp) goto noavail;
	// if (!get_gitstatus_porcv1(fp, branch, &modcount)) goto noavail;
	if (!get_gitstatus_porcv2(fp, branch, commits, &modcount)) goto noavail;
	// DEBUG("GIT LINE STATUS: %s [%d], modcount: %d\n", branch, strlen(branch), modcount);
	if (strlen(commits) > 0 && strcmp(commits, "0") != 0) {
		if (modcount > 0) sprintf(ret, "(%s↑%s|%d)", branch, commits, modcount);
		else sprintf(ret, "(%s↑%s|✔)", branch, commits);
	} else {
		if (modcount > 0) sprintf(ret, "(%s|%d)", branch, modcount);
		else sprintf(ret, "(%s|✔)", branch);
	}
// cleanup:
	pret = pclose(fp);
	/// Return code of git status is an error, e.g. no git repo in this directory
	if (pret) {
		// DEBUG("COULDN'T FIND GIT REPO\n");
		free(ret);
		return NULL;
	}
	return ret;
noavail:
	// DEBUG("COULDN'T FIND GIT REPO\n");
	if (fp) pclose(fp);
	if (ret) free(ret);
	return NULL;
}


static char *
get_cwd(void)
{
	long path_max;
	size_t size;
	char *buf;
	char *ptr;
	path_max = pathconf(".", _PC_PATH_MAX);
	if (path_max == -1)
		size = 1024;
	else if (path_max > 10240)
		size = 10240;
	else
		size = path_max;
	for (buf = ptr = NULL; ptr == NULL; size *= 2)
	{
		if ((buf = realloc(buf, size)) == NULL) return NULL;
		ptr = getcwd(buf, size);
		if (ptr == NULL && errno != ERANGE) return NULL;
	}
	return buf;
}


static char *
get_prompt(void)
{
	char *prompt = calloc(MAX_PROMPT_LEN, 1);
	char *cwd = get_cwd();
	char *gitstatus = get_gitstatus();
	sprintf(prompt, "%s %s", cwd ? cwd : "", gitstatus ? gitstatus : "");
	if (cwd) free(cwd);
	if (gitstatus) free(gitstatus);
	return prompt;
}


static void
read_repline(void)
{
	static int first = 1;
	io *f = runq->cmdfd;
	char *s;
	long n;
	if (first){
		char hist_path[PATH_MAX];
#ifdef HISTORY
		sprintf(hist_path, HISTORY);
#else
		sprintf(hist_path, "%s/.history.db", getenv("HOME"));
#endif
		rpl_set_history(hist_path, -1); /// No limit to number of history entries
		rpl_set_prompt_marker(promptstr, "> ");
		rpl_enable_twoline_prompt(true);
		rpl_set_hint_delay(0);
		first = 0;
	}
	char *p = get_prompt();
	s = rpl_readline(p);
	free(p);
	if (s == NULL) {
		s = rpl_malloc(strlen("exit") + 1);
		strcpy(s, "exit");
		rpl_history_close();
	}
	n = strlen(s);
	assert(n < NBUF-1);
	strcpy(f->buf, s);
	f->buf[n++] = '\n';
	f->bufp = f->buf;
	f->ebuf = f->buf+n;
	rpl_free(s);
}


void
pprompt(void)
{
	var *prompt;
	if(runq->iflag){
		flush(err);
		read_repline();
		prompt = vlook("prompt");
		if(prompt->val && prompt->val->next)
			promptstr = prompt->val->next->word;
		else
			promptstr="\t";
	}
	runq->lineno++;
	doprompt = 0;
}


void
exechistory(void)
{
}
