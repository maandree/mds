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
#include "mds-kkbd.h"
/* TODO: This server should wait for `Command: get-vt` to be available,
         query the active VT and connect to that TTY instead of stdin. */
/* TODO: Release control on `Command: switching-vt`. */

#include <libmdsserver/macros.h>
#include <libmdsserver/util.h>
#include <libmdsserver/mds-message.h>

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <pthread.h>
#define reconnect_to_display() -1



#ifdef __sparc__
# define GET_LED  KIOCGLED
# define SET_LED  KIOCSLED
#else
# define GET_LED  KDGETLED
# define SET_LED  KDSETLED
#endif


#ifdef __sparc__
# define LED_NUM_LOCK   1
# define LED_CAPS_LOCK  8
# define LED_SCRL_LOCK  4
# define LED_COMPOSE    2
#else
# define LED_NUM_LOCK   LED_NUM
# define LED_CAPS_LOCK  LED_CAP
# define LED_SCRL_LOCK  LED_SCR
#endif



#define MDS_KKBD_VARS_VERSION  0


/**
 * The name of the keyboard for which its server implements control
 */
#define KEYBOARD_ID  "kernel"
/* NOTE: length hardcoded in `initialise_server` */

/**
 * LED:s that we believe are pressent on the keyboard
 */
#ifdef LED_COMPOSE
# define PRESENT_LEDS  "num caps scrl compose"
#else
# define PRESENT_LEDS  "num caps scrl"
#endif



/**
 * This variable should declared by the actual server implementation.
 * It must be configured before `main` is invoked.
 * 
 * This tells the server-base how to behave
 */
server_characteristics_t server_characteristics =
  {
    .require_privileges = 0,
    .require_display = 1,
    .require_respawn_info = 0,
    .sanity_check_argc = 1,
    .fork_for_safety = 1,
    .danger_is_deadly = 0
  };



/**
 * Value of the ‘Message ID’ header for the next message
 */
static uint32_t message_id = 3;

/**
 * Buffer for received messages
 */
static mds_message_t received;

/**
 * Whether the server is connected to the display
 */
static int connected = 1;

/**
 * File descriptor for accessing the keyboard LED:s
 */
static int ledfd = 0;

/**
 * Saved LED states
 */
static int saved_leds;

/**
 * Saved TTY settings
 */
static struct termios saved_stty;

/**
 * Save keyboard mode
 */
static int saved_kbd_mode;

/**
 * Keycode remapping table
 */
static int* restrict mapping = NULL;

/**
 * The size of `mapping`
 */
static size_t mapping_size = 0;

/**
 * Scancode buffer
 */
static int scancode_buf[3] = { 0, 0, 0 };

/**
 * The number of elements stored in `scancode_buf`
 */
static int scancode_ptr = 0;

/**
 * Message buffer for `send_key`
 */
static char key_send_buffer[111];

/**
 * Message buffer for the main thread
 */
static char* send_buffer = NULL;

/**
 * The size of `send_buffer`
 */
static size_t send_buffer_size = 0;

/**
 * The keyboard listener thread
 */
static pthread_t kbd_thread;

/**
 * Whether `kbd_thread` has started
 */
static int kbd_thread_started = 0;

/**
 * Mutex that should be used when sending message
 */
static pthread_mutex_t send_mutex;

/**
 * Mutex that should be used when accessing the keycode map
 */
static pthread_mutex_t mapping_mutex;

/**
 * Whether the server should free memory as soon as possible
 */
static int danger = 0;



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
  const char* const message =
    "Command: intercept\n"
    "Message ID: 0\n"
    "Length: 75\n"
    "\n"
    "Command: set-keyboard-leds\n"
    "Command: get-keyboard-leds\n"
    "Command: keycode-map\n"
    /* NEXT MESSAGE */
    "Command: intercept\n"
    "Message ID: 1\n"
    "Modifying: yes\n"
    "Length: 59\n"
    "\n"
    "Command: enumerate-keyboards\n"
    "Command: keyboard-enumeration\n"
    /* NEXT MESSAGE */
    "Command: new-keyboard\n"
    "Message ID: 2\n"
    "Length: 7\n"
    "\n"
    KEYBOARD_ID "\n";
  
  fail_if (open_leds() < 0);
  stage = 1;
  fail_if (open_input() < 0);
  stage = 2;
  fail_if (pthread_mutex_init(&send_mutex, NULL));
  stage = 3;
  fail_if (pthread_mutex_init(&mapping_mutex, NULL));
  stage = 4;
  
  if (full_send(message, strlen(message)))
    return 1;
  
  fail_if (server_initialised());
  stage = 5;
  fail_if (mds_message_initialise(&received));
  
  return 0;
  
 pfail:
  xperror(*argv);
  if (stage < 5)
    {
      if (stage >= 2)  close_input();
      if (stage >= 1)  close_leds();
    }
  if (stage >= 3)
    pthread_mutex_destroy(&send_mutex);
  if (stage >= 4)
    pthread_mutex_destroy(&mapping_mutex);
  mds_message_destroy(&received);
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
  if (connected)
    return 0;
  
  if (reconnect_to_display())
    {
      mds_message_destroy(&received);
      return 1;
    }
  connected = 1;
  return 0;
}


/**
 * This function is called by the parent server process when the
 * child server process exits, if the server has completed its
 * initialisation
 * 
 * @param  status  The status the child died with
 */
void fork_cleanup(int status)
{
  (void) status;
  close_input();
  close_leds();
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
  size_t rc = 9 * sizeof(int) + sizeof(uint32_t) + sizeof(struct termios);
  rc += sizeof(size_t) + mapping_size * sizeof(int);
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
  buf_set_next(state_buf, int, MDS_KKBD_VARS_VERSION);
  buf_set_next(state_buf, int, connected);
  buf_set_next(state_buf, uint32_t, message_id);
  buf_set_next(state_buf, int, ledfd);
  buf_set_next(state_buf, int, saved_leds);
  buf_set_next(state_buf, struct termios, saved_stty);
  buf_set_next(state_buf, int, saved_kbd_mode);
  buf_set_next(state_buf, int, scancode_ptr);
  buf_set_next(state_buf, int, scancode_buf[0]);
  buf_set_next(state_buf, int, scancode_buf[1]);
  buf_set_next(state_buf, int, scancode_buf[2]);
  buf_set_next(state_buf, size_t, mapping_size);
  if (mapping_size > 0)
    {
      memcpy(state_buf, mapping, mapping_size * sizeof(int));
      state_buf += mapping_size * sizeof(int) / sizeof(char);
    }
  mds_message_marshal(&received, state_buf);
  
  mds_message_destroy(&received);
  free(mapping);
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
  /* buf_get_next(state_buf, int, MDS_KKBDOARD_VARS_VERSION); */
  buf_next(state_buf, int, 1);
  buf_get_next(state_buf, int, connected);
  buf_get_next(state_buf, uint32_t, message_id);
  buf_get_next(state_buf, int, ledfd);
  buf_get_next(state_buf, int, saved_leds);
  buf_get_next(state_buf, struct termios, saved_stty);
  buf_get_next(state_buf, int, saved_kbd_mode);
  buf_get_next(state_buf, int, scancode_ptr);
  buf_get_next(state_buf, int, scancode_buf[0]);
  buf_get_next(state_buf, int, scancode_buf[1]);
  buf_get_next(state_buf, int, scancode_buf[2]);
  buf_get_next(state_buf, size_t, mapping_size);
  if (mapping_size > 0)
    {
      fail_if (xmalloc(mapping, mapping_size, int));
      memcpy(mapping, state_buf, mapping_size * sizeof(int));
      state_buf += mapping_size * sizeof(int) / sizeof(char);
    }
  fail_if (mds_message_unmarshal(&received, state_buf));
  
  return 0;
 pfail:
  xperror(*argv);
  mds_message_destroy(&received);
  free(mapping);
  abort(); /* We must abort on failure to not risk the keyboard
	      getting stuck and freeze up the computer until
	      someone ssh:es into it and kill the server. */
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
 * This function is called when a signal that
 * signals that the system is running out of memory
 * has been received
 * 
 * @param  signo  The signal that has been received
 */
void received_danger(int signo)
{
  (void) signo;
  danger = 1;
}


/**
 * Perform the server's mission
 * 
 * @return  Non-zero on error
 */
int master_loop(void)
{
  int rc = 1, joined = 0, r;
  void* kbd_ret;
  
  fail_if ((errno = pthread_create(&kbd_thread, NULL, keyboard_loop, NULL)));
  
  while (!reexecing && !terminating)
    {
      if (danger)
	{
	  danger = 0;
	  free(send_buffer), send_buffer = NULL;
	  send_buffer_size = 0;
	}
      
      if (r = mds_message_read(&received, socket_fd), r == 0)
	{
	  if (r = handle_message(), r == 0)
	    continue;
	}
      
      if (r == -2)
	{
	  eprint("corrupt message received, aborting.");
	  goto fail;
	}
      else if (errno == EINTR)
	continue;
      else if (errno != ECONNRESET)
	goto pfail;
      
      eprint("lost connection to server.");
      mds_message_destroy(&received);
      mds_message_initialise(&received);
      connected = 0;
      if (reconnect_to_display())
	goto fail;
      connected = 1;
    }
  
  joined = 1;
  fail_if ((errno = pthread_join(kbd_thread, &kbd_ret)));
  rc = kbd_ret == NULL ? 0 : 1;
  goto fail;
 pfail:
  xperror(*argv);
 fail:
  pthread_mutex_destroy(&send_mutex);
  pthread_mutex_destroy(&mapping_mutex);
  free(send_buffer);
  if ((!joined) && (errno = pthread_join(kbd_thread, NULL)))
    xperror(*argv);
  if (!rc && reexecing)
    return 0;
  mds_message_destroy(&received);
  free(mapping);
  return rc;
}


/**
 * The keyboard listener thread's main function
 * 
 * @param   data  Input data
 * @return        Output data
 */
void* keyboard_loop(void* data)
{
  (void) data;
  
  kbd_thread_started = 1;
  
  while (!reexecing && !terminating)
    if (fetch_keys() < 0)
      if (errno != EINTR)
	goto pfail;
  
  return NULL;
  
 pfail:
  xperror(*argv);
  raise(SIGTERM);
  return (void*)1024;
}


/**
 * Handle the received message
 * 
 * @return  Zero on success, -1 on error
 */
int handle_message(void)
{
  const char* recv_command = NULL;
  const char* recv_client_id = "0:0";
  const char* recv_message_id = NULL;
  const char* recv_modify_id = NULL;
  const char* recv_active = NULL;
  const char* recv_mask = NULL;
  const char* recv_keyboard = NULL;
  const char* recv_action = NULL;
  size_t i;
  
#define __get_header(storage, header)  \
  (startswith(received.headers[i], header))  \
    storage = received.headers[i] + strlen(header)
  
  for (i = 0; i < received.header_count; i++)
    {
      if      __get_header(recv_command,    "Command: ");
      else if __get_header(recv_client_id,  "Client ID: ");
      else if __get_header(recv_message_id, "Message ID: ");
      else if __get_header(recv_modify_id,  "Modify ID: ");
      else if __get_header(recv_active,     "Active: ");
      else if __get_header(recv_mask,       "Mask: ");
      else if __get_header(recv_keyboard,   "Keyboard: ");
      else if __get_header(recv_action,     "Action: ");
    }
  
#undef __get_header
  
  if (recv_message_id == NULL)
    {
      eprint("received message with ID, ignoring, master server is misbehaving.");
      return 0;
    }
  
  if (recv_command == NULL)
    return 0; /* How did that get here, not matter, just ignore it? */
  
  if (strequals(recv_command, "enumerate-keyboards"))
    return handle_enumerate_keyboards(recv_client_id, recv_message_id, recv_modify_id);
  if (strequals(recv_command, "keyboard-enumeration"))
    return handle_keyboard_enumeration(recv_modify_id);
  if (strequals(recv_command, "set-keyboard-leds"))
    return handle_set_keyboard_leds(recv_active, recv_mask, recv_keyboard);
  if (strequals(recv_command, "get-keyboard-leds"))
    return handle_get_keyboard_leds(recv_client_id, recv_message_id, recv_keyboard);
  if (strequals(recv_command, "keycode-map"))
    return handle_keycode_map(recv_client_id, recv_message_id, recv_action, recv_keyboard);
  
  return 0; /* How did that get here, not matter, just ignore it? */
}


/**
 * Make sure `send_buffer` is large enough
 * 
 * @param   size  The size required for the buffer
 * @return        Zero on success, -1 on error
 */
static int ensure_send_buffer_size(size_t size)
{
  char* old = send_buffer;
  
  if (send_buffer_size >= size)
    return 0;
  
  if (xrealloc(send_buffer, size, char))
    {
      send_buffer = old;
      return -1;
    }
  else
    send_buffer_size = size;
  
  return 0;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: enumerate-keyboards`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_modify_id   The value of the `Modify ID`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_enumerate_keyboards(const char* recv_client_id, const char* recv_message_id,
			       const char* recv_modify_id)
{
  uint32_t msgid;
  size_t n;
  int r;
  
  if (recv_modify_id == NULL)
    {
      eprint("did not get add modify ID, ignoring.");
      return 0;
    }
  
  
  if (strequals(recv_client_id, "0:0"))
    {
      eprint("received information request from an anonymous client, sending non-modifying response.");
      
      with_mutex (send_mutex,
		  msgid = message_id;
		  message_id = message_id == UINT32_MAX ? 0 : (message_id + 1);
		  );
      
      if (ensure_send_buffer_size(48 + strlen(recv_modify_id) + 1) < 0)
	return -1;
      sprintf(send_buffer,
	      "Modify: no\n"
	      "Modify ID: %s\n"
	      "Message ID: %" PRIu32 "\n"
	      "\n",
	      recv_modify_id, msgid);
      
      with_mutex (send_mutex,
		  r = full_send(send_buffer, strlen(send_buffer));
		  );
      return r;
    }
  
  with_mutex (send_mutex,
	      msgid = message_id;
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      );
  
  n = 176 + 3 * sizeof(size_t) + strlen(KEYBOARD_ID);
  n += strlen(recv_modify_id) + strlen(recv_message_id);
  if (ensure_send_buffer_size(n + 1) < 0)
    return -1;
  sprintf(send_buffer,
	  "Modify: yes\n"
	  "Modify ID: %s\n"
	  "Message ID: %" PRIu32 "\n"
	  "\n"
	  /* NEXT MESSAGE */
	  "Command: keyboard-enumeration\n"
	  "To: %s\n"
	  "In response to: %s\n"
	  "Length: %zu\n"
	  "Message ID: %" PRIu32 "\n"
	  "\n"
	  KEYBOARD_ID "\n",
	  recv_modify_id, msgid,
	  recv_message_id, recv_client_id, strlen(KEYBOARD_ID) + 1, msgid + 1);
  
  with_mutex (send_mutex,
	      r = full_send(send_buffer, strlen(send_buffer));
	      );
  return r;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: keyboard-enumeration`
 * 
 * @param   recv_modify_id  The value of the `Modify ID`-header, `NULL` if omitted
 * @return                  Zero on success, -1 on error
 */
int handle_keyboard_enumeration(const char* recv_modify_id)
{
  size_t i, off, top, n = 1 + strlen(KEYBOARD_ID "\n") + 3 * sizeof(size_t);
  uint32_t msgid;
  int r, have_len = 0;
  
  if (recv_modify_id == NULL)
    {
      eprint("did not get add modify ID, ignoring.");
      return 0;
    }
  
  for (i = 0; i < received.header_count; i++)
    n += strlen(received.headers[i]);
  n += received.header_count;
  n += received.payload_size;
  
  n += off = 64 + strlen(recv_modify_id) + 3 * sizeof(size_t);
  
  if (ensure_send_buffer_size(n + 1) < 0)
    return -1;
  
  with_mutex (send_mutex,
	      msgid = message_id;
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      );
  
  n = off;
  for (i = 0; i < received.header_count; i++)
    {
      const char* header = received.headers[i];
      if (!have_len && startswith(header, "Length: "))
	{
	  have_len = 1;
	  sprintf(send_buffer + n,
		  "Length: %zu\n",
		  strlen(KEYBOARD_ID "\n") + received.payload_size);
	  n += strlen(send_buffer + n);
	}
      else
	{
	  sprintf(send_buffer + n,
		  "%s\n",
		  header);
	  n += strlen(header) + 1;
	}
    }
  memcpy(send_buffer + n, received.payload, received.payload_size * sizeof(char));
  n += received.payload_size;
  n -= off;
  
  sprintf(send_buffer,
	  "Modify ID: %s\n"
	  "Message ID: %" PRIu32 "\n"
	  "Length: %zu\n",
	  recv_modify_id, msgid, n);
  top = strlen(send_buffer) + 1;
  send_buffer[top - 1] = '\n';
  off -= top;
  n += top;
  memmove(send_buffer + off, send_buffer, top * sizeof(char));
  
  with_mutex (send_mutex,
	      r = full_send(send_buffer + off, n);
	      );
  
  return r;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: set-keyboard-leds`
 * 
 * @param   recv_active    The value of the `Active`-header, `NULL` if omitted
 * @param   recv_mask      The value of the `Mask`-header, `NULL` if omitted
 * @param   recv_keyboard  The value of the `Keyboard`-header, `NULL` if omitted
 * @return                 Zero on success, -1 on error
 */
int handle_set_keyboard_leds(const char* recv_active, const char* recv_mask,
			     const char* recv_keyboard)
{
  int active = 0;
  int mask = 0;
  int current;
  const char* begin;
  const char* end;
  
  if ((recv_keyboard != NULL) && !strequals(recv_keyboard, KEYBOARD_ID))
    return 0;
  
  if (recv_active == NULL)
    {
      eprint("received LED writing request without active header, ignoring.");
      return 0;
    }
  
  if (recv_mask == NULL)
    {
      eprint("received LED writing request without mask header, ignoring.");
      return 0;
    }
  
  current = get_leds();
  if (current < 0)
    {
      xperror(*argv);
      return 0; /* Not fatal */
    }
  
#define __test(have, want)  (startswith(have, want " ") || strequals(have, want))
  
  for (begin = end = recv_active; end != NULL;)
    {
      end = strchr(begin, ' ');
      if      (__test(begin, "num"))      active |= LED_NUM_LOCK;
      else if (__test(begin, "caps"))     active |= LED_CAPS_LOCK;
      else if (__test(begin, "scroll"))   active |= LED_SCRL_LOCK;
#ifdef LED_COMPOSE
      else if (__test(begin, "compose"))  active |= LED_COMPOSE;
#endif
      begin = end + 1;
    }
  
  for (begin = end = recv_mask; end != NULL;)
    {
      end = strchr(begin, ' ');
      if      (__test(begin, "num"))      mask |= LED_NUM_LOCK;
      else if (__test(begin, "caps"))     mask |= LED_CAPS_LOCK;
      else if (__test(begin, "scroll"))   mask |= LED_SCRL_LOCK;
#ifdef LED_COMPOSE
      else if (__test(begin, "compose"))  mask |= LED_COMPOSE;
#endif
      begin = end + 1;
    }
  
#undef __test
  
  current = (active & mask) | ((current ^ active) & ~mask);
  if (set_leds(current) < 0)
    {
      xperror(*argv); /* Not fatal */
      return 0;
    }
  
  return 0;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: get-keyboard-leds`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_keyboard    The value of the `Keyboard`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_get_keyboard_leds(const char* recv_client_id, const char* recv_message_id,
			     const char* recv_keyboard)
{
  uint32_t msgid;
  size_t n;
  int r, leds;
  
  if ((recv_keyboard != NULL) && !strequals(recv_keyboard, KEYBOARD_ID))
    return 0;
  
  if (recv_keyboard == NULL)
    {
      eprint("received LED reading request but no specified keyboard, ignoring.");
      return 0;
    }
  
  if (strequals(recv_client_id, "0:0"))
    {
      eprint("received information request from an anonymous client, ignoring.");
      return 0;
    }
  
  leds = get_leds();
  if (leds < 0)
    {
      int error = errno;
      xperror(*argv);
      send_errno(error, recv_client_id, recv_message_id);
      return -1;
    }
  
  with_mutex (send_mutex,
	      msgid = message_id;
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      );
  
  n = 95 + 3 * sizeof(size_t) + 2 * strlen(PRESENT_LEDS);
  n += strlen(recv_client_id) + strlen(recv_message_id);
  if (ensure_send_buffer_size(n + 1) < 0)
    return -1;
  sprintf(send_buffer,
	  "To: %s\n"
	  "In response to: %s\n"
	  "Message ID: %" PRIu32 "\n"
	  "Active:%s%s%s%s%s\n"
	  "Present: " PRESENT_LEDS "\n"
	  "\n",
	  recv_client_id, recv_message_id, msgid,
	  (leds & LED_NUM_LOCK)  ? " num"    : "",
	  (leds & LED_CAPS_LOCK) ? " caps"   : "",
	  (leds & LED_SCRL_LOCK) ? " scroll" : "",
#ifdef LED_COMPOSE
	  (leds & LED_COMPOSE) ? " compose"  :
#endif
	  "",
	  leds == 0 ? " none" : ""
	  );
  
  with_mutex (send_mutex,
	      r = full_send(send_buffer, strlen(send_buffer));
	      );
  
  return r;
}


/**
 * Parse a keycode rampping line
 * 
 * @param   begin  The beginning of the line
 * @param   end    The end of the line, `NULL` if it is not terminated by a new line
 * @param   n      The size of the table from the position of `begin`
 * @param   in     Output parameter for the keycode that should be remapped
 * @param   out    Output parameter for the keycode's new mapping
 * @return         -1 on error, 1 if parsed, 0 if the line is empty
 */
static int parse_remap_line(char* begin, char* end, size_t n, int* restrict in, int* restrict out)
{
  static char buf[3 * sizeof(int) + 1];
  
  size_t len = end == NULL ? n : (size_t)(end - begin);
  char* delimiter = memchr(begin, ' ', len);
  
  if (len == 0)
    return 0;
  
  if (delimiter == NULL)
    {
      *in = -1, *out = -1;
      return -1;
    }
  
  *delimiter++ = '\0';
  *in = atoi(begin);
  if (end == NULL)
    {
      snprintf(buf, sizeof(buf) / sizeof(char), "%.*s",
	       (int)(len - (size_t)(delimiter - begin)), delimiter);
      *out = atoi(buf);
    }
  else
    {
      *end = '\0';
      *out = atoi(delimiter);
    }
  
  return 1;
}


/**
 * Add a mapping to the keycode mapping table
 * 
 * @param   in   The keycode to remap
 * @parma   out  The keycode's new mapping
 * @return       Zero on success, -1 on error
 */
static int add_mapping(int in, int out)
{
  size_t n = ((size_t)in) + 1;
  int* old;
  
  if (n > mapping_size)
    {
      if (in == out)
	return 0;
      
      if (old = mapping, xrealloc(mapping, n, int))
	{
	  mapping = old;
	  return -1;
	}
      
      for (; mapping_size < n; mapping_size++)
	mapping[mapping_size] = (int)mapping_size;
    }
  
  mapping[in] = out;
  return 0;
}


/**
 * Change the keycode mapping
 * 
 * @param   table  The remapping table as described by the `Command: keycode-map` protocol
 * @param   n      The size of `table`
 * @return         Zero on success, -1 on error
 */
static int remap(char* table, size_t n)
{
  char* begin = table;
  int greatest_remap = -1;
  int greatest_reset = -1;
  
  for (;;)
    {
      char* end = memchr(begin, '\n', n);
      int in, out;
      
      if (!parse_remap_line(begin, end, n, &in, &out))
	goto next;
      if ((in < 0) || (out < 0) || ((in | out) >= 0x4000))
	{
	  eprint("received malformated remapping table.");
	  goto next;
	}
      
      if (in != out)  greatest_remap = max(greatest_remap, in);
      else            greatest_reset = max(greatest_reset, in);
      
      if (add_mapping(in, out) < 0)
	return -1;
      
    next:
      if (end == NULL)
	break;
      end++;
      n -= (size_t)(end - begin);
      begin = end;
    }
  
  if ((greatest_reset > greatest_remap) && (((((size_t)greatest_remap) + 1) >> 1) < mapping_size))
    shrink_map();
  
  return 0;
}


/**
 * Responde to a keycode mapping query
 * 
 * @param   recv_client_id   The value of the `Client ID`-header
 * @param   recv_message_id  The value of the `Message ID`-header
 * @return                   Zero on success, -1 on error
 */
static int mapping_query(const char* recv_client_id, const char* recv_message_id)
{
  size_t top = 64 + 3 * sizeof(size_t), n = 0, off, i;
  int greatest = (int)mapping_size, r;
  uint32_t msgid;
  
  for (i = 0; i < mapping_size; i++)
    if (mapping[i] != (int)i)
      {
	greatest = max(greatest, mapping[i]);
	n++;
      }
  
  n *= 3 + (size_t)(greatest > 0x00FF ? 5 : 3);
  
  if (ensure_send_buffer_size(top + n + 2) < 0)
    return -1;
  
  with_mutex (send_mutex,
	      msgid = message_id;
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      );
  
  off = top + 1;
  for (i = 0; i < mapping_size; i++)
    if (mapping[i] != (int)i)
      {
	sprintf(send_buffer + off, "%i %i\n", i, mapping[i]);
	off += strlen(send_buffer + off);
      }
  
  n = (size_t)(off - (top + 1));
  
  sprintf(send_buffer,
	  "To: %s\n"
	  "In response to: %s\n"
	  "Message ID: %" PRIu32 "\n"
	  "Length: %zu\n"
	  "\n",
	  recv_client_id, recv_message_id, msgid, n);
  
  off = top + 1;
  off -= top = strlen(send_buffer);
  memmove(send_buffer + off, send_buffer, top * sizeof(char));
  
  with_mutex (send_mutex,
	      r = full_send(send_buffer + off, top + n);
	      );
  
  return r;
}


/**
 * Handle the received message after it has been
 * identified to contain `Command: keycode-map`
 * 
 * @param   recv_client_id   The value of the `Client ID`-header, "0:0" if omitted
 * @param   recv_message_id  The value of the `Message ID`-header
 * @param   recv_action      The value of the `Action`-header, `NULL` if omitted
 * @param   recv_keyboard    The value of the `Keyboard`-header, `NULL` if omitted
 * @return                   Zero on success, -1 on error
 */
int handle_keycode_map(const char* recv_client_id, const char* recv_message_id,
		       const char* recv_action, const char* recv_keyboard)
{
  int r = 0;
  
  if ((recv_keyboard != NULL) && !strequals(recv_keyboard, KEYBOARD_ID))
    return 0;
  
  if (recv_action == NULL)
    eprint("received keycode map request but without any action, ignoring.");
  else if (strequals(recv_action, "remap"))
    {
      if (received.payload_size == 0)
	{
	  eprint("received keycode remap request without a payload, ignoring.");
	  return 0;
	}
      
      with_mutex (mapping_mutex,
		  r = remap(received.payload, received.payload_size);
		  );
    }
  else if (strequals(recv_action, "reset"))
    {
      with_mutex (mapping_mutex,
		  free(mapping);
		  mapping_size = 0;
		  );
    }
  else if (strequals(recv_action, "query"))
    {
      if (strequals(recv_client_id, "0:0"))
	{
	  eprint("received information request from an anonymous client, ignoring.");
	  return 0;
	}
      
      r = mapping_query(recv_client_id, recv_message_id);
    }
  else
    eprint("received keycode map request with invalid action, ignoring.");
  
  return r;
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
  
  if (kbd_thread_started)
    if (pthread_equal(current_thread, kbd_thread) == 0)
      pthread_kill(kbd_thread, signo);
}


/**
 * Send a full message even if interrupted
 * 
 * @param   message  The message to send
 * @param   length   The length of the message
 * @return           Non-zero on success
 */
int full_send(const char* message, size_t length)
{
  size_t sent;
  
  while (length > 0)
    {
      sent = send_message(socket_fd, message, length);
      if (sent > length)
	{
	  eprint("Sent more of a message than exists in the message, aborting.");
	  return -1;
	}
      else if ((sent < length) && (errno != EINTR))
	{
	  xperror(*argv);
	  return -1;
	}
      message += sent;
      length -= sent;
    }
  return 0;
}


/**
 * Acquire access of the keyboard's LED:s
 * 
 * @return  Zero on success, -1 on error
 */
int open_leds(void)
{
#ifdef __sparc__
  if ((ledfd = open(SPARC_KBD, O_RDONLY)) < 0)
    return -1;
  if (ioctl(ledfd, GET_LED, &saved_leds) < 0)
    {
      close(ledfd);
      return -1;
    }
  return 0;
#else
  return ioctl(ledfd, GET_LED, &saved_leds);
#endif
}


/**
 * Release access of the keyboard's LED:s
 */
void close_leds(void)
{
  if (ioctl(ledfd, SET_LED, saved_leds) < 0)
    xperror(*argv);
#ifdef __sparc__
  close(ledfd);
#endif
}


/**
 * Get active LED:s on the keyboard
 * 
 * @return  Active LED:s, -1 on error
 */
int get_leds(void)
{
  int leds;
  if (ioctl(ledfd, GET_LED, &leds) < 0)
    return -1;
#ifdef __sparc__
  leds &= 15;
#endif
  return leds;
}


/**
 * Set active LED:s on the keyboard
 * 
 * @param   leds  Active LED:s
 * @return        Zero on success, -1 on error
 */
int set_leds(int leds)
{
  return ioctl(ledfd, SET_LED, leds);
}


/**
 * Acquire access of keyboard input
 * 
 * @return  Zero on success, -1 on error
 */
int open_input(void)
{
  struct termios stty;
  if (tcgetattr(STDIN_FILENO, &saved_stty) < 0)
    return -1;
  stty = saved_stty;
  stty.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG);
  stty.c_iflag = 0;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &stty) < 0)
    return -1;
  /* K_MEDIUMRAW: utilise keyboard drivers, but not layout */
  if ((ioctl(STDIN_FILENO, KDGKBMODE, &saved_kbd_mode) < 0) ||
      (ioctl(STDIN_FILENO, KDSKBMODE, K_MEDIUMRAW) < 0))
    {
      xperror(*argv);
      return tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty);
    }
  return 0;
}


/**
 * Release access of keyboard input
 */
void close_input(void)
{
  if (ioctl(STDIN_FILENO, KDSKBMODE, saved_kbd_mode) < 0)
    xperror(*argv);
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_stty) < 0)
    xperror(*argv);
}


/**
 * Broadcast a keyboard input event
 * 
 * @param   scancode  The scancode
 * @param   trio      Whether the scancode has three integers rather than one
 * @return            Zero on success, -1 on error
 */
int send_key(int* restrict scancode, int trio)
{
  int r, keycode, released = (scancode[0] & 0x80) == 0x80;
  uint32_t msgid;
  scancode[0] &= 0x7F;
  if (trio)
    {
      keycode = (scancode[1] &= 0x7F) << 7;
      keycode |= (scancode[2] &= 0x7F);
    }
  else
    keycode = scancode[0];
  
  with_mutex (mapping_mutex,
	      if ((size_t)keycode < mapping_size)
		keycode = mapping[keycode];
	      );
  
  with_mutex (send_mutex,
	      msgid = message_id;
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      );
  
  if (trio)
    sprintf(key_send_buffer,
	    "Command: key-sent\n"
	    "Scancode: %i %i %i\n"
	    "Keycode: %i\n"
	    "Released: %s\n"
	    "Keyboard: " KEYBOARD_ID "\n"
	    "Message ID: %" PRIu32 "\n"
	    "\n",
	    scancode[0], scancode[1], scancode[2], keycode,
	    released ? "yes" : "no", msgid);
  else
    sprintf(key_send_buffer,
	    "Command: key-sent\n"
	    "Scancode: %i\n"
	    "Keycode: %i\n"
	    "Released: %s\n"
	    "Keyboard: " KEYBOARD_ID "\n"
	    "Message ID: %" PRIu32 "\n"
	    "\n",
	    scancode[0], keycode,
	    released ? "yes" : "no", msgid);
  
  with_mutex (send_mutex,
	      r = full_send(key_send_buffer, strlen(key_send_buffer));
	      );
  return r;
}


/**
 * Fetch and broadcast keys until interrupted
 * 
 * @return  Zero on success, -1 on error
 */
int fetch_keys(void)
{
#ifdef DEBUG
  int consecutive_escapes = 0;
#endif
  int c;
  ssize_t r;
  
  for (;;)
    {
      r = read(STDIN_FILENO, &c, sizeof(int));
      if (r <= 0)
	{
	  if (r == 0)
	    {
	      raise(SIGTERM);
	      errno = 0;
	    }
	  break;
	}
      
#ifdef DEBUG
      if ((c & 0x7F) == 1) /* Exit with ESCAPE, ESCAPE, ESCAPE */
	{
	  if (++consecutive_escapes >= 2 * 3)
	    {
	      raise(SIGTERM);
	      errno = 0;
	      break;
	    }
	}
      else
	consecutive_escapes = 0;
#endif
      
    redo:
      scancode_buf[scancode_ptr] = c;
      if (scancode_ptr == 0)
	{
	  if ((c & 0x7F) == 0)
	    scancode_ptr++;
	  else
	    send_key(scancode_buf, 0);
	}
      else if (scancode_ptr == 1)
	{
	  if ((c & 0x80) == 0)
	    {
	      scancode_ptr = 0;
	      goto redo;
	    }
	  scancode_ptr++;
	}
      else
	{
	  scancode_ptr = 0;
	  if ((c & 0x80) == 0)
	    {
	      send_key(scancode_buf + 1, 0);
	      goto redo;
	    }
	  send_key(scancode_buf, 1);
	}
    }
  
  return errno == 0 ? 0 : -1;
}


/**
 * Send a response with an error number
 * 
 * @param   error            The error number
 * @param   recv_client_id   The client's ID
 * @param   recv_message_id  The message ID of the message the client sent
 * @return                   Zero on success, -1 on error
 */
int send_errno(int error, const char* recv_client_id, const char* recv_message_id)
{
  size_t n = 79 + strlen(recv_client_id) + strlen(recv_message_id) + 3 * sizeof(int);
  int r;
  
  if (ensure_send_buffer_size(n + 1) < 0)
    return -1;
  
  with_mutex (send_mutex,
	      sprintf(send_buffer,
		      "Command: error\n"
		      "To: %s\n"
		      "In response to: %s\n"
		      "Message ID: %" PRIu32 "\n"
		      "Error: %i\n"
		      "\n",
		      recv_client_id, recv_message_id, message_id, error);
	      
	      message_id = message_id == INT32_MAX ? 0 : (message_id + 1);
	      r = full_send(send_buffer, strlen(send_buffer));
	      );
  return r;
}


/**
 * Attempt to shrink `mapping`
 */
void shrink_map(void)
{
  size_t i, greatest_mapping = 0;
  int* old;
  
  for (i = mapping_size; i > 0; i--)
    if (mapping[i] != (int)i)
      {
	greatest_mapping = i;
	break;
      }
  
  if (greatest_mapping == 0)
    {
      if (mapping[0] == 0)
	{
	  free(mapping);
	  mapping_size = 0;
	}
    }
  else if (greatest_mapping + 1 < mapping_size)
    {
      if (old = mapping, xrealloc(mapping, greatest_mapping + 1, int))
	{
	  mapping = old;
	  xperror(*argv);
	}
      else
	mapping_size = greatest_mapping + 1;
    }
}

