#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <assert.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <toy_message.h>

#define NUM_MESSAGES 10

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;


static void sigchldHandler(int sig) {
    int status;
    pid_t childPid;

    printf("handler: Caught SIGCHLD : %d\n", sig);
    while ((childPid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("handler: Reaped child %ld - ", (long) childPid);
    }

    if (childPid == -1 && errno != ECHILD)
        printf("waitpid");

    puts("handler: returning");
}

mqd_t create_msg_queue(const char *name)
{
    mqd_t msg_q;
    struct mq_attr attr = {0,};
    attr.mq_maxmsg = NUM_MESSAGES;
    attr.mq_msgsize = sizeof(toy_msg_t);

    mq_unlink(name);
    msg_q = mq_open(name, O_RDWR | O_CREAT | O_CLOEXEC, 0777, &attr);
    return msg_q;
}

int main()
{
    pid_t spid, gpid, ipid, wpid;
    int status;
    struct sigaction sa;
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = sigchldHandler;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        puts("sigaction");
        exit(EXIT_FAILURE);
    }

    watchdog_queue = create_msg_queue("/watchdog_queue");
    assert(watchdog_queue != -1);
    monitor_queue = create_msg_queue("/monitor_queue");
    assert(monitor_queue != -1);
    disk_queue = create_msg_queue("/disk_queue");
    assert(disk_queue != -1);
    camera_queue = create_msg_queue("/camera_queue");
    assert(camera_queue != -1);

    printf("메인 함수입니다.\n");
    printf("시스템 서버를 생성합니다.\n");
    spid = create_system_server();
    printf("웹 서버를 생성합니다.\n");
    wpid = create_web_server();
    printf("입력 프로세스를 생성합니다.\n");
    ipid = create_input();
    printf("GUI를 생성합니다.\n");
    gpid = create_gui();

    waitpid(spid, &status, 0);
    waitpid(gpid, &status, 0);
    waitpid(ipid, &status, 0);
    waitpid(wpid, &status, 0);

    return 0;
}
