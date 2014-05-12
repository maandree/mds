/**
 * mds — A micro-display server
 * Copyright © 2014  Mattias Andrée (maandree@member.fsf.org)
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
  unsigned int display;
  FILE *f;
  int rc;
  int j;
  
  
  argc = argc_;
  argv = argv_;
  
  
  /* Sanity check the number of command line arguments. */
  if (argc > ARGC_LIMIT)
    {
      eprint("that number of arguments is ridiculous, I will not allow it.");
      return 1;
    }
  
  /* Parse command line arguments. */
  for (j = 1; j < argc; j++)
    {
      char* arg = argv[j];
      if (startswith(arg, "--master-server=")) /* Master server. */
	{
	  if (got_master_server)
	    {
	      eprintf("duplicate declaration of %s.", "--master-server");
	      return 1;
	    }
	  got_master_server = 1;
	  master_server = arg + strlen("--master-server=");
	}
    }
  
  /* Stymied if the effective user is not root. */
  if (geteuid() != ROOT_USER_UID)
    {
      eprint("the effective user is not root, cannot continue.");
      return 1;
    }
  
  /* Set up to ignore SIGUSR1, used in mds for re-exec, but we cannot re-exec. */
  if (xsigaction(SIGUSR1, SIG_IGN) < 0)
    {
      perror(*argv);
      eprint("while ignoring the SIGUSR1 signal.");
    }
  
  /* Create directory for socket files, PID files and such. */
  if (create_directory_root(MDS_RUNTIME_ROOT_DIRECTORY))
    return 1;
  
  /* Determine display index. */
  for (display = 0; display < DISPLAY_MAX; display++)
    {
      xsnprintf(pathname, "%s/%u.pid", MDS_RUNTIME_ROOT_DIRECTORY, display);
      
      fd = open(pathname, O_CREAT | O_EXCL);
      if (fd == -1)
	{
	  /* Reuse display index not no longer used. */
	  size_t read_len;
	  f = fopen(pathname, "r");
	  if (f == NULL) /* Race, or error? */
	    {
	      perror(*argv);
	      eprintf("while opening the file: %s", pathname);
	      continue;
	    }
	  read_len = fread(piddata, 1, sizeof(piddata) / sizeof(char), f);
	  if (ferror(f)) /* Failed to read. */
	    {
	      perror(*argv);
	      eprintf("while reading the file: %s", pathname);
	    }
	  else if (feof(f) == 0) /* Did not read everything. */
	    eprint("the content of a PID file is longer than expected.");
	  else
	    {
	      pid_t pid = 0;
	      size_t i, n = read_len - 1;
	      for (i = 0; i < n; i++)
		{
		  char c = piddata[i];
		  if (('0' <= c) && (c <= '9'))
		    pid = pid * 10 + (c & 15);
		  else
		    {
		      eprint("the content of a PID file is invalid.");
		      goto bad;
		    }
		}
	      if (piddata[n] != '\n')
		{
		  eprint("the content of a PID file is invalid.");
		  goto bad;
		}
	      if (kill(pid, 0) < 0) /* Check if the PID is still allocated to any process. */
		if (errno == ESRCH) /* PID is not used. */
		  {
		    fclose(f);
		    close(fd);
		    break;
		  }
	    }
	bad:
	  fclose(f);
	  continue;
	}
      close(fd);
      break;
    }
  if (display == DISPLAY_MAX)
    {
      eprint("sorry, too many displays on the system.");
      return 1;
      /* Yes, the directory could have been removed, but it probably was not. */
    }
  
  /* Create PID file. */
  f = fopen(pathname, "w");
  if (f == NULL)
    {
      perror(*argv);
      eprintf("while opening the file: %s", pathname);
      return 1;
    }
  xsnprintf(piddata, "%u\n", getpid());
  if (fwrite(piddata, 1, strlen(piddata), f) < strlen(piddata))
    {
      fclose(f);
      if (unlink(pathname) < 0)
	perror(*argv);
      return -1;
    }
  fflush(f);
  fclose(f);
  
  /* Create data storage directory. */
  if (create_directory_root(MDS_STORAGE_ROOT_DIRECTORY))
    goto fail;
  xsnprintf(pathname,  "%s/%u.data", MDS_STORAGE_ROOT_DIRECTORY, display);
  if (unlink_recursive(pathname) || create_directory_user(pathname))
    goto fail;
  
  /* Save MDS_DISPLAY environment variable. */
  xsnprintf(pathname, /* Excuse the reuse without renaming. */
	   "%s=:%u", DISPLAY_ENV, display);
  putenv(pathname);
  
  /* Create display socket. */
  xsnprintf(pathname, "%s/%u.socket", MDS_RUNTIME_ROOT_DIRECTORY, display);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, pathname);
  unlink(pathname);
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if ((fchmod(fd, S_IRWXU) < 0) ||
      (fchown(fd, getuid(), NOBODY_GROUP_GID) < 0))
    {
      perror(*argv);
      eprint("while making anonymous socket private to the real user.");
      goto fail;
    }
  if (bind(fd, (struct sockaddr*)(&address), sizeof(address)) < 0)
    {
      perror(*argv);
      eprintf("while binding socket to file: %s", pathname);
      goto fail;
    }
  
  /* Start listening on socket. */
  if (listen(fd, SOMAXCONN) < 0)
    {
      perror(*argv);
      eprintf("while setting up listening on socket: %s", pathname);
      goto fail;
    }
  
  /* Start master server and respawn it if it crashes. */
  rc = spawn_and_respawn_server(fd);
  
 done:
  /* Shutdown, close and remove the socket. */
  if (fd != -1)
    {
      shutdown(fd, SHUT_RDWR);
      close(fd);
      unlink(pathname);
    }
  
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
  rc = 1;
  goto done;
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
  
  child_args[0] = strdup(master_server);
  for (i = 1; i < argc; i++)
    child_args[i] = argv[i];
  child_args[argc + 0] = strdup("--initial-spawn");
  xsnprintf(fdstr, "--socket-fd=%i", fd);
  child_args[argc + 1] = fdstr;
  child_args[argc + 2] = NULL;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 2)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 2.
#endif
  
  for (;;)
    {
      pid = fork();
      if (pid == (pid_t)-1)
	{
	  perror(*argv);
	  eprint("while forking.");
	  goto fail;
	}
      
      if (pid)
	{
	  /* Get the current time. (Start of child process.) */
	  time_error = (monotone(&time_start) < 0);
	  if (time_error)
	    {
	      perror(*argv);
	      eprint("while reading a monotonic clock.");
	    }
	  
	  /* Wait for master server to die. */
	  if (waitpid(pid, &status, 0) == (pid_t)-1)
	    {
	      perror(*argv);
	      eprint("while waiting for child process to exit.");
	      goto fail;
	    }
	  
	  /* If the server exited normally or SIGTERM, do not respawn. */
	  if (WIFEXITED(status) || (WEXITSTATUS(status) && WTERMSIG(status)))
	    break;
	  
	  /* Get the current time. (End of child process.) */
	  time_error |= (monotone(&time_end) < 0);
	  
	  /* Do not respawn if we could not read the time. */
	  if (time_error)
	    {
	      perror(*argv);
	      eprintf("%s died abnormally, not respawning because we could not read the time.", master_server);
	      goto fail;
	    }
	  
	  /* Respawn if the server did not die too fast. */
	  if (time_end.tv_sec - time_start.tv_sec < RESPAWN_TIME_LIMIT_SECONDS)
	    eprintf("%s died abnormally, respawning.", master_server);
	  else
	    {
	      eprintf("%s died abnormally, died too fast, not respawning.", master_server);
	      goto fail;
	    }
	  
	  if (first_spawn)
	    {
	      first_spawn = 0;
	      free(child_args[argc + 0]);
	      child_args[argc + 0] = strdup("--respawn");
	      if (child_args[argc + 0] == NULL)
		{
		  perror(*argv);
		  eprint("while duplicating string.");
		  goto fail;
		}
	    }
	}
      else
	{
	  rc++;
	  /* Drop privileges. They most not be propagated non-authorised components. */
	  /* setgid should not be set, but just to be safe we are restoring both user and group. */
	  if (drop_privileges())
	    {
	      perror(*argv);
	      eprint("while dropping privileges.");
	      goto fail;
	    }
	  
	  /* Start master server. */
	  execv(master_server, child_args);
	  perror(*argv);
	  eprint("while changing execution image.");
	  goto fail;
	}
    }
  
  rc--;
 fail:
  rc++;
  free(child_args[0]);
  free(child_args[argc + 0]);
  if (rc == 2)
    _exit(1);
  return rc;
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
    {
      /* Directory is missing, create it. */
      if (mkdir(pathname, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
	{
	  if (errno != EEXIST) /* Unlikely race condition. */
	    {
	      perror(*argv);
	      eprintf("while creating directory: %s", pathname);
	      return 1;
	    }
	}
      else
	/* Set ownership. */
	if (chown(pathname, ROOT_USER_UID, ROOT_GROUP_GID) < 0)
	  {
	    perror(*argv);
	    eprintf("while changing owner of directory: %s", pathname);
	    return 1;
	  }
    }
  
  return 0;
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
    {
      /* Directory is missing, create it. */
      if (mkdir(pathname, S_IRWXU) < 0)
	{
	  if (errno != EEXIST) /* Unlikely race condition. */
	    {
	      perror(*argv);
	      eprintf("while creating directory: %s", pathname);
	      return 1;
	    }
	}
      else
	/* Set ownership. */
	if (chown(pathname, getuid(), NOBODY_GROUP_GID) < 0)
	  {
	    perror(*argv);
	    eprintf("while changing owner of directory: %s", pathname);
	    return 1;
	  }
    }
  
  return 0;
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
  
  if (dir == NULL)
    {
      int errno_ = errno;
      struct stat _attr;
      if (stat(pathname, &_attr) < 0)
	return 0;
      errno = errno_;
      perror(*argv);
      eprintf("while examining the content of directory: %s", pathname);
      return 1;
    }
  
  while ((file = readdir(dir)) != NULL)
    if (strcmp(file->d_name, ".") && strcmp(file->d_name, ".."))
      if (unlink(file->d_name) < 0)
	{
	  if (errno == EISDIR)
	    unlink_recursive(file->d_name);
	  else
	    {
	      perror(*argv);
	      eprintf("while unlinking file: %s", file->d_name);
	      rc = 1;
	      goto done;
	    }
	  eprint("pop");
	}
  
  if (rmdir(pathname) < 0)
    {
      perror(*argv);
      eprintf("while removing directory: %s", pathname);
      rc = 1;
    }
  
 done:  
  closedir(dir);
  return rc;
}

