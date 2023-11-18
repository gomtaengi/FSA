#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <mqueue.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>
#include <toy_message.h>
#include <shared_memory.h>

#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\r\n\a"
#define TOY_BUFFSIZE 1024

static pthread_mutex_t global_message_mutex  = PTHREAD_MUTEX_INITIALIZER;
static char global_message[TOY_BUFFSIZE] = {0,};

static mqd_t watchdog_queue;
static mqd_t monitor_queue;
static mqd_t disk_queue;
static mqd_t camera_queue;
static shm_sensor_t *the_sensor_info = NULL;

int shm_id[SHM_KEY_MAX - SHM_KEY_BASE];

typedef struct _sig_ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
} sig_ucontext_t;

void 
segfault_handler(int sig_num, siginfo_t *info, void *ucontext) {
    void *array[50] = {0,};
    void *caller_address = 0;
    char **messages = 0;
    int size=0, i=0;
    sig_ucontext_t *uc = 0;

    uc = (sig_ucontext_t*)ucontext;

    caller_address = (void*)uc->uc_mcontext.rip;

    fprintf(stderr, "\n");

    if (sig_num == SIGSEGV) {
        printf("signal %d (%s), address is %p from %p\n",
            sig_num, strsignal(sig_num), info->si_addr,
            (void*)caller_address);
    } else {
        printf("signal %d (%s)\n", sig_num, strsignal(sig_num));
    }

    size = backtrace(array, 50);
    array[1] = caller_address;
    messages = backtrace_symbols(array, size);

    for (i=1; i<size && messages != NULL; ++i) {
        printf("[bt]: (%d) %s\n", i, messages[i]);
    }

    free(messages);
    exit(EXIT_FAILURE);
}


void *sensor_thread(void *arg)
{
    char *s = arg;
    int mqretcode = 0;
    toy_msg_t msg;
    // int i = 0;

    printf("%s", s);

    while (1) {
        posix_sleep_ms(5000);

        msg.msg_type = 1;
        msg.param1 = shm_id[0];
        msg.param2 = 0;
        
        the_sensor_info->temp = 35;
        the_sensor_info->humidity = 80;
        the_sensor_info->press = 55;
        mqretcode = mq_send(monitor_queue, (char *)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);

        // i = 0;
        // pthread_mutex_lock(&global_message_mutex);
        // while (global_message[i] != 0) {
        //     printf("%c", global_message[i]);
        //     fflush(stdout);
        //     posix_sleep_ms(500);
        //     i++;
        // }
        // pthread_mutex_unlock(&global_message_mutex);
    }
    return 0;
}


int toy_send(char **args);
int toy_mutex(char **args);
int toy_shell(char **args);
int toy_message_queue(char **args);
int toy_exit(char **args);

char *builtin_str[] = {
    "send",
    "mu",
    "sh",
    "mq",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_mutex,
    &toy_shell,
    &toy_message_queue,
    &toy_exit
};

int toy_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

int toy_send(char **args)
{
    printf("send message: %s\n", args[1]);
    return 1;
}

int toy_mutex(char **args)
{
    if (args[1] == NULL) {
        return 1;
    }

    printf("save message: %s\n", args[1]);
    pthread_mutex_lock(&global_message_mutex);
    strcpy(global_message, args[1]);
    pthread_mutex_unlock(&global_message_mutex);
    return 1;
}

int toy_message_queue(char **args)
{
    int mqretcode = 0;
    toy_msg_t msg = {0,};
    struct mq_attr attr = {0,};

    if (args[1] == NULL || args[2] == NULL) {
        return 1;
    }

    if (mq_getattr(camera_queue, &attr) == -1)
        return 1;
    
    if (attr.mq_curmsgs == attr.mq_maxmsg) {
        printf("# of messages currently on queue: %ld\n", attr.mq_curmsgs);
        return 1;
    }

    if (!strcmp(args[1], "camera")) {
        msg.msg_type = atoi(args[2]);
        msg.param1 = 0;
        msg.param2 = 0;
        mqretcode = mq_send(camera_queue, (char*)&msg, sizeof(msg), 0);
        assert(mqretcode == 0);
    }

    return 1;
}

int toy_exit(char **args)
{
    return 0;
}

int toy_shell(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    switch (pid) {
        case -1: {
            perror("toy");
        }
        case 0: {
            if (execvp(args[0], args) == -1) {
                perror("toy");
            }
            exit(EXIT_FAILURE);
        }
        default: {
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    return 1;
}

int toy_execute(char **args)
{
    if (args[0] == NULL) {
        return 1;
    }

    for (int i=0; i<toy_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return 1;
}

char *toy_read_line()
{
    char *line = NULL;
    size_t bufSize = 0;

    if (getline(&line, &bufSize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror(": getline\n");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **toy_split_line(char *line)
{
    int bufSize = TOY_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufSize * sizeof(char*));
    char *token, **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "toy: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOY_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufSize) {
            bufSize += TOY_TOK_BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, bufSize * sizeof(char*));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "toy: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOY_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

void toy_loop()
{
    char *line;
    char **args;
    int status;

    do {
        // 여기는 그냥 중간에 "TOY>"가 출력되는거 보기 싫어서.. 뮤텍스
        pthread_mutex_lock(&global_message_mutex);
        printf("TOY> ");
        pthread_mutex_unlock(&global_message_mutex);
        line = toy_read_line();
        args = toy_split_line(line);
        status = toy_execute(args);

        free(line);
        free(args);
    } while (status);
}

void *command_thread(void *arg)
{
    char *s = arg;
    
    printf("%s", s);
    toy_loop();

    return 0;
}


int input()
{
    printf("나 input 프로세스!\n");

    struct sigaction sa;
    pthread_t command_thread_tid, sensor_thread_tid;
    int threads[2];
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_handler = (void*)segfault_handler;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        puts("sigaction");
        exit(EXIT_FAILURE);
    }

    // 공유 메모리 생성
    shm_id[0] = shmget(SHM_KEY_SENSOR, sizeof(shm_sensor_t), IPC_CREAT | 0666);
    if (shm_id[0] == -1) {
        perror("shmget failed...");
        exit(EXIT_FAILURE);
    }
    the_sensor_info = shmat(shm_id[0], 0, 0);
    if (the_sensor_info == (shm_sensor_t*)-1) {
        perror("shmat failed...");
        exit(EXIT_FAILURE);
    }

    watchdog_queue = mq_open("/watchdog_queue", O_RDWR);
    assert(watchdog_queue != -1);
    monitor_queue = mq_open("/monitor_queue", O_RDWR);
    assert(monitor_queue != -1);
    disk_queue = mq_open("/disk_queue", O_RDWR);
    assert(disk_queue != -1);
    camera_queue = mq_open("/camera_queue", O_RDWR);
    assert(camera_queue != -1);

    threads[0] = pthread_create(&command_thread_tid, NULL, 
        command_thread, "command thread start!\n");
    threads[1] = pthread_create(&sensor_thread_tid, NULL,
        sensor_thread, "sensor thread start!\n");

    while (1) {
        sleep(1);
    }

    for (int i=0; i<2; i++)
        pthread_join(threads[i], NULL);

    return 0;
}

int create_input()
{
    pid_t systemPid;
    const char *name = "input";

    printf("여기서 input 프로세스를 생성합니다.\n");

    systemPid = fork();
    switch(systemPid) {
        case -1:
            puts("input fork failed...");
            break;
        case 0:
            if (prctl(PR_SET_NAME, (unsigned long)name) < 0)
                perror("prctl()");
            input();
            exit(EXIT_SUCCESS);
        default:
    }
    
    return systemPid;
}
