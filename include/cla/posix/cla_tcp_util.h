#ifndef CLA_TCP_UTIL_H_INCLUDED
#define CLA_TCP_UTIL_H_INCLUDED

#include <netinet/in.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * Convert a sockaddr structure to a sanitized CLA address.
 *
 * @param sockaddr A pointer to the struct sockaddr instance to be converted.
 * @param sockaddr_len The length (size) of the provided data type.
 * @return The sanitized CLA address as null-terminated string, NULL on error.
 */
char *cla_tcp_sockaddr_to_cla_addr(struct sockaddr *sockaddr,
				   socklen_t sockaddr_len);

/**
 * Create a new TCP socket and connect to or listen on the specified combination
 * of node and service name.
 *
 * @param node The node name, e.g. an IP address. If "*" is specified, the
 *             socket is bound to all interfaces.
 * @param service The service name, e.g. a TCP port number.
 * @param client If true, try to connect(). If false, bind() and listen().
 * @param addr_return If not NULL, the sanitized CLA address will be returned
 *                    as newly allocated string.
 * @return A TCP socket, or -1 on error.
 */
int create_tcp_socket(const char *const node, const char *const service,
		      const bool client, char **const addr_return);

/**
 * Create a new TCP socket and connect to the specified CLA address.
 *
 * @param cla_addr The CLA address, i.e. a combination of node and service name.
 * @param default_service The default service (port), if nothing is specified.
 * @return A TCP socket, or -1 on error.
 */
int cla_tcp_connect_to_cla_addr(const char *const cla_addr,
				const char *const default_service);

/**
 * Send all data to the given socket, ignoring interruptions by signals.
 *
 * @param socket The socket to be written to.
 * @param buffer The buffer from which data should be read.
 * @param length The amount of data to be written.
 * @return The return value is compatible to send(3).
 *         errno might be set accordingly.
 */
ssize_t tcp_send_all(const int socket, const void *const buffer,
		     const size_t length);

/**
 * Receive all data from the given socket, ignoring interruptions by signals.
 *
 * @param socket The socket to be read from.
 * @param buffer The buffer into which data should be read.
 * @param length The amount of data to be read.
 * @return The return value is compatible to recv(3).
 *         errno might be set accordingly.
 */
ssize_t tcp_recv_all(const int socket, void *const buffer, const size_t length);

#endif // CLA_TCP_UTIL_H_INCLUDED
