#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void find(char* path, char* filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // M: the same as ls
    if ((fd = open(path, O_RDONLY)) < 0) {
        fprintf(2, "[ERROR] find cannot open %s\n", path);
        return ;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "[ERROR] find cannot stat %s\n", path);
        return ;
    }    

    // M: the path should be a directory
    if (st.type != T_DIR) {
        // fprintf(2, "find: %s is not a directory\n", path);
        fprintf(2, "[ERROR] usage: find <directory> <filename>\n");
        return ;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    // M: if the argument path is not end with '/', add it
    // M: if the argument path is end with '/', we will get diretory//filename, which is also correct
    *p++ = '/';

    // M: one byte for '/' and one byte for '\0'
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        fprintf(2, "[ERROR] path too long\n");
        return ;
    }

    // M: traverse the directory
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
            continue;
        }

        // M: memmove don't tackle the \0 character, so we need to add it manually
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (stat(buf, &st) < 0) {
            fprintf(2, "[ERROR] find cannot stat %s\n", buf);
            continue;
        }

        // M: is a directory, and not the current directory and parent directory
        // M: find recursively
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0) {
            find(buf, filename);
        } else if (strcmp(filename, p) == 0) {
            // printf("%s\n", buf);
            fprintf(1, "%s\n", buf);
        }
    }

    close(fd);
    // return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(2, "[ERROR] usage: find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
