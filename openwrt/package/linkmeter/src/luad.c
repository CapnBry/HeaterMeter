/*

  luad.c - A libdaemon-based generic daemonization framework for Lua code.

  Copyright (C) 2003-2008 Lennart Poettering
                2011 Bart Van Der Meerssche <bart.vandermeerssche@flukso.net>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


/* Enable GNU extensions so we can use asprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#define DAEMON_USER   "daemon"
#define DAEMON_GROUP  "daemon"

#define DAEMON_VARRUN "/var/run"

#define SUP_ALLOWED_RESTARTS 10
#define SUP_MAX_SECONDS 60

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

struct restart_s {
	int i;
	time_t time[SUP_ALLOWED_RESTARTS];
};

static void restart_init(struct restart_s *restart) {
	restart->i = 0;

	int i;
	for (i = 0; i < SUP_ALLOWED_RESTARTS; i++) {
		restart->time[i] = 0;
	}
}

static void restart_add(struct restart_s *restart, time_t new_restart) {
	restart->time[restart->i++] = new_restart;
	restart->i %= SUP_ALLOWED_RESTARTS;
}

static int restart_max(struct restart_s *restart) {
	int i, total;

	total = 0;
	for (i = 0; i < SUP_ALLOWED_RESTARTS; i++) {
		if (restart->time[i] > time(NULL) - SUP_MAX_SECONDS) {
			total++;
		}
	}

	if (total == SUP_ALLOWED_RESTARTS) {
		return 1;
	}
	else {
		return 0;
	}
}

static void sigterm(int signo)
{
	daemon_log(LOG_INFO, "Caught a SIGTERM. Exiting... ");
	daemon_pid_file_remove();	
	exit(0);
}

static const char *daemon_pid_file_proc_override(void)
{
	char *fn;

	asprintf(&fn, "%s/%s/pid", DAEMON_VARRUN, daemon_log_ident);
	return fn;
}

static int drop_root(void)
{
	struct passwd *pw;
	struct group * gr;

	if (!(pw = getpwnam(DAEMON_USER))) {
		daemon_log(LOG_ERR, "Failed to find user '"DAEMON_USER"'");
		return -1;
	}

	if (!(gr = getgrnam(DAEMON_GROUP))) {
		daemon_log(LOG_ERR, "Failed to find group '"DAEMON_GROUP"'");
		return -1;
	}

	if (initgroups(DAEMON_USER, gr->gr_gid) != 0) {
		daemon_log(LOG_ERR, "Failed to change group list: %s", strerror(errno));
		return -1;
	}

	if (setregid(gr->gr_gid, gr->gr_gid) < 0) {
		daemon_log(LOG_ERR, "Failed to change GID: %s", strerror(errno));
		return -1;
	}

	if (setreuid(pw->pw_uid, pw->pw_uid) < 0) {
		daemon_log(LOG_ERR, "Failed to change UID: %s", strerror(errno));
		return -1;
	}

	setenv("USER", pw->pw_name, 1);
	setenv("LOGNAME", pw->pw_name, 1);
	setenv("HOME", pw->pw_dir, 1);

	return 0;
}

static int make_runtime_dir(char **pruntime_path)
{
	int r = -1;
	mode_t u;
	int reset_umask = 0;

	struct passwd *pw;
	struct group * gr;
	struct stat st;

	asprintf(pruntime_path, "%s/%s", DAEMON_VARRUN, daemon_log_ident);

	if (!(pw = getpwnam(DAEMON_USER))) {
		daemon_log(LOG_ERR, "Failed to find user '"DAEMON_USER"'");
		goto fail;
	}

	if (!(gr = getgrnam(DAEMON_GROUP))) {
		daemon_log(LOG_ERR, "Failed to find group '"DAEMON_GROUP"'");
		goto fail;
	}

	u = umask(0000);
	reset_umask = 1;

	if (mkdir(*pruntime_path, 0755) < 0 && errno != EEXIST) {
		daemon_log(LOG_ERR, "mkdir(\"%s\"): %s", *pruntime_path, strerror(errno));
		goto fail;
	}

	chown(*pruntime_path, pw->pw_uid, gr->gr_gid);

	if (stat(*pruntime_path, &st) < 0) {
		daemon_log(LOG_ERR, "stat(\"%s\"): %s\n", *pruntime_path, strerror(errno));
		goto fail;
	}

	if (!S_ISDIR(st.st_mode) || st.st_uid != pw->pw_uid || st.st_gid != gr->gr_gid) {
		daemon_log(LOG_ERR, "Failed to create runtime directory \"%s\"", *pruntime_path);
		goto fail;
	}

	r = 0;

fail:
	if (reset_umask)
		umask(u);
	return r;
}

int main(int argc, char *argv[])
{
	pid_t pid;
	struct sigaction sa;

	lua_State *L = NULL;
	char *luad_path = NULL;
	char *runtime_path = NULL;

	/* Reset signal handlers */
	if (daemon_reset_sigs(-1) < 0) {
		daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s", strerror(errno));
		return 1;
	}

	/* Unblock signals */
	if (daemon_unblock_sigs(-1) < 0) {
		daemon_log(LOG_ERR, "Failed to unblock all signals: %s", strerror(errno));
	return 1;
	}

	/* Set the daemon's syslog identification string */
	daemon_log_ident = daemon_ident_from_argv0(argv[0]);

	/* Set the pid file to /var/run/<daemon>/<pid> */
	daemon_pid_file_proc = daemon_pid_file_proc_override;

	/* Check if we are called with -k parameter */
	if (argc >= 2 && !strcmp(argv[1], "-k")) {
		int ret;

		/* Kill daemon with SIGTERM */

		/* Check if the new function daemon_pid_file_kill_wait() is available, if it is, use it. */
		if ((ret = daemon_pid_file_kill_wait(SIGTERM, 5)) < 0) {
			daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));
		}

		return ret < 0 ? 1 : 0;
	}


	if (getuid() != 0) {
		daemon_log(LOG_ERR, "This daemon should be run as root.");
		return 1;
	}

	/* Check that the daemon is not run twice a the same time */
	if ((pid = daemon_pid_file_is_running()) >= 0) {
		daemon_log(LOG_ERR, "Daemon already running on PID file %u", pid);
		return 1;
	}

	/* Prepare for return value passing from the initialization procedure of the daemon process */
	if (daemon_retval_init() < 0) {
		daemon_log(LOG_ERR, "Failed to create pipe.");
		return 1;
	}

	/* Do the fork */
	if ((pid = daemon_fork()) < 0) {

		/* Exit on error */
		daemon_retval_done();
		return 1;
	}
	else if (pid) {			/* The parent */
		int ret;

		/* Wait for 20 seconds for the return value passed from the daemon process */
		if ((ret = daemon_retval_wait(20)) < 0) {
			daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s", strerror(errno));
			return 255;
		}

		if (ret != 0)
			daemon_log(LOG_ERR, "Daemon returned %i as return value.", ret);
		return ret;
	}
	else {				/* The daemon */
		/* Close FDs */
		if (daemon_close_all(-1) < 0) {
			daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));

			/* Send the error condition to the parent process */
			daemon_retval_send(1);
			goto finish;
		}

		/* Create the daemon runtime dir */
		if (make_runtime_dir(&runtime_path) < 0) {
			goto finish;
		}

		/* Drop root priviledges */
		if (drop_root() < 0) {
			daemon_log(LOG_ERR, "Could not drop root privileges for %s/%s", DAEMON_USER, DAEMON_GROUP);
			goto finish;
		}

		/* Create the PID file */
		if (daemon_pid_file_create() < 0) {
			daemon_log(LOG_ERR, "Could not create PID file (%s)", strerror(errno));
			daemon_retval_send(2);
			goto finish;
		}

	        /* Initialize signal handling */
		sa.sa_handler = sigterm;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGTERM);
		sa.sa_flags = 0;

		if(sigaction(SIGTERM, &sa, NULL) < 0) {
			daemon_log(LOG_ERR, "Cannot catch SIGTERM: %s", strerror(errno));
			daemon_retval_send(3);
			goto finish;
		}

		/* Set environment vars for the Lua daemon */
		setenv("DAEMON", daemon_log_ident, 1);
		setenv("DAEMON_PATH", runtime_path, 1);

		/* Derive the Lua daemon path from the C daemon one */
		asprintf(&luad_path, "%s%s", (const char *)argv[0], ".lua");
       		
		/* Send OK to parent process */
        	daemon_retval_send(0);
        	daemon_log(LOG_INFO, "Sucessfully started %s SRC=%s", daemon_log_ident, luad_path);

		/* Erlang-style supervisor */
		struct restart_s restart;
		restart_init(&restart);

		do {
			/* Create a new Lua environment */
			L = luaL_newstate();
			/* And load the standard libraries into the Lua environment */
			luaL_openlibs(L);
			/* Tunnel through the wormhole into Lua neverland. This call should never return. */
			if (luaL_dofile(L, (const char *)luad_path)) {
				daemon_log(LOG_ERR, "%s", lua_tostring(L,-1));
			}
			/* Clean up the Lua state */
			lua_close(L);

			/* Wait for one second before restarting the Lua daemon */
			restart_add(&restart, time(NULL));
			sleep(1);

		} while (!restart_max(&restart));

		/* Stop when allowed number of restarts have occurred in specified time window */
		daemon_log(LOG_ERR, "%d restarts within a %d sec window", SUP_ALLOWED_RESTARTS, SUP_MAX_SECONDS);

	
		/* Do a cleanup */
finish:
		daemon_log(LOG_INFO, "Exiting...");
		daemon_retval_send(255);
		daemon_pid_file_remove();
	
		return 0;
	}
}
