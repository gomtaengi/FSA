#include <stdio.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <mqueue.h>
#include <assert.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <dirent.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <camera_HAL.h>
#include <toy_message.h>
#include <shared_memory.h>

#define BUF_LEN 1024
#define TOY_TEST_FS "./fs"

#define DUMP_STATE 2

pthread_mutex_t system_loop_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  system_loop_cond  = PTHREAD_COND_INITIALIZER;
bool            system_loop_exit = false;

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;

static int toy_timer = 0;

pthread_mutex_t toy_timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t global_timer_sem;
static bool global_timer_stopped = false;
static shm_sensor_t *the_sensor_info = NULL;

void signal_exit(void);

static void timer_expire_signal_handler()
{
    if (sem_post(&global_timer_sem) == -1)
        exit(EXIT_FAILURE);
}

void system_timeout_handler()
{
    pthread_mutex_lock(&toy_timer_mutex);
    toy_timer++;
    // printf("toy_timer: %d\n", toy_timer);
    pthread_mutex_unlock(&toy_timer_mutex);
    // signal_exit();
    // printf("timer_expire_signal_handler: %d\n", toy_timer++);
}

int posix_sleep_ms(unsigned int timeout_ms)
{
    struct timespec sleep_time;

    sleep_time.tv_sec = timeout_ms / MILLISEC_PER_SECOND;
    sleep_time.tv_nsec = (timeout_ms % MILLISEC_PER_SECOND) * (NANOSEC_PER_USEC * USEC_PER_MILLISEC);

    return nanosleep(&sleep_time, NULL);
}

void set_periodic_timer(long sec_delay, long usec_delay)
{
	struct itimerval itimer_val = {
		 .it_interval = { .tv_sec = sec_delay, .tv_usec = usec_delay },
		 .it_value = { .tv_sec = sec_delay, .tv_usec = usec_delay }
    };
	setitimer(ITIMER_REAL, &itimer_val, NULL);
}

static void *timer_thread(void *not_used)
{
    sem_init(&global_timer_sem, 0, 1);
    signal(SIGALRM, timer_expire_signal_handler);
    set_periodic_timer(3, 0);

	while (!global_timer_stopped) {
		if (sem_wait(&global_timer_sem) == -1)
            exit(EXIT_FAILURE);
        system_timeout_handler();
	}
	return 0;
}

void *watchdog_thread(void *arg)
{
    char *s = arg;
    int mqretcode;
    toy_msg_t msg;
    struct mq_attr attr;

    printf("%s", s);

    while (1)
    {
        if (mq_getattr(watchdog_queue, &attr) == -1)
            return 0;
        if (attr.mq_curmsgs) {
            mqretcode = mq_receive(watchdog_queue, (char *)&msg, sizeof(msg), NULL);
            if (mqretcode >= 0) {
                printf("watchdog_thread: 메시지가 도착했습니다.\n");
                printf("msg.msg_type: %u\n", msg.msg_type);
                printf("msg.param1: %u\n", msg.param1);
                printf("msg.param2: %u\n", msg.param2);
            }
        }
    }

    return 0;
}

#define SENSOR_DATA 1

void dump_file(char *file_name)
{
    char buf[BUFSIZ] = {0};
    size_t buf_len = 0;
    int fd = open(file_name, O_RDONLY);

    while ((buf_len = read(fd, buf, BUFSIZ)) > 0) {
        if (write(1, buf, buf_len) <= 0) {
            break;
        }
    }
    close(fd);
}

void dump_state()
{
    puts("======== dump state ========");
    dump_file("/proc/version");
    puts("============================");
    dump_file("/proc/meminfo");
    puts("============================");
    
}

void *monitor_thread(void *arg)
{
    char *s = arg;
    int mqretcode;
    toy_msg_t msg;
    struct mq_attr attr;

    printf("%s", s);

    while (1)
    {
        if (mq_getattr(monitor_queue, &attr) == -1)
            return 0;
        if (attr.mq_curmsgs) {
            mqretcode = mq_receive(monitor_queue, (char *)&msg, sizeof(msg), NULL);
            if (mqretcode >= 0) {
                printf("monitor_queue: 메시지가 도착했습니다.\n");
                printf("msg.msg_type: %u\n", msg.msg_type);
                printf("msg.param1: %u\n", msg.param1);
                printf("msg.param2: %u\n", msg.param2);
                if (msg.msg_type == SENSOR_DATA) {
                    shm_id[0] = msg.param1;
                    the_sensor_info = shmat(shm_id[0], 0, SHM_RDONLY);
                    if (the_sensor_info == (shm_sensor_t*)-1) {
                        perror("shmat failed...");
                        exit(EXIT_FAILURE);
                    }
                    printf("sensor temp: %d\n", the_sensor_info->temp);
                    printf("sensor press: %d\n", the_sensor_info->press);
                    printf("sensor humidity: %d\n", the_sensor_info->humidity);
                    if (shmdt(the_sensor_info) == -1) {
                        perror("shmdt failed...");
                        exit(EXIT_FAILURE);
                    }
                } else if (msg.msg_type == DUMP_STATE) {
                    dump_state();
                }
            }
        }
    }
    return 0;
}

void display_inotify_event(int fd, int read_len, int total_size)
{
    struct stat sb;
    fstat(fd, &sb);
    
    printf("Read %d bytes from inotify fd\n", read_len);
    printf("directory size: %d\n", total_size);
    printf("File type: ");
    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:  printf("regular file\n");            break;
    case S_IFDIR:  printf("directory\n");               break;
    case S_IFCHR:  printf("character device\n");        break;
    case S_IFBLK:  printf("block device\n");            break;
    case S_IFLNK:  printf("symbolic (soft) link\n");    break;
    case S_IFIFO:  printf("FIFO or pipe\n");            break;
    case S_IFSOCK: printf("socket\n");                  break;
    default:       printf("unknown file type?\n");      break;
    }

    printf("Device containing i-node: major=%ld minor=%ld\n",
        (long)major(sb.st_dev), (long)minor(sb.st_dev));
    printf("I-node number: %ld\n", (long)sb.st_ino);
    printf("Number of (hard) links: %ld\n", (long)sb.st_nlink);
    printf("Ownership:                UID=%ld   GID=%ld\n",
            (long) sb.st_uid, (long) sb.st_gid);

    printf("File size:                %lld bytes\n", (long long) sb.st_size);
    printf("Optimal I/O block size:   %ld bytes\n", (long) sb.st_blksize);
    printf("512B blocks allocated:    %lld\n", (long long) sb.st_blocks);

    printf("Last file access: %s", ctime(&sb.st_atime));
    printf("Last file modification: %s", ctime(&sb.st_mtime));
    printf("Last status change: %s", ctime(&sb.st_ctime));
}

int get_dir_size(char *dir)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat sb;
    int size = 0;
    char file_name[261] = {0};

    dirp = opendir(dir);
    if (dirp == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((dp = readdir(dirp))) {
        if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
            continue;
        
        sprintf(file_name, "%s/%s", TOY_TEST_FS, dp->d_name);
        if (stat(file_name, &sb) == -1) {
            closedir(dirp);
            perror("stat");
            exit(EXIT_FAILURE);
        }
        if (S_ISDIR(sb.st_mode)) {
            size += get_dir_size(file_name);
        } else {
            size += sb.st_size;
        }
    }

    closedir(dirp);

    return size;
}

void *disk_service_thread(void *arg)
{
    char *s = arg;
    // FILE *apipe;
    // char buf[1024];
    // char cmd[] = "df -h ./";
    // int mqretcode;
    // toy_msg_t msg;
    // struct mq_attr attr;
    int inotifyFd, wd;
    char buf[BUF_LEN] __attribute__ ((aligned(8)));
    ssize_t numRead;
    struct inotify_event *event;
    char *directory = TOY_TEST_FS;
    char *p;
    int total_size = 0;    

    printf("%s", s);

    inotifyFd = inotify_init();
    if (inotifyFd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }
    wd = inotify_add_watch(inotifyFd, directory, IN_CREATE);
    if (wd == -1) {
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        numRead = read(inotifyFd, buf, BUF_LEN);
        if (numRead == 0)
            continue;
        else if (numRead == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        for (p=buf; p<buf+numRead;) {
            event = (struct inotify_event*)p;
            total_size = get_dir_size(directory);
            display_inotify_event(event->wd, numRead, total_size);
            p += sizeof(struct inotify_event) + event->len;
        }
        // apipe = popen(cmd, "r");
        // if (apipe) {
        //     while (fgets(buf, sizeof(buf), apipe))
        //         printf("%s", buf);
        //     pclose(apipe);
        // } else {
        //     perror("popen() 실패...");
        // }
        // if (mq_getattr(disk_queue, &attr) == -1)
        //     return 0;
        // if (attr.mq_curmsgs) {
        //     mqretcode = mq_receive(disk_queue, (char *)&msg, sizeof(msg), NULL);
        //     if (mqretcode >= 0) {
        //         printf("disk_queue: 메시지가 도착했습니다.\n");
        //         printf("msg.msg_type: %u\n", msg.msg_type);
        //         printf("msg.param1: %u\n", msg.param1);
        //         printf("msg.param2: %u\n", msg.param2);
        //     }
        // }
    }
    return 0;
}


void *camera_service_thread(void *arg)
{
    char *s = arg;
    int mqretcode;
    toy_msg_t msg;
    struct mq_attr attr;

    printf("%s", s);
    
    toy_camera_open();

    while (1)
    {
        if (mq_getattr(camera_queue, &attr) == -1)
            return 0;
        if (attr.mq_curmsgs) {
            mqretcode = mq_receive(camera_queue, (char *)&msg, sizeof(msg), NULL);
            if (mqretcode >= 0) {
                printf("camera_queue: 메시지가 도착했습니다.\n");
                printf("msg.msg_type: %u\n", msg.msg_type);
                printf("msg.param1: %u\n", msg.param1);
                printf("msg.param2: %u\n", msg.param2);
                if (msg.msg_type == 1) {
                    toy_camera_take_picture();
                } else if (msg.msg_type == DUMP_STATE) {
                    toy_camera_dump();
                }
            }
        }
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
    pthread_t watchdog_thread_tid;
    pthread_t monitor_thread_tid;
    pthread_t disk_service_thread_tid;
    pthread_t camera_service_thread_tid;
    pthread_t timer_thread_tid;
    int threads[5];

    printf("나 system_server 프로세스!\n");

    watchdog_queue = mq_open("/watchdog_queue", O_RDWR);
    assert(watchdog_queue != -1);
    monitor_queue = mq_open("/monitor_queue", O_RDWR);
    assert(monitor_queue != -1);
    disk_queue = mq_open("/disk_queue", O_RDWR);
    assert(disk_queue != -1);
    camera_queue = mq_open("/camera_queue", O_RDWR);
    assert(camera_queue != -1);

    threads[0] = pthread_create(&watchdog_thread_tid, NULL, watchdog_thread, "watchdog thread\n");
    threads[1] = pthread_create(&monitor_thread_tid, NULL, monitor_thread, "monitor thread\n");
    threads[2] = pthread_create(&disk_service_thread_tid, NULL, 
        disk_service_thread, "disk service thread\n");
    threads[3] = pthread_create(&camera_service_thread_tid, NULL, 
        camera_service_thread, "camera service thread\n");
    threads[4] = pthread_create(&timer_thread_tid, NULL, timer_thread, "timer thread\n");

    puts("system init done.  waiting...");

    pthread_mutex_lock(&system_loop_mutex);
    while (system_loop_exit == false) {
        pthread_cond_wait(&system_loop_cond, &system_loop_mutex);
    }
    pthread_mutex_unlock(&system_loop_mutex);
    puts("<== system\n");

    while (1) {
        posix_sleep_ms(5000);
    }

    for (int i=0; i<5; i++) {
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
    