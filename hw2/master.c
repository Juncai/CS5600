#include "master.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

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

	/* char *worker_path = "./worker"; */
	/* char *mechanism = "sequential"; */
	/* int x = 2; */
	/* int n = 3; */


	computeExponential(worker_path, mechanism, x, n);
	return 0;
}

void computeExponential(char *worker_path, char *mechanism, int x, int n)
{
	int i;
	int status = 0;
	char x_str[50];
	char n_str[50];
	/* printf("n: %d\n", n); */
	int cid = -1;
	char i_str[50];
	char w_str[50];
	int pipefds[2];
	char buf;
	int exec_res;
	/* char *argv[] = {"./worker", "-x", x_str, "-n", i_str}; */
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
		int read_ends[n];
		int write_ends[n];
		for (i = 0; i <= n; i++) {
			pipe(pipefds);
			read_ends[i] = pipefds[0];
			write_ends[i] = pipefds[1];
		}

		for (i = 0; i <= n; i++) {
			printf("read_ends: %d\n", read_ends[i]);
		}

		/* for (i = 0; i <= n; i++) { */
		/* 	sprintf(i_str, "%d", i); */
		/* 	pipe(pipefds); */
		/* 	cid = fork(); */
		/* 	if (cid == 0) { */
		/* 		close(pipefds[0]); */
		/* 		dup2(pipefds[1], STDOUT_FILENO); */
		/* 		exec_res = execv(worker_path, argv); */
		/* 		if (exec_res == -1) perror("exec failed"); */
		/* 		_exit(EXIT_FAILURE); */
		/* 	} else { */
		/* 		waitpid(cid, &status, 0); */
		/* 		close(pipefds[1]); */
		/* 		sprintf(w_str, "worker %d: ", i); */
		/* 		write(STDOUT_FILENO, w_str, strlen(w_str)); */
		/* 		while (read(pipefds[0], &buf, 1) > 0) */
                   /* write(STDOUT_FILENO, &buf, 1); */
		/* 		close(pipefds[0]); */
		/* 	} */
		/* } */
	}

	return;
}
