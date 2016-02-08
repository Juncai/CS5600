#include "master.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#define MAXEVEDNTS 1000
void computeExponential(char *worker_path, char *mechanism, int x, int n);

int main(int argc, char *argv[])
{
	if (argc != 9) {
		printf("Usage: --worker_path WORKER_PATH --wait_mechanism MECHANISM -x X -n N\n");
		return -1;
	}

	char *worker_path = argv[2];
	char *mechanism = argv[4];
	int x = atoi(argv[6]);
	int n = atoi(argv[8]);

	computeExponential(worker_path, mechanism, x, n);
	return 0;
}

void computeExponential(char *worker_path, char *mechanism, int x, int n)
{
	int i;
	int status = 0;
	char x_str[50];
	char n_str[50];
	int cid = -1;
	char i_str[50];
	char w_str[50];
	int pipefds[2];
	char buf;
	int exec_res;
	char *argv[6];

	sprintf(x_str, "%d", x);
	sprintf(n_str, "%d", n);

	argv[0] = "./worker";
	argv[1] = "-x";
	argv[2] = x_str;
	argv[3] = "-n";
	argv[4] = i_str;
	argv[5] = (char *) NULL;

	if (strcmp(mechanism, "sequential") == 0) {
		for (i = 0; i <= n; i++) {
			sprintf(i_str, "%d", i);
			pipe(pipefds);
			cid = fork();
			if (cid == 0) {
				close(pipefds[0]);
				dup2(pipefds[1], STDOUT_FILENO);
				exec_res = execv(worker_path, argv);
				if (exec_res == -1) perror("exec failed");
				_exit(EXIT_FAILURE);
			} else {
				waitpid(cid, &status, 0);
				close(pipefds[1]);
				sprintf(w_str, "worker %d: ", i);
				write(STDOUT_FILENO, w_str, strlen(w_str));
				while (read(pipefds[0], &buf, 1) > 0)
                   write(STDOUT_FILENO, &buf, 1);
				close(pipefds[0]);
			}
		}
	}

	if (strcmp(mechanism, "select") == 0) {
		// create a list of pipe read-ends
		fd_set rfds;
		int read_ends[n + 1];
		int write_ends[n + 1];
		int max_fd = 0;

		FD_ZERO(&rfds);
		
		for (i = 0; i <= n; i++) {
			pipe(pipefds);
			FD_SET(pipefds[0], &rfds);
			read_ends[i] = pipefds[0];
			write_ends[i] = pipefds[1];
			if (pipefds[0] > max_fd) {
				max_fd = pipefds[0];
			}
		}

		for (i = 0; i <= n; i++) {
			sprintf(i_str, "%d", i);
			cid = fork();
			if (cid == 0) {
				close(read_ends[i]);
				dup2(write_ends[i], STDOUT_FILENO);
				exec_res = execv(worker_path, argv);
				if (exec_res == -1) perror("exec failed");
				_exit(EXIT_FAILURE);
			} else {
				close(write_ends[i]);
			}
		}

		struct timeval tv;
		int retval;
		int counter = n + 1;

		while (counter > 0) {
		/* while (1) { */
			// reset the rfds
			FD_ZERO(&rfds);
			for (i = 0; i <= n; i++) {
				if (read_ends[i] != 0) {
					FD_SET(read_ends[i], &rfds);
				}
			}
			// reset the timer
			// wait for half a second
			tv.tv_sec = 0;
			tv.tv_usec = 500000;
			retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);

			if (retval == -1) {
				perror("select");
				return;
			} else if (retval) {
				for (i = 0; i <= n; i++) {
					if (FD_ISSET(read_ends[i], &rfds)) {
						sprintf(w_str, "worker %d: ", i);
						write(STDOUT_FILENO, w_str, strlen(w_str));
						while (read(read_ends[i], &buf, 1) > 0)
						    write(STDOUT_FILENO, &buf, 1);
						close(read_ends[i]);
						read_ends[i] = 0;
						counter--;
					}
				}
			}
		}
	}

	if (strcmp(mechanism, "poll") == 0) {
		// create a list of pipe read-ends
		struct pollfd pfds[n + 1];
		int read_ends[n + 1];
		int write_ends[n + 1];
		int max_fd = 0;
		int timeout = 500;  // in milisecond
		
		for (i = 0; i <= n; i++) {
			pipe(pipefds);
			pfds[i].fd = pipefds[0];
			pfds[i].events = POLLIN;
			read_ends[i] = pipefds[0];
			write_ends[i] = pipefds[1];
			if (pipefds[0] > max_fd) {
				max_fd = pipefds[0];
			}
		}

		for (i = 0; i <= n; i++) {
			sprintf(i_str, "%d", i);
			cid = fork();
			if (cid == 0) {
				close(read_ends[i]);
				dup2(write_ends[i], STDOUT_FILENO);
				exec_res = execv(worker_path, argv);
				if (exec_res == -1) perror("exec failed");
				_exit(EXIT_FAILURE);
			} else {
				close(write_ends[i]);
			}
		}

		int retval;
		int counter = n + 1;

		// wait for half a second
		while (counter > 0) {
			retval = poll(pfds, n + 1, timeout);

			if (retval == -1) {
				perror("poll");
				return;
			} else if (retval) {
				for (i = 0; i <= n; i++) {
					if (pfds[i].revents & POLLIN) {
						sprintf(w_str, "worker %d: ", i);
						write(STDOUT_FILENO, w_str, strlen(w_str));
						while (read(read_ends[i], &buf, 1) > 0)
						    write(STDOUT_FILENO, &buf, 1);
						close(read_ends[i]);
						read_ends[i] = 0;
						counter--;
					}
				}
			}
		}
	}

	if (strcmp(mechanism, "epoll") == 0) {
		// create a list of pipe read-ends
		struct epoll_event ev, events[MAXEVEDNTS];
		int epollfd;
		int read_end, write_end;
		int read_ends[n + 1];
		int write_ends[n + 1];
		int max_fd = 0;
		int retval;
		int counter = n + 1;
		
		// create epoll instance
		epollfd = epoll_create1(0);
		if (epollfd == -1) {
			perror("epoll");
			exit(EXIT_FAILURE);
		}

		for (i = 0; i <= n; i++) {
			pipe(pipefds);
			read_end = pipefds[0];
			write_end = pipefds[1];
			// add event to epoll
			ev.data.fd = read_end;
			ev.events = EPOLLIN | EPOLLET;
			retval = epoll_ctl(epollfd, EPOLL_CTL_ADD, read_end, &ev);
			if (retval == -1) {
				perror ("epoll_ctl");
				exit(EXIT_FAILURE);
			}
			// store the read and write end fds
			read_ends[i] = read_end;
			write_ends[i] = write_end;

			if (pipefds[0] > max_fd) {
				max_fd = pipefds[0];
			}
		}

		// create threads for executing worker program
		for (i = 0; i <= n; i++) {
			sprintf(i_str, "%d", i);
			cid = fork();
			if (cid == 0) {
				close(read_ends[i]);
				dup2(write_ends[i], STDOUT_FILENO);
				exec_res = execv(worker_path, argv);
				if (exec_res == -1) perror("exec failed");
				_exit(EXIT_FAILURE);
			} else {
				close(write_ends[i]);
			}
		}

		// wait for half a second
		while (counter > 0) {
			int n_eve, i_eve;
			n_eve = epoll_wait(epollfd, events, MAXEVEDNTS, -1);
			for (i_eve = 0; i_eve < n_eve; i_eve++) {
				read_end = events[i_eve].data.fd;
				for (i = 0; i < n + 1; i++) {
					if (read_ends[i] == read_end) {
						sprintf(w_str, "worker %d: ", i);
						write(STDOUT_FILENO, w_str, strlen(w_str));
					}
				}
				while (read(read_end, &buf, 1) > 0)
					write(STDOUT_FILENO, &buf, 1);
				close(read_end);
				read_ends[i] = 0;
				counter--;
			}
		}
	}
	return;
}
