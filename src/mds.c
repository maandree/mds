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
#include "config.h"

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
static const char* master_server = LIBEXECDIR "/mds-master";


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
      fprintf(stderr,
	      "%s: that number of arguments is ridiculous, I will not allow it.\n",
	      *argv);
      return 1;
    }
  
  /* Parse command line arguments. */
  for (j = 1; j < argc; j++)
    {
      char* arg = argv[j];
      if (strstr(arg, "--master-server=") == arg) /* Master server. */
	{
	  if (got_master_server)
	    {
	      fprintf(stderr, "%s: duplicate declaration of %s.\n", *argv, "--master-server");
	      return 1;
	    }
	  got_master_server = 1;
	  master_server = arg + strlen("--master-server=");
	}
    }
  
  /* Stymied if the effective user is not root. */
  if (geteuid() != ROOT_USER_UID)
    {
      fprintf(stderr,
	      "%s: the effective user is not root, cannot continue.\n",
	      *argv);
      return 1;
    }
  
  /* Create directory for socket files, PID files and such. */
  if (create_directory_root(MDS_RUNTIME_ROOT_DIRECTORY))
    return 1;
  
  /* Determine display index. */
  for (display = 0; display < DISPLAY_MAX; display++)
    {
      snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/%u.pid",
	       MDS_RUNTIME_ROOT_DIRECTORY, display);
      
      fd = open(pathname, O_CREAT | O_EXCL);
      if (fd == -1)
	{
	  /* Reuse display index not no longer used. */
	  size_t read_len;
	  f = fopen(pathname, "r");
	  if (f == NULL) /* Race, or error? */
	    {
	      perror(*argv);
	      continue;
	    }
	  read_len = fread(piddata, 1, sizeof(piddata) / sizeof(char), f);
	  if (ferror(f)) /* Failed to read. */
	    perror(*argv);
	  else if (feof(f) == 0) /* Did not read everything. */
	    fprintf(stderr,
		    "%s: the content of a PID file is longer than expected.\n",
		    *argv);
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
		      fprintf(stderr,
			      "%s: the content of a PID file is invalid.\n",
			      *argv);
		      goto bad;
		    }
		}
	      if (piddata[n] != '\n')
		{
		  fprintf(stderr,
			  "%s: the content of a PID file is invalid.\n",
			  *argv);
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
      fprintf(stderr,
	      "%s: Sorry, too many displays on the system.\n",
	      *argv);
      return 1;
      /* Yes, the directory could have been removed, but it probably was not. */
    }
  
  /* Create PID file. */
  f = fopen(pathname, "w");
  if (f == NULL)
    {
      perror(*argv);
      return 1;
    }
  snprintf(piddata, sizeof(piddata) / sizeof(char), "%u\n", getpid());
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
  snprintf(pathname, sizeof(pathname) / sizeof(char),
	   "%s/%u.data", MDS_STORAGE_ROOT_DIRECTORY, display);
  if (unlink_recursive(pathname) || create_directory_user(pathname))
    goto fail;
  
  /* Save MDS_DISPLAY environment variable. */
  snprintf(pathname, sizeof(pathname) / sizeof(char), /* Excuse the reuse without renaming. */
	   "%s=:%u", DISPLAY_ENV, display);
  putenv(pathname);
  
  /* Create display socket. */
  snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/%u.socket",
	   MDS_RUNTIME_ROOT_DIRECTORY, display);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, pathname);
  unlink(pathname);
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if ((fchmod(fd, S_IRWXU) < 0) ||
      (fchown(fd, getuid(), NOBODY_GROUP_GID) < 0))
    goto fail;
  if (bind(fd, (struct sockaddr*)(&address), sizeof(address)) < 0)
    goto fail;
  
  /* Start listening on socket. */
  if (listen(fd, SOMAXCONN) < 0)
    goto fail;
  
  /* Drop privileges. They most not be propagated non-authorised components. */
  /* setgid should not be set, but just to be safe we are restoring both user and group. */
  if ((seteuid(getuid()) < 0) || (setegid(getgid()) < 0))
    goto fail;
  
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
  snprintf(pathname, sizeof(pathname) / sizeof(char), "%s/%u.pid",
	   MDS_RUNTIME_ROOT_DIRECTORY, display);
  unlink(pathname);
  
  /* Remove directories */
  rmdir(MDS_RUNTIME_ROOT_DIRECTORY); /* Do not care if it fails, it is probably used by another display. */
  rmdir(MDS_STORAGE_ROOT_DIRECTORY); /* Do not care if it fails, it is probably used by another display. */
  snprintf(pathname, sizeof(pathname) / sizeof(char),
	   "%s/%u.data", MDS_STORAGE_ROOT_DIRECTORY, display);
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
  struct timespec time_start;
  struct timespec time_end;
  char* child_args[ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT + 1];
  char fdstr[12 /* strlen("--socket-fd=") */ + 64];
  int i;
  pid_t pid;
  int status;
  int time_error = 0;
  int first_spawn = 1;
  
  child_args[0] = strdup(master_server);
  for (i = 1; i < argc; i++)
    child_args[i] = argv[i];
  child_args[argc + 0] = strdup("--initial-spawn");
  snprintf(fdstr, sizeof(fdstr) / sizeof(char), "--socket-fd=%i", fd);
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
	  return 1;
	}
      
      if (pid)
	{
	  /* Get the current time. (Start of child process.) */
	  time_error = (clock_gettime(CLOCK_MONOTONIC, &time_start) < 0);
	  if (time_error)
	    perror(*argv);
	  
	  /* Wait for master server to die. */
	  if (waitpid(pid, &status, 0) == (pid_t)-1)
	    {
	      perror(*argv);
	      return 1;
	    }
	  
	  /* If the server exited normally or SIGTERM, do not respawn. */
	  if (WIFEXITED(status) || (WEXITSTATUS(status) && WTERMSIG(status)))
	    break;
	  
	  /* Get the current time. (End of child process.) */
	  time_error |= (clock_gettime(CLOCK_MONOTONIC, &time_end) < 0);
	  
	  /* Do not respawn if we could not read the time. */
	  if (time_error)
	    {
	      perror(*argv);
	      fprintf(stderr,
		      "%s: %s died abnormally, not respoawning because we could not read the time.\n",
		      *argv, master_server);
	      return 1;
	    }
	  
	  /* Respawn if the server did not die too fast. */
	  if (time_end.tv_sec - time_start.tv_sec < RESPAWN_TIME_LIMIT_SECONDS)
	    fprintf(stderr, "%s: %s died abnormally, respawning.\n", *argv, master_server);
	  else
	    {
	      fprintf(stderr,
		      "%s: %s died abnormally, died too fast, not respawning.\n",
		      *argv, master_server);
	      return 1;
	    }
	  
	  if (first_spawn)
	    {
	      first_spawn = 0;
	      free(child_args[argc + 0]);
	      child_args[argc + 0] = strdup("--respawn");
	    }
	}
      else
	{
	  /* Start master server. */
	  execv(master_server, child_args);
	  perror(*argv);
	  return 1;
	}
    }
  
  free(child_args[0]);
  free(child_args[argc + 0]);
  free(child_args[argc + 1]);
  return 0;
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
	  fprintf(stderr,
		  "%s: %s already exists but is not a directory.\n",
		  pathname, *argv);
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
	      return 1;
	    }
	}
      else
	/* Set ownership. */
	if (chown(pathname, ROOT_USER_UID, ROOT_GROUP_GID) < 0)
	  {
	    perror(*argv);
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
	  fprintf(stderr,
		  "%s: %s already exists but is not a directory.\n",
		  pathname, *argv);
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
	      return 1;
	    }
	}
      else
	/* Set ownership. */
	if (chown(pathname, getuid(), NOBODY_GROUP_GID) < 0)
	  {
	    perror(*argv);
	    return 1;
	  }
    }
  
  return 0;
}


/**
 * Recursively remove a directory
 * 
 * @param   pathname  The pathname of the directory to remove
 * @return            Non-zero on error
 */
int unlink_recursive(const char* pathname)
{
  DIR* dir = opendir(pathname);
  int rc = 0;
  struct dirent* file;
  
  if (dir == NULL)
    {
      perror(*argv);
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
	      rc = 1;
	      goto done;
	    }
	}
  
  if (rmdir(pathname) < 0)
    {
      perror(*argv);
      rc = 1;
    }

 done:  
  closedir(dir);
  return rc;
}

