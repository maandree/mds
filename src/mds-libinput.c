/**
 * mds — A micro-display server
 * Copyright © 2014, 2015, 2016  Mattias Andrée (maandree@member.fsf.org)
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
#include "mds-libinput.h"

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>

#include <linux/input.h>
#include <sys/select.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define reconnect_to_display() -1



#define MDS_LIBINPUT_VARS_VERSION  0



/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 1,
    .require_display = 1,
    .require_respawn_info = 0,
    .sanity_check_argc = 1,
    .fork_for_safety = 0,
    .danger_is_deadly = 0
  };



/**
 * Value of the ‘Message ID’ header for the next message
 */
static uint32_t message_id = 1;

/**
 * Buffer for received messages
 */
static mds_message_t received;

/**
 * Whether the server is connected to the display
 */
static int connected = 1;

/**
 * The seat for libinput
 */
static const char* seat = "seat0";

/**
 * libinput context
 */
static struct libinput* li = NULL;

/**
 * udev context
 */
static struct udev* udev = NULL;

/**
 * List of all opened devices
 */
static struct libinput_device** devices = NULL;

/**
 * The number of element slots allocated for `devices`
 */
static size_t devices_size = 0;

/**
 * The last element slot used in `devices`
 */
static size_t devices_used = 0;

/**
 * The index of the first free slot in `devices`
 */
static size_t devices_ptr = 0;

/**
 * The input event listener thread
 */
static pthread_t ev_thread;

/**
 * Whether `ev_thread` has started
 */
static volatile sig_atomic_t ev_thread_started = 0;

/**
 * Message buffer for the main thread
 */
static char* resp_send_buffer = NULL;

/**
 * The size of `resp_send_buffer`
 */
static size_t resp_send_buffer_size = 0;

/**
 * Message buffer for the event thread
 */
static char* anno_send_buffer = NULL;

/**
 * The size of `anno_send_buffer`
 */
static size_t anno_send_buffer_size = 0;

/**
 * File descriptor for the libinput events
 */
static int event_fd = -1;

/**
 * File descriptor set for select(3):ing
 * the libinput events
 */
static fd_set event_fd_set;

/**
 * Whether the server has been signaled to
 * free unneeded memory
 * 
 * For event thread
 */
static volatile sig_atomic_t ev_danger = 0;

/**
 * Whether the server has been signaled to
 * output runtime information
 * 
 * For event thread
 */
static volatile sig_atomic_t info = 0;

/**
 * Mutex that should be used when accessing
 * the device list
 */
static pthread_mutex_t dev_mutex;



/**
 * Send a full message even if interrupted
 * 
 * @param   message:const char*  The message to send
 * @param   length:size_t        The length of the message
 * @return  :int                 Zero on success, -1 on error
 */
#define full_send(message, length)  \
  ((full_send)(socket_fd, message, length))


/**
 * Parse command line arguments
 * 
 * @return  Non-zero on error
 */
int parse_cmdline(void)
{
  int i;
  
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
      else if (startswith(arg, "--seat=")) /* Seat to pass to libinput. */
	seat = arg + strlen("--seat=");
    }
  if (is_reexec)
    {
      is_respawn = 1;
      eprint("re-exec performed.");
    }
  
  /* Check that mandatory arguments have been specified. */
  if (server_characteristics.require_respawn_info)
    exit_if (is_respawn < 0,
	     eprintf("missing state argument, require either %s or %s.",
		     "--initial-spawn", "--respawn"););
  return 0;
}


/**
 * This function will be invoked before `initialise_server` (if not re-exec:ing)
 * or before `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) preinitialise_server(void)
{
  return 0;
}


/**
 * This function should initialise the server,
 * and it not invoked after a re-exec.
 * 
 * @return  Non-zero on error
 */
int initialise_server(void)
{
  int stage = 0;
  fail_if (server_initialised());
  fail_if (mds_message_initialise(&received));  stage++;
  
  return 0;
  
 fail:
  xperror(*argv);
  if (stage >= 1)  mds_message_destroy(&received);
  return 1;
}


/**
 * This function will be invoked after `initialise_server` (if not re-exec:ing)
 * or after `unmarshal_server` (if re-exec:ing)
 * 
 * @return  Non-zero on error
 */
int postinitialise_server(void)
{
  int stage = 0;
  fail_if (initialise_libinput());
  fail_if (pthread_mutex_init(&dev_mutex, NULL));  stage++;
  
  if (connected)
    return 0;
  
  fail_if (reconnect_to_display());
  connected = 1;
  return 0;
 fail:
  terminate_libinput();
  mds_message_destroy(&received);
  if (stage >= 1)  pthread_mutex_destroy(&dev_mutex);
  return 1;
}


/**
 * Calculate the number of bytes that will be stored by `marshal_server`
 * 
 * On failure the program should `abort()` or exit by other means.
 * However it should not be possible for this function to fail.
 * 
 * @return  The number of bytes that will be stored by `marshal_server`
 */
size_t marshal_server_size(void)
{
  size_t rc = 2 * sizeof(int) + sizeof(uint32_t);
  rc += mds_message_marshal_size(&received);
  return rc;
}


/**
 * Marshal server implementation specific data into a buffer
 * 
 * @param   state_buf  The buffer for the marshalled data
 * @return             Non-zero on error
 */
int marshal_server(char* state_buf)
{
  buf_set_next(state_buf, int, MDS_LIBINPUT_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, uint32_t, message_id);
  mds_message_marshal(&received, state_buf);
  
  mds_message_destroy(&received);
  return 0;
}


/**
 * Unmarshal server implementation specific data and update the servers state accordingly
 * 
 * On critical failure the program should `abort()` or exit by other means.
 * That is, do not let `reexec_failure_recover` run successfully, if it unrecoverable
 * error has occurred or one severe enough that it is better to simply respawn.
 * 
 * @param   state_buf  The marshalled data that as not been read already
 * @return             Non-zero on error
 */
int unmarshal_server(char* state_buf)
{
  /* buf_get_next(state_buf, int, MDS_LIBINPUT_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, uint32_t, message_id);
  fail_if (mds_message_unmarshal(&received, state_buf));
  
  return 0;
 fail:
  xperror(*argv);
  mds_message_destroy(&received);
  return -1;
}


/**
 * Attempt to recover from a re-exec failure that has been
 * detected after the server successfully updated it execution image
 * 
 * @return  Non-zero on error
 */
int __attribute__((const)) reexec_failure_recover(void)
{
  return -1;
}


/**
 * Send a singal to all threads except the current thread
 * 
 * @param  signo  The signal
 */
void signal_all(int signo)
{
  pthread_t current_thread = pthread_self();
  
  if (pthread_equal(current_thread, master_thread) == 0)
    pthread_kill(master_thread, signo);
  
  if (ev_thread_started)
    if (pthread_equal(current_thread, ev_thread) == 0)
      pthread_kill(ev_thread, signo);
}


/**
 * This function is called when a signal that
 * signals that the system is running out of memory
 * has been received
 * 
 * @param  signo  The signal that has been received
 */
void received_danger(int signo)
{
  SIGHANDLER_START;
  (void) signo;
  if ((danger == 0) || (ev_danger == 0))
    {
      danger = 1;
      ev_danger = 1;
      eprint("danger signal received.");
    }
  SIGHANDLER_END;
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  int rc = 1, joined = 0, r;
  void* ev_ret;
  
  /* Start thread that reads input events. */
  fail_if ((errno = pthread_create(&ev_thread, NULL, event_loop, NULL)));
  
  /* Listen for messages. */
  while (!reexecing && !terminating)
    {
      if (info)
	dump_info();
      if (danger)
	{
	  danger = 0;
	  free(resp_send_buffer), resp_send_buffer = NULL;
	  resp_send_buffer_size = 0;
	  pack_devices();
	}
      
      if (r = mds_message_read(&received, socket_fd), r == 0)
	if (r = handle_message(), r == 0)
	  continue;
      
      if (r == -2)
	{
	  eprint("corrupt message received, aborting.");
	  goto done;
	}
      else if (errno == EINTR)
	continue;
      else
	fail_if (errno != ECONNRESET);
      
      eprint("lost connection to server.");
      mds_message_destroy(&received);
      mds_message_initialise(&received);
      connected = 0;
      fail_if (reconnect_to_display());
      connected = 1;
    }
  
  joined = 1;
  fail_if ((errno = pthread_join(ev_thread, &ev_ret)));
  rc = ev_ret == NULL ? 0 : 1;
  goto done;
 fail:
  xperror(*argv);
 done:
  free(resp_send_buffer);
  if (!joined && (errno = pthread_join(ev_thread, NULL)))
    xperror(*argv);
  if (!rc && reexecing)
    return 0;
  mds_message_destroy(&received);
  terminate_libinput();
  return rc;
}


/**
 * The event listener thread's main function
 * 
 * @param   data  Input data
 * @return        Output data
 */
void* event_loop(void* data)
{
  (void) data;
  
  ev_thread_started = 1;
  
  if (handle_event() < 0)
    fail_if (errno != EINTR);
  while (!reexecing && !terminating)
    {
      if (ev_danger)
	{
	  ev_danger = 0;
	  free(anno_send_buffer), anno_send_buffer = NULL;
	  anno_send_buffer_size = 0;
	}
      
      FD_SET(event_fd, &event_fd_set);
      if (select(event_fd + 1, &event_fd_set, NULL, NULL, NULL) < 0)
	{
	  fail_if (errno != EINTR);
	  continue;
	}
      if (handle_event() < 0)
	fail_if (errno != EINTR);
    }
  
  return NULL;
  
 fail:
  xperror(*argv);
  raise(SIGTERM);
  return (void*)1024;
}


/**
 * Handle an event from libinput
 * 
 * @return  Zero on success, -1 on error
 */
int handle_event(void)
{
  struct libinput_event* ev;
  if ((errno = -libinput_dispatch(li)))
    return -1;
  while ((ev = libinput_get_event(li))) {
    switch (libinput_event_get_type(ev)) {
      /* TODO */
    default:
      break;
    }
    libinput_event_destroy(ev);
    if ((errno = -libinput_dispatch(li)))
      return -1;
  }
  return 0;
}


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void)
{
  /* TODO */
  return 0;
}


/**
 * Used by libinput to open a device
 * 
 * @param   path      The filename of the device
 * @param   flags     The flags to open(3)
 * @param   userdata  Not used
 * @return            The file descriptor, or `-errno` on error
 */
int open_restricted(const char* path, int flags, void* userdata)
{
  int fd = open(path, flags);
  if (fd < 0)
    return perror(*argv), -errno;
  return fd;
  (void) userdata;
}


/**
 * Used by libinput to close device
 * 
 * @param  fd        The file descriptor of the device
 * @param  userdata  Not used
 */
void close_restricted(int fd, void* userdata)
{
  close(fd);
  (void) userdata;
}


/**
 * Acquire access of input devices
 * 
 * @return  Zero on success, -1 on error
 */
int initialise_libinput(void)
{
  const struct libinput_interface interface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted
  };
  
  if (!(udev = udev_new()))
    return eprint("failed to initialize udev."), errno = 0, -1;
  if (!(li = libinput_udev_create_context(&interface, NULL, udev)))
    return eprint("failed to initialize context from udev."), errno = 0, -1;
  if (libinput_udev_assign_seat(li, seat))
    return eprintf("failed to set seat: %s", seat), errno = 0, -1;
  
  event_fd = libinput_get_fd(li);
  FD_ZERO(&event_fd_set);
  
  return 0;
}


/**
 * Release access of input devices
 */
void terminate_libinput(void)
{
  while (devices_used--)
    if (devices[devices_used])
      libinput_device_unref(devices[devices_used]);
  if (li)    libinput_unref(li);
  if (udev)  udev_unref(udev);
}


/**
 * Add a device to the device list
 * 
 * @param   dev  The device
 * @return       Zero on success, -1 on error
 */
int add_device(struct libinput_device* dev)
{
  if (devices_ptr == devices_size)
    {
      struct libinput_device** tmp;
      if (yrealloc(tmp, devices, devices_size + 10, struct libinput_device*))
	return -1;
      devices_size += 10;
    }
  
  devices[devices_ptr++] = libinput_device_ref(dev);
  while ((devices_ptr < devices_used) && devices[devices_ptr])
    devices_ptr++;
  
  return 0;
}


/**
 * Remove a device from the device list
 * 
 * @param  dev  The device
 */
void remove_device(struct libinput_device* dev)
{
  size_t i;
  
  for (i = 0; i < devices_used; i++)
    if (devices[i] == dev)
      {
	libinput_device_unref(dev);
	devices[i] = NULL;
	if (i < devices_ptr)
	  devices_ptr = i;
	if (i + 1 == devices_used)
	  devices_used -= 1;
	break;
      }
}


/**
 * Pack the device list
 */
void pack_devices(void)
{
  size_t i;
  
  for (i = devices_ptr = 0; i < devices_used; i++)
    if (devices[i])
      devices[devices_ptr++] = devices[i];
  devices_used = devices_ptr;
  
  if (devices_used)
    {
      struct libinput_device** tmp;
      if (yrealloc(tmp, devices, devices_used, struct libinput_device*))
	return;
      devices_size = devices_used;
    }
  else
    {
      free(devices), devices = NULL;
      devices_size = 0;
    }
}


/**
 * This function is called when a signal that
 * signals that the system to dump state information
 * and statistics has been received
 * 
 * @param  signo  The signal that has been received
 */
void received_info(int signo)
{
  SIGHANDLER_START;
  (void) signo;
  info = 1;
  SIGHANDLER_END;
}


/**
 * The the state of the server
 */
void dump_info(void)
{
  info = 1;
  iprintf("next message ID: %" PRIu32, message_id);
  iprintf("connected: %s", connected ? "yes" : "no");
  iprintf("libinput seat: %s", seat);
  iprintf("sigdanger pending (main): %s", danger ? "yes" : "no");
  iprintf("sigdanger pending (event): %s", ev_danger ? "yes" : "no");
  iprintf("response send buffer size: %zu bytes", resp_send_buffer_size);
  iprintf("announce send buffer size: %zu bytes", anno_send_buffer_size);
  iprintf("event file descriptor: %i", event_fd);
  iprintf("event thread started: %s", ev_thread_started ? "yes" : "no");
  /* TODO list devices -- with_mutex(dev_mutex, ); */
}

