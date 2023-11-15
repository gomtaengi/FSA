#include <stdio.h>

#include <system_server.h>
#include <gui.h>
#include <input.h>
#include <web_server.h>

int create_gui()
{
    pid_t systemPid;

    printf("여기서 GUI 프로세스를 생성합니다.\n");

    sleep(3);

    systemPid = fork();
    switch (systemPid) {
        case -1:
            break;
        case 0:
            execl("/usr/bin/google-chrome-stable", "google-chrome-stable", "http://localhost:8080", NULL);
            exit(EXIT_SUCCESS);
        default:
    }

    return systemPid;
}
