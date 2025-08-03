#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//筛选是从最小的素数开始的，之后的所有数字只要和它不成倍数关系即可。并且给找到的prime打印将其在管道中“去掉”
void sieve(int fd) {
    int buf;
    int pipes[2];

    pipe(pipes);
    if (read(fd, &buf, sizeof(int)) == 0) {
        close(fd);
        exit(0);
    }

    printf("prime %d\n", buf);

    if (fork() == 0) {
        close(pipes[1]);
        sieve(pipes[0]);
    } else {
        close(pipes[0]);
        int buf_;
        while(read(fd, &buf_, sizeof(int)) > 0) {
            if (buf_ % buf != 0) {
                write(pipes[1], &buf_, sizeof(int));
            }
        }
        close(pipes[1]);
        wait(0);
    }

    exit(0);
}

//父进程读取数据并等待，子进程调用sieve进行筛选
int main(int args, char *argv[]) {
    int pipes[2];

    pipe(pipes);
    if (fork() == 0) {
        close(pipes[1]);
        sieve(pipes[0]);  
    } else {
        close(pipes[0]);
        for (int i = 2; i <= 35; i++) {
            write(pipes[1], &i, sizeof(int));
        }
        close(pipes[1]);
        wait(0);
    }

    exit(0);
}   