#include <stdio.h>
#include <sys/prctl.h>
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>


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


int input()
{
    printf("나 input 프로세스!\n");

    struct sigaction sa;
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_handler = segfault_handler;

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        puts("sigaction");
        exit(EXIT_FAILURE);
    }

    while (1) {
        sleep(1);
    }

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
