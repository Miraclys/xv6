#include "kernel/types.h"
#include "user/user.h"

#define READ 0
#define WRITE 1

int main(int argc, char* argv[]) {
	char data = 'a';

	int child2parent[2];
	int parent2child[2];

	pipe(child2parent);
	pipe(parent2child);

	int pid = fork();
	int flg = 0;

	if (pid < 0) {
		fprintf(2, "[ERROR] fork error.");
		close(child2parent[READ]);
		close(child2parent[WRITE]);
		close(parent2child[READ]);
		close(child2parent[WRITE]);
		exit(1);
	} else if (pid == 0) {
		close(parent2child[WRITE]);
		close(child2parent[READ]);

		// 先执行的话会阻塞
		if (read(parent2child[READ], &data, sizeof(char)) != sizeof(char)) {
			fprintf(2, "[ERROR] child read error.");
			flg = 1;
		} else {
			fprintf(1, "%d: received ping\n", getpid());
		}
		close(parent2child[READ]); // M: try to close here

		if (write(child2parent[WRITE], &data, sizeof(char)) != sizeof(char)) {
			fprintf(2, "[ERROR] child write error.");
			flg = 1;
		}

		// close(parent2child[READ]);
		close(child2parent[WRITE]);

		exit(flg);
	} else {
		close(child2parent[WRITE]);
		close(parent2child[READ]);

		if (write(parent2child[WRITE], &data, sizeof(char)) != sizeof(char)) {
			fprintf(2, "[ERROR] parent write error.");
			flg = 1;
		}
		close(parent2child[WRITE]); // M: try to close here

		// 这里会阻塞，等待子进程写入
		if (read(child2parent[READ], &data, sizeof(char)) != sizeof(char)) {
			fprintf(2, "[ERROR] parent read error.");
			flg = 1;
		} else {
			fprintf(1, "%d: received pong\n", getpid());
		}

		close(child2parent[READ]);
		// close(parent2child[WRITE]);

		int status; // M: fix the logic of exit between parent and child
		wait(&status);
		if (status != 0) {
			flg = 1;
		}

		exit(flg);
	}
}

