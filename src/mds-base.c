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
#include "mds-base.h"

#include <libmdsserver/config.h>
#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>


#define  try(INSTRUCTION)  if ((r = INSTRUCTION))  goto fail


/**
 * Number of elements in `argv`
 */
int argc = 0;

/**
 * Command line arguments
 */
char** argv = NULL;

/**
 * Whether the server has been respawn
 * rather than this being the initial spawn.
 * This will be at least as true as `is_reexec`.
 */
int is_respawn = -1;

/**
 * Whether the server is continuing
 * from a self-reexecution
 */
int is_reexec = 0;

/**
 * Whether the server should do its
 * best to resist event triggered death
 */
int is_immortal = 0;

/**
 * Whether to fork the process when the
 * server has been properly initialised
 */
int on_init_fork = 0;

/**
 * Command the run (`NULL` for none) when
 * the server has been properly initialised
 */
char* on_init_sh = NULL;

/**
 * The thread that runs the master loop
 */
pthread_t master_thread;


/**
 * Whether the server has been signaled to terminate
 */
volatile sig_atomic_t terminating = 0;

/**
 * Whether the server has been signaled to re-exec
 */
volatile sig_atomic_t reexecing = 0;


/**
 * The file descriptor of the socket
 * that is connected to the server
 */
int socket_fd = -1;



/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
int __attribute__((weak)) parse_cmdline(void)
{
  int i;
  
#if (LIBEXEC_ARGC_EXTRA_LIMIT < 2)
# error LIBEXEC_ARGC_EXTRA_LIMIT is too small, need at least 2.
#endif
  
  
  /* Parse command line arguments. */
  for (i = 1; i < argc; i++)
    {
      char* arg = argv[i];
      int v;
      if ((v = strequals(arg, "--initial-spawn")) || /* Initial spawn? */
	  strequals(arg, "--respawn"))               /* Respawning after crash? */
	{
	  exit_if (is_respawn == v,
		   eprintf("conflicting arguments %s and %s cannot be combined.",
			   "--initial-spawn", "--respawn"););
	  is_respawn = !v;
	}
      else if (strequals(arg, "--re-exec")) /* Re-exec state-marshal. */
	is_reexec = 1;
      else if (startswith(arg, "--alarm=")) /* Schedule an alarm signal for forced abort. */
	alarm(min(atou(arg + strlen("--alarm=")), 60)); /* At most 1 minute. */
      else if (strequals(arg, "--on-init-fork")) /* Fork process when initialised. */
	on_init_fork = 1;
      else if (startswith(arg, "--on-init-sh=")) /* Run a command when initialised. */
	on_init_sh = arg + strlen("--on-init-sh=");
      else if (strequals(arg, "--immortal")) /* I return to serve. */
	is_immortal = 1;
    }
  if (is_reexec)
    {
      is_respawn = 1;
      eprint("re-exec performed.");
    }
  
  /* Check that manditory arguments have been specified. */
  if (server_characteristics.require_respawn_info)
    {
      exit_if (is_respawn < 0,
	       eprintf("missing state argument, require either %s or %s.",
		       "--initial-spawn", "--respawn"););
    }
  return 0;
}


/**
 * Connect to the display
 * 
 * @return  Non-zero on error
 */
int __attribute__((weak)) connect_to_display(void)
{
  char* display;
  char pathname[PATH_MAX];
  struct sockaddr_un address;
  
  display = getenv("MDS_DISPLAY");
  exit_if ((display == NULL) || (strchr(display, ':') == NULL),
	   eprint("MDS_DISPLAY has not set."););
  exit_if (display[0] != ':',
	   eprint("Remote mds sessions are not supported."););
  xsnprintf(pathname, "%s/%s.socket", MDS_RUNTIME_ROOT_DIRECTORY, display + 1);
  address.sun_family = AF_UNIX;
  strcpy(address.sun_path, pathname);
  fail_if ((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0);
  fail_if (connect(socket_fd, (struct sockaddr*)(&address), sizeof(address)) < 0);
  
  return 0;
  
 pfail:
  xperror(*argv);
  if (socket_fd >= 0)
    close(socket_fd);
  return 1;
}


/**
 * Put the server into a fork of itself as
 * described by the `fork_for_safety`
 * server characteristics
 * 
 * @return  Zero on success, -1 on error
 */
static int server_initialised_fork_for_safety(void)
{
  unsigned pending_alarm = alarm(0); /* Disable the alarm. */
  pid_t pid = fork();
  int status;
  
  if (pid == (pid_t)-1)
    {
      xperror(*argv);
      eprint("while forking for safety.");
      return -1;
    }
  else if (pid == 0)
    /* Reinstate the alarm for the child. */
    alarm(pending_alarm);
  else
    {
      /* SIGDANGER cannot hurt the parent process. */
      if (xsigaction(SIGDANGER, SIG_IGN) < 0)
	{
	  xperror(*argv);
	  eprint("WARNING! parent process failed to sig up ignoring of SIGDANGER.");
	}
      
      /* Wait for the child process to die. */
      if (uninterruptable_waitpid(pid, &status, 0) == (pid_t)-1)
	{
	  xperror(*argv);
	  kill(pid, SIGABRT);
	  sleep(5);
	}
      
      /* Clean up after us. */
      fork_cleanup(status);
      
      /* Die like the child. */
      if      (WIFEXITED(status))    exit(WEXITSTATUS(status));
      else if (WIFSIGNALED(status))  raise(WTERMSIG(status));
      exit(1);
    }
  
  return 0;
}


/**
 * This function should be called when the server has
 * been properly initialised but before initialisation
 * of anything that is removed at forking is initialised
 * 
 * @return  Zero on success, -1 on error
 */
int __attribute__((weak)) server_initialised(void)
{
  pid_t r;
  if (on_init_fork && (r = fork()))
    {
      if (r == (pid_t)-1)
	{
	  xperror(*argv);
	  eprint("while forking at completed initialisation.");
	  return -1;
	}
      else
	exit(0);
    }
  
  if (on_init_sh != NULL)
    system(on_init_sh);
  
  if (server_characteristics.fork_for_safety)
    return server_initialised_fork_for_safety();
  return 0;
}


/**
 * This function is called when an intraprocess signal
 * that used to send a notification to a thread
 * 
 * @param  signo  The signal that has been received
 */
static void __attribute__((const)) received_noop(int signo)
{
  (void) signo;
}


# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
/**
 * This function should be implemented by the actual server implementation
 * if the server is multi-threaded
 * 
 * Send a singal to all threads except the current thread
 * 
 * @param  signo  The signal
 */
void __attribute__((weak)) signal_all(int signo)
{
  (void) signo;
}
# pragma GCC diagnostic pop


/**
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `reexecing` and
 * `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void __attribute__((weak)) received_reexec(int signo)
{
  (void) signo;
  if (reexecing == 0)
    {
      reexecing = terminating = 1;
      eprint("re-exec signal received.");
      signal_all(signo);
    }
}


/**
 * This function is called when a signal that
 * signals the server to terminate has been received
 * 
 * When this function is invoked, it should set `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void __attribute__((weak)) received_terminate(int signo)
{
  (void) signo;
  if (terminating == 0)
    {
      terminating = 1;
      eprint("terminate signal received.");
      signal_all(signo);
    }
}


/**
 * Unmarshal the server's saved state
 * 
 * @return  Non-zero on error
 */
static int base_unmarshal(void)
{
  pid_t pid = getpid();
  int reexec_fd, r;
  char shm_path[NAME_MAX + 1];
  char* state_buf;
  char* state_buf_;
  
  /* Acquire access to marshalled data. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (intmax_t)pid);
  reexec_fd = shm_open(shm_path, O_RDONLY, S_IRWXU);
  fail_if (reexec_fd < 0); /* Critical. */
  
  /* Read the state file. */
  fail_if ((state_buf = state_buf_ = full_read(reexec_fd, NULL)) == NULL);
  
  /* Release resources. */
  close(reexec_fd);
  shm_unlink(shm_path);
  
  
  /* Unmarshal state. */
  
  /* Get the marshal protocal version. Not needed, there is only the one version right now. */
  /* buf_get(state_buf_, int, 0, MDS_BASE_VARS_VERSION); */
  buf_next(state_buf_, int, 1);
  
  buf_get_next(state_buf_, int, socket_fd);
  r = unmarshal_server(state_buf_);
  
  
  /* Release resources. */
  free(state_buf);
  
  /* Recover after failure. */
  if (r)
    fail_if (reexec_failure_recover());
  
  return 0;
 pfail:
  xperror(*argv);
  return 1;
}


/**
 * Marshal the server's state
 * 
 * @param   reexec_fd  The file descriptor of the file into which the state shall be saved
 * @return             Non-zero on error
 */
static int base_marshal(int reexec_fd)
{
  size_t state_n;
  char* state_buf;
  char* state_buf_;
  
  /* Calculate the size of the state data when it is marshalled. */
  state_n = 2 * sizeof(int);
  state_n += marshal_server_size();
  
  /* Allocate a buffer for all data. */
  state_buf = state_buf_ = malloc(state_n);
  fail_if (state_buf == NULL);
  
  
  /* Marshal the state of the server. */
  
  /* Tell the new version of the program what version of the program it is marshalling. */
  buf_set_next(state_buf_, int, MDS_BASE_VARS_VERSION);
  
  /* Store the state. */
  buf_set_next(state_buf_, int, socket_fd);
  fail_if (marshal_server(state_buf_));
  
  
  /* Send the marshalled data into the file. */
  fail_if (full_write(reexec_fd, state_buf, state_n) < 0);
  
  free(state_buf);
  return 0;
  
 pfail:
  xperror(*argv);
  free(state_buf);
  return 1;
}


/**
 * Marshal and re-execute the server
 * 
 * This function only returns on error,
 * in which case the error will have been printed.
 */
static void perform_reexec(void)
{
  pid_t pid = getpid();
  int reexec_fd;
  char shm_path[NAME_MAX + 1];
  
  /* Marshal the state of the server. */
  xsnprintf(shm_path, SHM_PATH_PATTERN, (unsigned long int)pid);
  reexec_fd = shm_open(shm_path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
  if (reexec_fd < 0)
    {
      xperror(*argv);
      return;
    }
  if (base_marshal(reexec_fd) < 0)
    goto fail;
  close(reexec_fd);
  reexec_fd = -1;
  
  /* Re-exec the server. */
  reexec_server(argc, argv, is_reexec);
  xperror(*argv);
  
 fail:
  if (reexec_fd >= 0)
    close(reexec_fd);
  shm_unlink(shm_path);
}


/**
 * Entry point of the server
 * 
 * @param   argc_  Number of elements in `argv_`
 * @param   argv_  Command line arguments
 * @return         Non-zero on error
 */
int main(int argc_, char** argv_)
{
  int r = 1;
  
  argc = argc_;
  argv = argv_;
  
  
  if (server_characteristics.require_privileges == 0)
    /* Drop privileges like it's hot. */
    fail_if (drop_privileges());
  
  
  /* Use /proc/self/exe when re:exec-ing */
  if (prepare_reexec())
    xperror(*argv);
  
  
  /* Sanity check the number of command line arguments. */
  exit_if (argc > ARGC_LIMIT + LIBEXEC_ARGC_EXTRA_LIMIT,
	   eprint("that number of arguments is ridiculous, I will not allow it."););
  
  /* Parse command line arguments. */
  try (parse_cmdline());
  
  
  /* Store the current thread so it can be killed from elsewhere. */
  master_thread = pthread_self();
  
  /* Set up signal traps for all especially handled signals. */
  trap_signals();
  
  
  /* Initialise the server. */
  try (preinitialise_server());
  
  if (is_reexec == 0)
    {
      if (server_characteristics.require_display)
	/* Connect to the display. */
	try (connect_to_display());
      
      /* Initialise the server. */
      try (initialise_server());
    }
  else
    {
      /* Unmarshal the server's saved state. */
      try (base_unmarshal());
    }
  
  /* Initialise the server. */
  try (postinitialise_server());
  
  
  /* Run the server. */
  try (master_loop());
  
  
  /* Re-exec server if signal to re-exec. */
  if (reexecing)
    {
      perform_reexec();
      goto fail;
    }
  
  close(socket_fd);
  return 0;
  
  
 pfail:
  xperror(*argv);
  r = 1;
 fail:
  if (socket_fd >= 0)
    close(socket_fd);
  return r;
}


/**
 * This function is called when `SIGDANGER` is received
 * of `server_characteristics.danger_is_deadly` is non-zero
 * unless the signal handler for `SIGDANGER` has been
 * modified by the server implementation.
 * 
 * This function will abruptly kill the process
 * 
 * @param  signo  The signal that has been received
 */
static void commit_suicide(int signo)
{
  (void) signo;
  
  eprint("SIGDANGER received, process is killing itself to free memory.");
  
  /* abort(), but on the process rather than the thread. */
  xsigaction(SIGABRT, SIG_DFL);
  kill(getpid(), SIGABRT);
  
  /* Just in case. */
  xperror(*argv);
  _exit(1);
}


/**
 * Set up signal traps for all especially handled signals
 * 
 * @return  Non-zero on error
 */
int trap_signals(void)
{
  /* Make the server update without all slaves dying on SIGUPDATE. */
  fail_if (xsigaction(SIGUPDATE, received_reexec) < 0);
  
  /* Implement clean exit on SIGTERM. */
  fail_if (xsigaction(SIGTERM, received_terminate) < 0);
  
  /* Implement clean exit on SIGINT. */
  fail_if (xsigaction(SIGINT, received_terminate) < 0);
  
  /* Implement silent interruption on SIGRTMIN. */
  fail_if (xsigaction(SIGRTMIN, received_noop) < 0);
  
  /* Implement death on SIGDANGER or ignoral of SIGDANGER. */
  if (server_characteristics.danger_is_deadly && !is_immortal)
    { fail_if (xsigaction(SIGDANGER, commit_suicide) < 0); }
  else
    { fail_if (xsigaction(SIGDANGER, SIG_IGN) < 0); }
  
  return 0;
 pfail:
  xperror(*argv);
  return 1;
}


/**
 * This function should be implemented by the actual server implementation
 * if the server has set `server_characteristics.fork_for_safety` to be
 * true
 * 
 * This function is called by the parent server process when the
 * child server process exits, if the server has completed its
 * initialisation
 * 
 * @param  status  The status the child died with
 */
void __attribute__((weak)) fork_cleanup(int status)
{
  (void) status;
  fprintf(stderr, "Something is wrong, `fork_cleanup` has been called but not reimplemented.");
}


#undef try

