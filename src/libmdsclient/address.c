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
#include "address.h"

#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>

#include <libmdsserver/config.h>



/**
 * Check whether a string is a radix-10 non-negative integer
 * 
 * @param   str  The string
 * @return       Whether a string is a radix-10 non-negative integer
 */
static int __attribute__((nonnull))
is_pzinteger(const char *restrict str)
{
	for (; *str; str++)
		if ('0' > *str || *str > '9')
			return 0;
	return 1;
}


/**
 * Set the socket address, with the address family `AF_UNIX`
 * 
 * @param   out_address  Output parameter for the socket address
 * @param   pathlen      Pointer to a variable where the length of the pathname will be stored
 * @param   format       Formatting string for the pathname (should end with "%zn")
 * @param   ...          Formatting arguments for the pathname (should end with `pathlen`)
 * @return               Zero on success, -1 on error, `errno`
 *                       will have been set accordinly on error
 * 
 * @throws  ENOMEM        Out of memory. Possibly, the application hit the
 *                        RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 */
static int __attribute__((nonnull, format(gnu_printf, 3, 4)))
set_af_unix(struct sockaddr **restrict out_address, ssize_t *pathlen, const char *format, ...)
#define set_af_unix(a, f, ...) ((set_af_unix)(a, &pathlen, f "%zn", __VA_ARGS__, &pathlen))
{
	struct sockaddr_un *address;
	size_t maxlen;
	va_list args;

	address = malloc(sizeof(struct sockaddr_un));
	*out_address = (struct sockaddr*)address;
	if (!address)
		return -1;

	maxlen = sizeof(address->sun_path) / sizeof(address->sun_path[0]);

	va_start(args, format);

	address->sun_family = AF_UNIX;
	vsnprintf(address->sun_path, maxlen, format, args);

	va_end(args);

	if ((size_t)*pathlen > maxlen)
		return errno = ENAMETOOLONG, -1;

	return 0;
}


/**
 * Set the socket address, with either of the address
 * families `AF_INET` and `AF_INET6`
 * 
 * @param   out_address      Output parameter for the socket address
 * @param   out_address_len  Output parameter for the socket address's allocation size
 * @param   out_gai_error    Output parameter for error at `getaddrinfo`
 * @param   out_domain       Output parameter for the protocol famility,
 *                           unused unless the address familiy is unknown
 * @param   address_family   The address family, `AF_UNSPEC` if unknown
 * @param   host             The host
 * @param   port             The address
 * @return                   Zero on success, -1 on error, `errno` will have been
 *                           set accordinly on error, `*out_gai_error` may be set
 *                           on error and zero returned
 * 
 * @throws  ENOMEM  Out of memory. Possibly, the application hit the
 *                  RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 */
static int __attribute__((nonnull))
set_af_inet(struct sockaddr **restrict out_address, socklen_t *restrict out_address_len,
            int *restrict out_gai_error, int *restrict out_domain, int address_family,
            const char *restrict host, const char *restrict port)
{
	struct addrinfo hints;
	struct addrinfo *result;
	int saved_errno;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = address_family;

	*out_gai_error = getaddrinfo(host, port, &hints, &result);
	if (*out_gai_error)
		return 0;

	*out_address = malloc(result->ai_addrlen);
	if (!*out_address)
		return saved_errno = errno, freeaddrinfo(result), errno = saved_errno, -1;
	memcpy(*out_address, result->ai_addr, result->ai_addrlen);

	*out_address_len = result->ai_addrlen;
	*out_domain =
		result->ai_family == AF_UNSPEC ? PF_UNSPEC :
		result->ai_family == AF_INET   ? PF_INET   :
		result->ai_family == AF_INET6  ? PF_INET6  :
		result->ai_family;

	freeaddrinfo(result);
	return 0;
}


/**
 * Parse a display address string
 * 
 * @param   display  The address in MDS_DISPLAY-formatting, must not be `NULL`
 * @param   address  Output parameter for parsed address, must not be `NULL`
 * @return           Zero on success, even if parsing failed, -1 on error,
 *                   `errno` will have been set accordinly on error
 * 
 * @throws  ENOMEM        Out of memory. Possibly, the application hit the
 *                        RLIMIT_AS or RLIMIT_DATA limit described in getrlimit(2).
 * @throws  ENAMETOOLONG  The filename of the target socket is too long
 */
int
libmds_parse_display_address(const char *restrict display, libmds_display_address_t *restrict address)
{
	ssize_t pathlen = 0;
	char *host;
	char *port;
	char *params;
	size_t i = 0;
	char c;
	int esc = 0;
	int colesc = 0;

	address->domain = -1;
	address->type = -1;
	address->protocol = -1;
	address->address = NULL;
	address->address_len = 0;
	address->gai_error = 0;

	if (!strchr(display, ':'))
		return 0;

	if (*display == ':') {
		address->domain = PF_UNIX;
		address->type = SOCK_STREAM;
		address->protocol = 0;
		address->address_len = sizeof(struct sockaddr_un);

		if (strstr(display, ":file:") == display)
			return set_af_unix(&(address->address), "%s", display + 6);
		else if (display[1] && is_pzinteger(display + 1))
			return set_af_unix(&(address->address), "%s/%s.socket",
			                   MDS_RUNTIME_ROOT_DIRECTORY, display + 1);
		return 0;
	}

	host = alloca((strlen(display) + 1) * sizeof(char));

	if (*display == '[')
		colesc = 1, i++;
	for (; (c = display[i]); i++) {
		if (esc) {
			host[pathlen++] = c;
			esc = 0;
		} else if (c == '\\') {
			esc = 1;
		} else if (c == ']') {
			if (colesc)
				{ i++; break; }
			else
				host[pathlen++] = c;
		} else if (c == ':') {
			if (colesc)
				host[pathlen++] = c;
			else
				break;
		} else {
			host[pathlen++] = c;
		}
	}
	if (esc || display[i++] != ':')
		return 0;
	host[pathlen++] = '\0';

	port = host + pathlen;
	memcpy(port, display + i, (strlen(display) + 1 - i) * sizeof(char));
	params = strchr(port, ':');
	if (params)
		*params++ = '\0';

#define param_test(f, n, a, b, c)\
	((n >= 3 && !strcasecmp(params, a ":" b ":" c)) ||\
	 (n >= 2 && !strcasecmp(params, a ":" b)) ||\
	 (n >= 1 && !strcasecmp(params, a)) ||\
	 (!f ? 0 : !strcasecmp(params, f)))
#define set(d, t, p)\
	(address->domain = (d), address->type = (t), address->protocol = (p))

	if      (!params)                                              set(PF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);
	else if (param_test("ip/tcp",    1, "ip",    "stream", "tcp")) set(PF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);
	else if (param_test("ipv4/tcp",  1, "ipv4",  "stream", "tcp")) set(PF_INET,   SOCK_STREAM, IPPROTO_TCP);
	else if (param_test("ipv6/tcp",  1, "ipv6",  "stream", "tcp")) set(PF_INET6,  SOCK_STREAM, IPPROTO_TCP);
	else if (param_test("inet/tcp",  1, "inet",  "stream", "tcp")) set(PF_INET,   SOCK_STREAM, IPPROTO_TCP);
	else if (param_test("inet6/tcp", 1, "inet6", "stream", "tcp")) set(PF_INET6,  SOCK_STREAM, IPPROTO_TCP);
	else
		return 0;

#undef set
#undef param_test

	return set_af_inet(&(address->address), &(address->address_len),
	                   &(address->gai_error), &(address->domain),
	                   address->domain == PF_UNSPEC ? AF_UNSPEC :
	                   address->domain == PF_INET   ? AF_INET   :
	                   address->domain == PF_INET6  ? AF_INET6  :
	                   address->domain, host, port);
}
