#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char * agrv[]) {
    int pipe_p[2], pipe_c[2];
    char buf[] = {'a'};
    int pid;

    pipe(pipe_c);
    pipe(pipe_p);

    pid = fork();
    //parent send in pipe_c[1], child receives in pipe_c[0]
    //child send in pipe_p[1], parent receives in pip_p[0]
    if (pid == 0) {
        //child 
        pid = getpid();
        close(pipe_p[0]);
        close(pipe_c[1]);
        read(pipe_c[0], buf, 1);
        printf("%d: received ping\n", pid);
        write(pipe_p[1], buf, 1);
        exit(0);
    } else {
        //parent
        pid = getpid();
        close(pipe_c[0]);
        close(pipe_p[1]);
        write(pipe_c[1], buf, 1);
        read(pipe_p[0], buf, 1);
        printf("%d: received pong\n", pid);
        exit(0);

    }

}