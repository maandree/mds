/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016, 2017  Mattias Andrée (maandree@kth.se)
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
#ifndef MDS_MDS_BASE_H
#define MDS_MDS_BASE_H


#include <pthread.h>
#include <signal.h>


#define MDS_BASE_VARS_VERSION 0



/**
 * Characteristics of the server
 */
typedef struct server_characteristics {
	/**
	 * Setting this to zero will cause the server to drop privileges as a security precaution
	 */
	unsigned require_privileges : 1;

	/**
	 * Setting this to non-zero will cause the server to connect to the display
	 */
	unsigned require_display : 1;

	/**
	 * Setting this to non-zero will cause the server to refuse to
	 * start unless either --initial-spawn or --respawn is used
	 */
	unsigned require_respawn_info : 1;

	/**
	 * Setting this to non-zero will cause the server to refuse to
	 * start if there are too many command line arguments
	 */
	unsigned sanity_check_argc : 1;

	/**
	 * Setting this to non-zero will cause the server to place
	 * itself in a fork of itself when initialised. This can be
	 * used to let the server clean up fatal stuff after itself
	 * if it crashes. When the child exits, no matter how it
	 * exits, the parent will call `fork_cleanup` and then
	 * die it the same manner as the child.
	 */
	unsigned fork_for_safety : 1;

	/**
	 * Setting this to non-zero without setting a signal action
	 * for `SIGDANGER` will cause the server to die if `SIGDANGER`
	 * is received. It is safe to set both `danger_is_deadly` and
	 * `fork_for_safety` to non-zero, during the call of
	 * `server_initialised` the signal handler for `SIGDANGER`
	 * in the parent process will be set to `SIG_IGN` independently
	 * of the value of `danger_is_deadly` if `fork_for_safety`
	 * is set to non-zero.
	 * 
	 * This setting will be treated as set to zero if
	 * --immortal is used.
	 */
	unsigned danger_is_deadly : 1;
} __attribute__((packed)) server_characteristics_t;



/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
extern server_characteristics_t server_characteristics;


/**
 * Number of elements in `argv`
 */
extern int argc;

/**
 * Command line arguments
 */
extern char **argv;

/**
 * Whether the server has been respawn
 * rather than this being the initial spawn.
 * This will be at least as true as `is_reexec`.
 */
extern int is_respawn;

/**
 * Whether the server is continuing
 * from a self-reexecution
 */
extern int is_reexec;

/**
 * Whether the server should do its
 * best to resist event triggered death
 */
extern int is_immortal;

/**
 * Whether to fork the process when the
 * server has been properly initialised
 */
extern int on_init_fork;

/**
 * Command the run (`NULL` for none) when
 * the server has been properly initialised
 */
extern char* on_init_sh;

/**
 * The thread that runs the master loop
 */
extern pthread_t master_thread;


/**
 * Whether the server has been signaled to terminate
 */
extern volatile sig_atomic_t terminating;

/**
 * Whether the server has been signaled to re-exec
 */
extern volatile sig_atomic_t reexecing;

/**
 * Whether the server has been signaled to free unneeded memory
 */
extern volatile sig_atomic_t danger;


/**
 * The file descriptor of the socket
 * that is connected to the server
 */
extern int socket_fd;



/**
 * Set up signal traps for all especially handled signals
 * 
 * @return  Non-zero on error
 */
int trap_signals(void);


/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
int parse_cmdline(void); /* __attribute__((weak)) */


/**
 * Connect to the display
 * 
 * @return  Non-zero on error
 */
int connect_to_display(void); /* __attribute__((weak)) */

/**
 * This function should be called when the server has
 * been properly initialised but before initialisation
 * of anything that is removed at forking is initialised
 * 
 * @return  Zero on success, -1 on error
 */
int server_initialised(void); /* __attribute__((weak)) */


/**
 * This function should be implemented by the actual server implementation
 * if the server is multi-threaded
 * 
 * Send a singal to all threads except the current thread
 * 
 * @param  signo  The signal
 */
void signal_all(int signo); /* __attribute__((weak)) */

/**
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `reexecing` and
 * `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void received_reexec(int signo); /* __attribute__((weak)) */

/**
 * This function is called when a signal that
 * signals the server to terminate has been received
 * 
 * When this function is invoked, it should set `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void received_terminate(int signo); /* __attribute__((weak)) */

/**
 * This function is called when a signal that signals that
 * the system is running out of memory has been received
 * 
 * When this function is invoked, it should set `danger` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
void received_danger(int signo); /* __attribute__((weak)) */

/**
 * This function is called when a signal that
 * signals that the system to dump state information
 * and statistics has been received
 * 
 * @param  signo  The signal that has been received
 */
void received_info(int signo); /* __attribute__((weak)) */

/**
 * This function should be implemented by the actual server implementation
 * 
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
extern int preinitialise_server(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * This function should initialise the server,
 * and it not invoked after a re-exec.
 * 
 * @return  Non-zero on error
 */
extern int initialise_server(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * This function will be invoked after `initialise_server` (if not re-exec:ing)
 * or after `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
extern int postinitialise_server(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
__attribute__((pure))
extern size_t marshal_server_size(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
__attribute__((nonnull))
extern int marshal_server(char *state_buf);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Unmarshal server implementation specific data and update the servers state accordingly
 * 
 * On critical failure the program should `abort()` or exit by other means.
 * That is, do not let `reexec_failure_recover` run successfully, if it unrecoverable
 * error has occurred or one severe enough that it is better to simply respawn.
 * 
 * @param   state_buf  The marshalled data that as not been read already
 * @return             Non-zero on error
 */
__attribute__((nonnull))
extern int unmarshal_server(char *state_buf);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
extern int reexec_failure_recover(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
extern int master_loop(void);

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
void fork_cleanup(int status); /* __attribute__((weak)) */


#endif
