#include <stdio.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

static int toy_timer = 0;

void timer_handler()
{
    printf("timer_expire_signal_handler: %d\n", toy_timer++);
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;

    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

int system_server()
{
    struct sigaction  sa;
    struct itimerval itv = {
        .it_interval = {
            .tv_sec = 5,
            .tv_usec = 0
        },
        .it_value = {
            .tv_sec = 5,
            .tv_usec = 0,
        }
    };

    printf("나 system_server 프로세스!\n");

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = timer_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        puts("sigaction");
        exit(EXIT_FAILURE);
    }

    setitimer(ITIMER_REAL, &itv, NULL);

    while (1) {
        posix_sleep_ms(5000);
    }

    return 0;
}

int create_system_server()
{
    pid_t systemPid;
    const char *name = "system_server";

    printf("여기서 시스템 프로세스를 생성합니다.\n");
    
    systemPid = fork();
    switch (systemPid) {
        case -1:
            puts("system server fork failed...");
            break;
        case 0:
            if (prctl(PR_SET_NAME, (unsigned long)name) < 0)
                perror("prctl()");
            system_server();
            exit(EXIT_SUCCESS);
        default:
    }
    return systemPid;
}
    