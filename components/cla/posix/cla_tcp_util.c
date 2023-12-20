// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "cla/posix/cla_tcp_util.h"

#include "platform/hal_io.h"

#include "ud3tn/common.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif // NI_MAXHOST

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif // NI_MAXSERV

char *cla_tcp_sockaddr_to_cla_addr(struct sockaddr *const sockaddr,
				   const socklen_t sockaddr_len)
{
	char host_tmp[NI_MAXHOST];
	char service_tmp[NI_MAXSERV];

	int status = getnameinfo(
		sockaddr,
		sockaddr_len,
		host_tmp,
		NI_MAXHOST,
		service_tmp,
		NI_MAXSERV,
		NI_NUMERICHOST | NI_NUMERICSERV
	);

	if (status != 0) {
		LOGF_WARN(
			"TCP: getnameinfo failed: %s\n",
			gai_strerror(status)
		);
		return NULL;
	}

	if (sockaddr->sa_family != AF_INET && sockaddr->sa_family != AF_INET6) {
		LOGF_WARN(
			"TCP: getnameinfo returned invalid AF: %hu\n",
			sockaddr->sa_family
		);
		return NULL;
	}

	const size_t host_len = strlen(host_tmp);
	const size_t service_len = strlen(service_tmp);
	const size_t result_len = (
		host_len +
		service_len +
		1 + // ':'
		1 + // '\0'
		(sockaddr->sa_family == AF_INET6 ? 2 : 0) // "[...]:..."
	);
	char *const result = malloc(result_len);

	snprintf(
		result,
		result_len,
		(sockaddr->sa_family == AF_INET6 ? "[%s]:%s" : "%s:%s"),
		host_tmp,
		service_tmp
	);
	return result;
}

int create_tcp_socket(const char *const node, const char *const service,
		      const bool client, char **const addr_return)
{
	const int enable = 1;
	const int disable = 0;

	const char *node_param = node;

	ASSERT(node_param != NULL);
	ASSERT(service != NULL);
	// We support specifying "*" as node name to bind to all interfaces.
	// Note that by default this only uses IPv4. To support IPv4 and v6
	// at the same time, "::" should be specified instead.
	if (strcmp(node_param, "*") == 0)
		node_param = NULL;

	struct addrinfo hints;
	struct addrinfo *result, *e;
	int sock = -1;
	int status;
	int error_code = 0;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; // support IPv4 + v6
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_V4MAPPED; // enable IPv4 support via mapped addr
	if (!client)
		hints.ai_flags |= AI_PASSIVE; // node == NULL -> any interface

	status = getaddrinfo(node_param, service, &hints, &result);
	if (status != 0) {
		LOGF_WARN(
			"TCP: getaddrinfo() failed for %s:%s: %s",
			node,
			service,
			gai_strerror(status)
		);
		return -1;
	}

	// Default behavior when using getaddrinfo: try one after another
	for (e = result; e != NULL; e = e->ai_next) {
		sock = socket(
			e->ai_family,
			e->ai_socktype,
			e->ai_protocol
		);

		if (sock == -1) {
			error_code = errno;
			LOG_ERRNO("TCP", "socket()", error_code);
			continue;
		}

		// Enable the immediate reuse of a previously closed socket.
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			LOG_ERRNO(
				"TCP",
				"setsockopt(SO_REUSEADDR, 1)",
				error_code
			);
			close(sock);
			continue;
		}

#if defined(CLA_TCP_ALLOW_REUSE_PORT) && CLA_TCP_ALLOW_REUSE_PORT == 1
		// NOTE: SO_REUSEPORT is Linux- and BSD-specific.
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			LOG_ERRNO(
				"TCP",
				"setsockopt(SO_REUSEPORT, 1)",
				error_code
			);
			close(sock);
			continue;
		}
#endif // CLA_TCP_ALLOW_REUSE_PORT

		if (e->ai_family == AF_INET6) {
			// Some systems may want to only listen for IPv6
			// connections by default.
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
			    &disable, sizeof(int)) < 0) {
				error_code = errno;
				LOG_ERRNO(
					"TCP",
					"setsockopt(IPV6_V6ONLY, 0)",
					 error_code
				);
				close(sock);
				continue;
			}
		}

		// Disable the nagle algorithm to prevent delays in responses.
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			LOG_ERRNO(
				"TCP",
				"setsockopt(TCP_NODELAY, 1)",
				error_code
			);
			close(sock);
			continue;
		}

		if (client && connect(sock, e->ai_addr, e->ai_addrlen) < 0) {
			error_code = errno;
			LOG_ERRNO_INFO("TCP", "connect()", error_code);
			close(sock);
			continue;
		} else if (!client &&
			   bind(sock, e->ai_addr, e->ai_addrlen) < 0) {
			error_code = errno;
			LOG_ERRNO_INFO("TCP", "bind()", error_code);
			close(sock);
			continue;
		}

		// We have our socket!
		break;
	}

	if (e == NULL) {
		LOGF_WARN(
			"TCP: Failed to %s to [%s]:%s",
			client ? "connect" : "bind",
			node,
			service
		);
		if (sock != -1)
			close(sock);
		sock = -1;
		goto done;
	}

	if (addr_return) {
		*addr_return = cla_tcp_sockaddr_to_cla_addr(
			e->ai_addr,
			e->ai_addrlen
		);

		if (!*addr_return) {
			close(sock);
			sock = -1;
			goto done;
		}
	}

done:
	// Free the list allocated by getaddrinfo.
	freeaddrinfo(result);
	return sock;
}

int cla_tcp_connect_to_cla_addr(const char *const cla_addr,
				const char *const default_service)
{
	ASSERT(cla_addr != NULL && cla_addr[0] != 0);

	char *const addr = strdup(cla_addr);
	char *node, *service;

	if (!addr)
		return -1;
	// Split CLA addr into node and service names
	if (addr[0] == '[') {
		// IPv6 port notation
		service = strrchr(addr, ']');
		if (!service || service[1] != ':' || service[2] == 0) {
			if (!default_service) {
				LOG_WARN("TCP: Service field empty and no default service/port specified, cannot connect");
				free(addr);
				return -1;
			}
			// no port / service given
			if (service)
				service[0] = 0; // zero-terminate node string
			service = (char *)default_service; // use default port
		} else {
			service[0] = 0;
			service = &service[2];
		}
		node = &addr[1];
	} else {
		service = strrchr(addr, ':');
		if (!service || service[1] == 0) {
			if (!default_service) {
				LOG_WARN("TCP: Service field empty and no default service/port specified, cannot connect");
				free(addr);
				return -1;
			}
			// no port / service given
			if (service)
				service[0] = 0; // zero-terminate node string
			service = (char *)default_service; // use default port
		} else {
			service[0] = 0;
			service = &service[1];
		}
		node = addr;
	}

	const int socket = create_tcp_socket(node, service, true, NULL);

	free(addr);

	return socket;
}

ssize_t tcp_send_all(const int socket, const void *const buffer,
		     const size_t length)
{
	size_t sent = 0;

	while (sent < length) {
		const ssize_t r = send(
			socket,
			buffer,
			length - sent,
			0
		);

		if (r == 0)
			return r;
		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
					errno == EINTR)
				continue;
			return r;
		}

		sent += r;
	}

	return sent;
}

ssize_t tcp_recv_all(const int socket, void *const buffer, const size_t length)
{
	size_t recvd = 0;

	while (recvd < length) {
		const ssize_t r = recv(
			socket,
			buffer,
			length - recvd,
			MSG_WAITALL
		);

		if (r == 0)
			return r;
		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
					errno == EINTR)
				continue;
			return r;
		}

		recvd += r;
	}

	return recvd;
}

void tcp_write_to_socket(
	void *const p, const void *const buffer, const size_t length)
{
	struct tcp_write_to_socket_param *const wsp = p;
	ssize_t result;

	if (wsp->errno_)
		return;
	result = tcp_send_all(wsp->socket_fd, buffer, length);
	if (result == -1)
		wsp->errno_ = errno;
}
