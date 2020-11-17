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
		LOGF("TCP: getnameinfo failed: %s\n", gai_strerror(status));
		return NULL;
	}

	if (sockaddr->sa_family != AF_INET && sockaddr->sa_family != AF_INET6) {
		LOGF("TCP: getnameinfo returned invalid AF: %hu\n",
		     sockaddr->sa_family);
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

	const char *node_param = node;

	ASSERT(node_param != NULL);
	ASSERT(service != NULL);
	// We support specifying "*" as node name to bind to all interfaces.
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
	hints.ai_flags |= AI_PASSIVE; // support NULL as host name -> any if

	status = getaddrinfo(node_param, service, &hints, &result);
	if (status != 0) {
		LOGF(
			"TCP: getaddrinfo() failed for %s:%s: %s",
			node,
			service,
			gai_strerror(status)
		);
		return -1;
	}

	// Default behavior when using getaddrinfo: try one after another
	for (e = result; e != NULL; e = e->ai_next) {
		error_code = 0;
		sock = socket(
			e->ai_family,
			e->ai_socktype,
			e->ai_protocol
		);

		if (sock == -1) {
			error_code = errno;
			continue;
		}

		// Enable the immediate reuse of a previously closed socket.
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			close(sock);
			continue;
		}

#ifdef SO_REUSEPORT
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			close(sock);
			continue;
		}
#endif // SO_REUSEPORT

		// Disable the nagle algorithm to prevent delays in responses.
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
			       &enable, sizeof(int)) < 0) {
			error_code = errno;
			close(sock);
			continue;
		}

		if (client && connect(sock, e->ai_addr, e->ai_addrlen) < 0) {
			error_code = errno;
			close(sock);
			continue;
		} else if (!client &&
			   bind(sock, e->ai_addr, e->ai_addrlen) < 0) {
			error_code = errno;
			close(sock);
			continue;
		}

		// We have our socket!
		break;
	}

	if (e == NULL) {
		LOGF(
			"TCP: Failed to %s to [%s]:%s: %s",
			client ? "connect" : "bind",
			node,
			service,
			error_code ? strerror(error_code) : "<unknown>"
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
