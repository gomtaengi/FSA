#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

#define STACK_SIZE (4 * 1024 * 1024)

static int web_server()
{
    puts("Web Server 실행합니다.");
    execl("/usr/local/bin/filebrowser", "filebrowser", "-p", "8080", (char *) NULL);
    return 0;
}

int create_web_server()
{
    printf("여기서 Web Server 프로세스를 생성합니다.\n");

    char *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    int flags = SIGCHLD | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS;
    pid_t systemPid = clone(web_server, stack + STACK_SIZE, flags, NULL);
    if (systemPid == -1) {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    munmap(stack, STACK_SIZE);
    return systemPid;
}
