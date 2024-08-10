#include "kernel/types.h"
#include "user/user.h"
#include "kernel/sysinfo.h" // 因为 user.h 中只是声明了 sysinfo 这一个 struct，
                            //并没有具体定义，所以我们需要额外引入具体定义

int main(int argc, char* argv[]) {
    struct sysinfo info;
    sysinfo(&info);
    printf("free memory: %d\n", info.freemem);
    printf("process number: %d\n", info.nproc);
    exit(0);
}