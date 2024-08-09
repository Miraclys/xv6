#include "kernel/types.h"
#include "user/user.h"

#define READ 0
#define WRITE 1

void primes(int pipefd[2]) {
    close(pipefd[WRITE]);

    int first_data, flg = 0, cur_data;

    flg = (read(pipefd[READ], &first_data, sizeof(int)) == sizeof (int));

    // printf("first_data: %d\n", first_data);

    if (flg == 0) {
        close(pipefd[READ]);
        exit(0);
    }

    fprintf(1, "prime %d\n", first_data);

    int right_pipefd[2];
    pipe(right_pipefd);
    // close(right_pipefd[READ]);

    while (read(pipefd[READ], &cur_data, sizeof(int)) == sizeof(int)) {
        if (cur_data % first_data != 0) {
            write(right_pipefd[WRITE], &cur_data, sizeof(int));
        }
    }
    close(pipefd[READ]);
    close(right_pipefd[WRITE]);

    int pid = fork();
    if (pid == 0) {
        primes(right_pipefd);
    } else {
        // close(right_pipefd[WRITE]);
        wait(0);
    }
}

int main() {
    int pipefd[2];
    pipe(pipefd);

    // close(pipefd[READ]); 一开始关闭过早，子进程 fork 后也是关闭的。

    for (int i = 2; i <= 35; ++i) {
        write(pipefd[WRITE], &i, sizeof(i));
    }

    int pid = fork();
    if (pid == 0) {
        primes(pipefd);
    } else {
        close(pipefd[WRITE]);
        close(pipefd[READ]);
        wait(0);
    }

    exit(0);
}
