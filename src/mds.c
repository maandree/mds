/**
 * mds — A micro-display server
 * Copyright © 2014, 2015  Mattias Andrée (maandree@member.fsf.org)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "mds.h"

#include <libmdsserver/config.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>


/**
 * Number of elements in `argv`
 */
static int argc;

/**
 * Command line arguments
 */
static char** argv;

/**
 * The master server
 */
static const char* master_server = LIBEXECDIR "/mds-server";

/**
 * The umask the server start with
 */
static mode_t saved_umask;


/**
 * Entry point of the program
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  int fd = -1;
  int got_master_server = 0;
  struct sockaddr_un address;
  char pathname[PATH_MAX];
  char piddata[64];
  unsigned int display = DISPLAY_MAX;
  FILE* f;
  int rc;
  int j, r;
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Sanity check the number of command line arguments. */
  exit_if (argc > ARGC_LIMIT,
	   eprint("that number of arguments is ridiculous, I will not allow it."););
  
  /* Parse command line arguments. */
  for (j = 1; j < argc; j++)
    {
      char* arg = argv[j];
      if (startswith(arg, "--master-server=")) /* Master server. */
	{
	  exit_if (got_master_server,
		   eprintf("duplicate declaration of %s.", "--master-server"););
	  got_master_server = 1;
	  master_server = arg + strlen("--master-server=");
	}
    }
  
  /* Stymied if the effective user is not root. */
  exit_if (geteuid() != ROOT_USER_UID,
	   eprint("the effective user is not root, cannot continue."););
  
  /* Set up to ignore SIGUPDATE, used in mds for re-exec, but we cannot re-exec. */
  if (xsigaction(SIGUPDATE, SIG_IGN) < 0)
    xperror(*argv);
  
  /* Set up to ignore SIGDANGER. */
  if (xsigaction(SIGDANGER, SIG_IGN) < 0)
    xperror(*argv);
  
  /* Set up to ignore SIGINFO. */
  if (xsigaction(SIGINFO, SIG_IGN) < 0)
    xperror(*argv);
  
  /* Remove umask. */
  saved_umask = umask(0);
  
  /* Create directory for socket files, PID files and such. */
  fail_if (create_directory_root(MDS_RUNTIME_ROOT_DIRECTORY));
  
  
  /* Determine display index. */
  for (display = 0; display < DISPLAY_MAX; display++)
    {
      xsnprintf(pathname, "%s/%u.pid", MDS_RUNTIME_ROOT_DIRECTORY, display);
      
      fd = open(pathname, O_CREAT | O_EXCL, 0644);
      if (fd == -1)
	{
	  /* Reuse display index if no longer used. */
	  f = fopen(pathname, "r");
	  if (f == NULL) /* Race, or error? */
	    {
	      xperror(*argv);
	      continue;
	    }
	  r = is_pid_file_reusable(f);
	  xfclose(f);
	  if (r == 0)
	    continue;
	}
      xclose(fd);
      break;
    }
  exit_if (display == DISPLAY_MAX,
	   eprint("sorry, too many displays on the system."););
           /* Yes, the directory could have been removed, but it probably was not. */
  
  /* Create PID file. */
  fail_if (f = fopen(pathname, "w"), f == NULL);
  xsnprintf(piddata, "%u\n", getpid());
  if (fwrite(piddata, 1, strlen(piddata), f) < strlen(piddata))
    {
      xfclose(f);
      fail_if (1);
    }
  fflush(f);
  xfclose(f);
  if (chmod(pathname, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0)
    xperror(*argv);
  
  /* Create data storage directory. */
  xsnprintf(pathname,  "%s/%u.data", MDS_STORAGE_ROOT_DIRECTORY, display);
  fail_if (create_directory_root(MDS_STORAGE_ROOT_DIRECTORY));
  fail_if (unlink_recursive(pathname));
  fail_if (create_directory_user(pathname));
    
  
  /* Save MDS_DISPLAY environment variable. */
  xsnprintf(pathname, /* Excuse the reuse without renaming. */
	    ":%u", display);
  fail_if (setenv(DISPLAY_ENV, pathname, 1) < 0);
  
  /* Create a new process group and export MDS_PGROUP */
  fail_if (setpgid(0, 0) < 0);
  xsnprintf(pathname, /* Excuse the reuse without renaming. */
	    "%ji", (intmax_t)getpgrp());
  fail_if (setenv(PGROUP_ENV, pathname, 1) < 0);
  
  /* Create display socket. */
  xsnprintf(pathname, "%s/%u.socket", MDS_RUNTIME_ROOT_DIRECTORY, display);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, pathname);
  unlink(pathname);
  fail_if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0);
  fail_if (fchmod(fd, S_IRWXU) < 0);
  fail_if (bind(fd, (struct sockaddr*)(&address), sizeof(address)) < 0);
  fail_if (chown(pathname, getuid(), NOBODY_GROUP_GID) < 0);
  
  /* Start listening on socket. */
  fail_if (listen(fd, SOMAXCONN) < 0);
  
  /* Start master server and respawn it if it crashes. */
  rc = spawn_and_respawn_server(fd);
  
 done:
  /* Shutdown, close and remove the socket. */
  if (fd != -1)
    {
      shutdown(fd, SHUT_RDWR);
      xclose(fd);
      unlink(pathname);
    }
  
  if (display == DISPLAY_MAX)
    return rc;
  
  /* Remove PID file. */
  xsnprintf(pathname, "%s/%u.pid", MDS_RUNTIME_ROOT_DIRECTORY, display);
  unlink(pathname);
  
  /* Remove directories. */
  rmdir(MDS_RUNTIME_ROOT_DIRECTORY); /* Do not care if it fails, it is probably used by another display. */
  rmdir(MDS_STORAGE_ROOT_DIRECTORY); /* Do not care if it fails, it is probably used by another display. */
  xsnprintf(pathname, "%s/%u.data", MDS_STORAGE_ROOT_DIRECTORY, display);
  unlink_recursive(pathname); /* An error will be printed on error, do nothing more. */
  
  return rc;
  
 fail:
  xperror(*argv);
  rc = 1;
  goto done;
}


/**
 * Read a PID-file and determine whether it refers to an non-existing process
 * 
 * @param   f  The PID-file
 * @return     Whether the PID-file is not longer used
 */
int is_pid_file_reusable(FILE* f)
{
  char piddata[64];
  size_t read_len;
  
  read_len = fread(piddata, 1, sizeof(piddata) / sizeof(char), f);
  fail_if (ferror(f)); /* Failed to read. */
  if (feof(f) == 0) /* Did not read everything. */
    eprint("the content of a PID file is larger than expected.");
  else
    {
      pid_t pid = parse_pid_t(piddata, read_len - 1);
      if (pid == (pid_t)-1)
	eprint("the content of a PID file is invalid.");
      else
	if (kill(pid, 0) < 0) /* Check if the PID is still allocated to any process. */
	  return errno == ESRCH; /* PID is not used. */
    }
  
  return 0;
 fail:
  xperror(*argv);
  return 0;
}


/**
 * Parse an LF-terminated string as a non-negative `pid_t`
 * 
 * @param   str  The string
 * @param   n    The length of the string, excluding LF-termination
 * @return       The pid, `(pid_t)-1` if malformated
 */
pid_t parse_pid_t(const char* str, size_t n)
{
  pid_t pid = 0;
  size_t i;
  
  for (i = 0; i < n; i++)
    {
      char c = str[i];
      if (('0' <= c) && (c <= '9'))
	pid = pid * 10 + (c & 15);
      else
	return (pid_t)-1;
    }
  
  if (str[n] != '\n')
    return (pid_t)-1;
  
  return pid;
}



/**
 * Drop privileges and change execution image into the master server's image.
 * This function will only return on error.
 * 
 * @param  child_args  Command line arguments for the new image
 */
static void exec_master_server(char** child_args)
{
  /* Drop privileges. They most not be propagated non-authorised components. */
  /* setgid should not be set, but just to be safe we are restoring both user and group. */
  fail_if (drop_privileges());
  
  /* Start master server. */
  execv(master_server, child_args);
  fail_if (1);
 fail:
  return;
}


/**
 * Start master server and respawn it if it crashes
 * 
 * @param   fd  The file descriptor of the socket
 * @return      Non-zero on error
 */
int spawn_and_respawn_server(int fd)
{
  int time_error = 0;
  int first_spawn = 1;
  int rc = 0;
  struct timespec time_start;
  struct timespec time_end;
  char* child_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  char fdstr[12 /* strlen("--socket-fd=") */ + 64];
  int i;
  pid_t pid;
  int status;
  
  fail_if (xstrdup(child_args[0], master_server));
  for (i = 1; i < argc; i++)
    child_args[i] = argv[i];
  fail_if (xstrdup(child_args[argc + 0], "--initial-spawn"));
  xsnprintf(fdstr, "--socket-fd=%i", fd);
  child_args[argc + 1] = fdstr;
  child_args[argc + 2] = NULL;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 2)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 2.
#endif
  
  
 respawn:
  
  pid = fork();
  fail_if (pid == (pid_t)-1);
  
  if (pid == 0) /* Child. */
    {
      /* If this image exits, it should do so with failure status. */
      rc++;
      /* Reinstate original umask. */
      umask(saved_umask);
      /* Change image into the master server. */
      exec_master_server(child_args);
      fail_if (1);
    }
  
  
  /* Parent. */
  
  /* Get the current time. (Start of child process.) */
  if ((time_error = (monotone(&time_start) < 0)))
    xperror(*argv);
  
  /* Wait for master server to die. */
  fail_if (uninterruptable_waitpid(pid, &status, 0) == (pid_t)-1);
  
  /* If the server exited normally or SIGTERM, do not respawn. */
  if (WIFEXITED(status) ? (WEXITSTATUS(status) == 0) :
      ((WTERMSIG(status) == SIGTERM) || (WTERMSIG(status) == SIGINT)))
    {
      /* Child exited normally, stop. */
      rc--;
      goto done;
    }
  
  /* Get the current time. (End of child process.) */
  time_error |= (monotone(&time_end) < 0);
  
  if (WIFEXITED(status))
    eprintf("`%s' exited with code %i.", master_server, WEXITSTATUS(status));
  else
    eprintf("`%s' died by signal %i.", master_server, WTERMSIG(status));
  
  /* Do not respawn if we could not read the time. */
  if (time_error)
    {
      xperror(*argv);
      eprintf("`%s' died abnormally, not respawning because we could not read the time.", master_server);
      goto done;
    }
  
      /* Respawn if the server did not die too fast. */
  if (time_end.tv_sec - time_start.tv_sec >= RESPAWN_TIME_LIMIT_SECONDS)
    eprintf("`%s' died abnormally, respawning.", master_server);
  else
    {
      eprintf("`%s' died abnormally, died too fast, not respawning.", master_server);
      goto done;
    }
  
  if (first_spawn)
    {
      first_spawn = 0;
      free(child_args[argc + 0]);
      fail_if (xstrdup(child_args[argc + 0], "--respawn"));
    }
  
  goto respawn;
  
  
 done:
  rc++;
  free(child_args[0]);
  free(child_args[argc + 0]);
  if (rc == 2)
    _exit(1);
  return rc;
  
 fail:
  xperror(*argv);
  goto done;
}


/**
 * Create a directory owned by the root user and root group
 * 
 * @param   pathname  The pathname of the directory to create
 * @return            Non-zero on error
 */
int create_directory_root(const char* pathname)
{
  struct stat attr;
  
  if (stat(pathname, &attr) == 0)
    {
      /* Cannot create the directory, its pathname refers to an existing. */
      if (S_ISDIR(attr.st_mode) == 0)
	{
	  /* But it is not a directory so we cannot continue. */
	  eprintf("%s already exists but is not a directory.", pathname);
	  return 1;
	}
    }
  else
    /* Directory is missing, create it. */
    if (mkdir(pathname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
      /* Unlikely race condition. */
      fail_if (errno != EEXIST);
    else
      /* Set ownership. */
      fail_if (chown(pathname, ROOT_USER_UID, ROOT_GROUP_GID) < 0);
  
  return 0;
 fail:
  xperror(*argv);
  return 1;
}


/**
 * Create a directory owned by the real user and nobody group
 * 
 * @param   pathname  The pathname of the directory to create
 * @return            Non-zero on error
 */
int create_directory_user(const char* pathname)
{
  struct stat attr;
  
  if (stat(pathname, &attr) == 0)
    {
      /* Cannot create the directory, its pathname refers to an existing. */
      if (S_ISDIR(attr.st_mode) == 0)
	{
	  /* But it is not a directory so we cannot continue. */
	  eprintf("%s already exists but is not a directory.", pathname);
	  return 1;
	}
    }
  else
    /* Directory is missing, create it. */
    if (mkdir(pathname, S_IRWXU) < 0)
      /* Unlikely race condition. */
      fail_if (errno != EEXIST);
    else
      /* Set ownership. */
      fail_if (chown(pathname, getuid(), NOBODY_GROUP_GID) < 0);
  
  return 0;
 fail:
  xperror(*argv);
  return 1;
}


/**
 * Recursively remove a directory
 * 
 * @param   pathname  The pathname of the directory to remove
 * @return            Non-zero on error, but zero if the directory does not exist
 */
int unlink_recursive(const char* pathname)
{
  DIR* dir = opendir(pathname);
  int rc = 0;
  struct dirent* file;
  
  /* Check that we could examine the directory. */
  if (dir == NULL)
    {
      int saved_errno = errno;
      struct stat _attr;
      if (stat(pathname, &_attr) < 0)
	return 0; /* Directory does not exist. */
      errno = saved_errno;
      fail_if (1);
    }
  
  /* Remove the content of the directory. */
  while ((file = readdir(dir)) != NULL)
    if (strcmp(file->d_name, ".") &&
	strcmp(file->d_name, "..") &&
	(unlink(file->d_name) < 0))
      {
	fail_if (errno != EISDIR);
	unlink_recursive(file->d_name);
      }
  
  /* Remove the drectory. */
  fail_if (rmdir(pathname) < 0);
  
 done:
  if (dir != NULL)
    closedir(dir);
  return rc;
  
  
 fail:
  xperror(*argv);
  rc = -1;
  goto done;
}

