// SPDX-License-Identifier: BSD-3-Clause OR Apache-2.0
#include "platform/hal_io.h"

#include "ud3tn/common.h"

#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdio.h>

int create_unix_domain_socket(const char *path)
{
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock == -1) {
		LOG_ERRNO("Socket Util", "socket(AF_UNIX)", errno);
		return -1;
	}

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	const int rv1 = snprintf(
		addr.sun_path,
		sizeof(addr.sun_path),
		"%s",
		path
	);

	if (rv1 <= 0 || (unsigned int)rv1 > sizeof(addr.sun_path)) {
		LOGF_ERROR(
			"Socket Util: Invalid socket path, len = %d, maxlen = %zu",
			rv1,
			sizeof(addr.sun_path)
		);
		return -1;
	}

	unlink(path);
	int rv2 = bind(
		sock,
		(const struct sockaddr *)&addr,
		sizeof(struct sockaddr_un)
	);
	if (rv2 == -1) {
		LOG_ERRNO("Socket Util", "bind(unix_domain_socket)", errno);
		return -1;
	}

	return sock;
}

int poll_recv_timeout(const int socket_fd, const int timeout)
{
	struct pollfd pollfd[1];

	pollfd[0].events = POLLIN;
	pollfd[0].fd = socket_fd;

	for (;;) {
		if (poll(pollfd, ARRAY_LENGTH(pollfd), timeout) == -1) {
			const int err = errno;

			// Try again if interrupted by a signal.
			if (err == EINTR)
				continue;

			LOG_ERRNO("Socket Util", "poll()", err);
			return -1;
		}
		if (pollfd[0].revents & POLLERR) {
			LOG_WARN("Socket Util: Socket error (e.g. TCP RST) detected.");
			return -1;
		}
		if (pollfd[0].revents & POLLHUP) {
			LOG_INFO("Socket Util: The peer closed the connection.");
			return -1;
		}
		if (pollfd[0].revents & POLLNVAL) {
			LOG_INFO("Socket Util: Connection was not open.");
			return -1;
		}
		if (pollfd[0].revents & POLLIN)
			return 1;
		return 0;
	}
}
