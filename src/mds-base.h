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
#ifndef MDS_MDS_BASE_H
#define MDS_MDS_BASE_H


#include <pthread.h>
#include <signal.h>


#define MDS_BASE_VARS_VERSION  0



/**
 * Characteristics of the server
 */
typedef struct server_characteristics
{
  /**
   * Setting this to zero will cause the server to drop privileges as a security precaution
   */
  int require_privileges : 1;
  
  /**
   * Setting this to non-zero will cause the server to connect to the display
   */
  int require_display : 1;
  
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
extern char** argv;

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
 * This function should be implemented by the actual server implementation
 * 
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `reexecing` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
extern void received_reexec(int signo);

/**
 * This function should be implemented by the actual server implementation
 * 
 * This function is called when a signal that
 * signals the server to re-exec has been received
 * 
 * When this function is invoked, it should set `terminating` to a non-zero value
 * 
 * @param  signo  The signal that has been received
 */
extern void received_terminate(int signo);

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
 * Unmarshal server implementation specific data and update the servers state accordingly
 * 
 * @param   state_buf  The marshalled data that as not been read already
 * @return             Non-zero on error
 */
extern int unmarshal_server(char* state_buf);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
extern int marshal_server(char* state_buf);

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
extern size_t marshal_server_size(void);

/**
 * This function should be implemented by the actual server implementation
 * 
 * Attempt to recover from an re-exec failure that has been
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


#endif

