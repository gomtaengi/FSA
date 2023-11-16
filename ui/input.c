#include <stdio.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>


#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

#define TOY_TOK_BUFSIZE 64
#define TOY_TOK_DELIM " \t\r\n\a"

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

    printf("%s", s);

    while (1) {
        posix_sleep_ms(5000);
    }

    return 0;
}


int toy_send(char **args);
int toy_shell(char **args);
int toy_exit(char **args);

char *builtin_str[] = {
    "send",
    "sh",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &toy_send,
    &toy_shell,
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
        printf("TOY> ");
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
