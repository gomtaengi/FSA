#include <stdio.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <camera_HAL.h>


pthread_mutex_t system_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  system_loop_cond  = PTHREAD_COND_INITIALIZER;
bool            system_loop_exit = false;

static int toy_timer = 0;

void signal_exit(void);

void timer_handler()
{
    toy_timer++;
    signal_exit();
    // printf("timer_expire_signal_handler: %d\n", toy_timer++);
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;

    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

void *watchdog_thread(void *arg)
{
    char *s = arg;

    printf("%s", s);

    while (1)
    {
        posix_sleep_ms(1000);
    }

    return 0;
}

void *monitor_thread(void *arg)
{
    char *s = arg;

    printf("%s", s);

    while (1)
    {
        posix_sleep_ms(1000);
    }

    return 0;
}

void *disk_service_thread(void *arg)
{
    char *s = arg;
    FILE *apipe;
    char buf[1024];
    char cmd[] = "df -h ./";

    printf("%s", s);

    while (1)
    {
        apipe = popen(cmd, "r");
        if (apipe) {
            while (fgets(buf, sizeof(buf), apipe))
                printf("%s", buf);
            pclose(apipe);
        } else {
            perror("popen() 실패...");
        }

        posix_sleep_ms(10000);
    }
    return 0;
}

void *camera_service_thread(void *arg)
{
    char *s = arg;

    printf("%s", s);

    toy_camera_open();
    toy_camera_take_picture();

    while (1)
    {
        posix_sleep_ms(1000);
    }

    return 0;
}

void signal_exit(void)
{
    pthread_mutex_lock(&system_loop_mutex);
    system_loop_exit = true;
    pthread_cond_broadcast(&system_loop_cond);
    pthread_mutex_unlock(&system_loop_mutex);
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

    pthread_t watchdog_thread_tid;
    pthread_t monitor_thread_tid;
    pthread_t disk_service_thread_tid;
    pthread_t camera_service_thread_tid;
    int threads[4];

    printf("나 system_server 프로세스!\n");

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = timer_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        puts("sigaction");
        exit(EXIT_FAILURE);
    }
    setitimer(ITIMER_REAL, &itv, NULL);

    threads[0] = pthread_create(&watchdog_thread_tid, NULL, watchdog_thread, "watchdog thread\n");
    threads[1] = pthread_create(&monitor_thread_tid, NULL, monitor_thread, "monitor thread\n");
    threads[2] = pthread_create(&disk_service_thread_tid, NULL, 
        disk_service_thread, "disk service thread\n");
    threads[3] = pthread_create(&camera_service_thread_tid, NULL, 
        camera_service_thread, "camera service thread\n");

    printf("system init done.  waiting...");

    pthread_mutex_lock(&system_loop_mutex);
    while (system_loop_exit == false) {
        pthread_cond_wait(&system_loop_cond, &system_loop_mutex);
    }
    pthread_mutex_unlock(&system_loop_mutex);
    puts("<== system\n");

    while (1) {
        posix_sleep_ms(5000);
    }

    for (int i=0; i<4; i++) {
        pthread_join(threads[i], NULL);
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
    