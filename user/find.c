#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *path, char *target) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    //判断路径是否可打开
    if ((fd = open(path, 0) )< 0) {
        fprintf(2, "find: cannot open %s.\n", path);
        return ;
    }
    //判断属性
    if (fstat(fd, &st) < 0) {
        // fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    //把路径存储到buf中，并插入/在后面存放插入的文件
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    //循环读取该目录下的所有文件
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        //去掉空目录 . ..
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;
        //将其文件名拼接到buf（path后）
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
            // printf("find: cannot stat %s\n", buf);
            continue;
        }
        //判断是不是目标文件，若为目录则继续循环
        if (st.type == T_FILE && strcmp(de.name, target) == 0) {
            printf("%s\n", buf);
        } else if (st.type == T_DIR) {
            find(buf, target);
        }
    }
    close(fd); //最后关闭，防止内存泄漏
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage: find [file] [filename]!");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}
