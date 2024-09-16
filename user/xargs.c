#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

#define MAXN 1024

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: xargs <command> [args...]\n");
        exit(1);
    }

    char* argvs[MAXARG];
    int index = 0;
    for (int i = 1; i < argc; ++i) {
        argvs[index++] = argv[i];
    }
    
    char buf[MAXN] = {"\0"};

    int n;
    char temp[MAXN];
    // M: read the input from stdin, store it in buf
    while ((n = read(0, buf, MAXN)) > 0) {
        int j = 0;
        for (int i = 0; i < n; ++i) {
            // M: if the current character is '\n' or the last character in the buffer
            if (buf[i] == '\n' || i == n - 1) {
                if (buf[i] != '\n') {
                    temp[j++] = buf[i];
                }
                // M: the two kinds of implementation are the same
                temp[j] = 0;
                // temp[j] = '\0';
                if (index < MAXARG - 1) {
                    // argvs[index] = strdup(temp);

                    // M: add the new argument to the argvs
                    argvs[index] = temp; 
                    argvs[index + 1] = 0; 
                }
                if (fork() == 0) {
                    // M: only add one argument to the argvs a time
                    exec(argv[1], argvs);
                    exit(1);
                }
                wait(0);
                j = 0;
                memset(temp, 0, sizeof(temp));
                // memset(temp, 0, MAXN);
            } else {
                temp[j++] = buf[i];
            }
        }
    }
    exit(0);
}

