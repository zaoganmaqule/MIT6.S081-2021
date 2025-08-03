#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLEN 512

int main(int argc, char * argv[]) {
    char buf[MAXLEN] = {0};
    uint occupy = 0; //缓冲区已经使用的字节数
    char *xargv[MAXARG] = {0};
    int stdin_end = 0; //标准输入的结尾

    for (int i = 1; i < argc; i++) { //先把xargs的参数存储到xargv里面
        xargv[i - 1] = argv[i];
    }

    while (!(stdin_end && occupy == 0)) { //缓冲区读取完毕， 标准输入结尾
        if (!stdin_end) {
            int remain_size = MAXLEN - occupy; //读取缓冲区的字节
            int read_bytes = read(0, buf + occupy, remain_size); 
            if (read_bytes < 0) {
                fprintf(2, "xargs: read returns -1 error\n");
            }
            if (read_bytes == 0) {
                close(0);
                stdin_end = 1;
            } 
            occupy += read_bytes;
        }

        char *line_end = strchr(buf, '\n'); //找到第一个\n的位置
        while (line_end) { //找到
            char xbuf[MAXLEN] = {0};
            memcpy(xbuf, buf, line_end - buf); //把\n前部分拷贝到xbuf
            xargv[argc - 1] = xbuf;
            
            if (fork() == 0) {
                if (!stdin_end) {
                    close(0);
                }
                if (exec(argv[1], xargv) < 0) {
                    fprintf(2, "xargs: exec fails with -\n");
                    exit(1);
                }
            } else {
                memmove(buf, line_end + 1, occupy - (line_end - buf) - 1);//重新设置buf
                occupy -= line_end - buf + 1;
                memset(buf + occupy, 0, MAXLEN - occupy);//后面置空
                // harvest zombie
                int pid;
                wait(&pid);
                line_end = strchr(buf, '\n'); //再次找下一个\n
            }
        }
    }
    exit(0);
}