#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

char prompt[20] = "hello";

void handler(int sig)
{
    char *msg = "You typed Control-C!\n";
    char final_msg[50];
    strcpy(final_msg, msg);
    strcat(final_msg,prompt);
    strcat(final_msg, ": ");
    strcat(final_msg, "\0");
    write(1, final_msg, strlen(final_msg));
}

int main()
{
    char command[1024], saved_cmd[1024];
    char *token;
    int i;
    char *file;
    int fd, amper, redirect, piping, retid, status, argc1, append, redirect_left;
    int fildes[2];
    char *argv1[10], *argv2[10];
    int flag = 0;
    memset(saved_cmd, '\0', 1024);
    while (1)
    {
        signal(SIGINT, handler);

        printf("%s: ", prompt);
        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = '\0';
        piping = 0;
        redirect = 0;
        redirect_left = 0;
        append = 0;

        if (!strcmp(command, "quit"))
        {
            break;
        }

        if (!strcmp(command, "!!"))
        {
            strcpy(command, saved_cmd);
        }
        else
        {
            strcpy(saved_cmd, command);
        }

        /* parse command line */
        i = 0;
        token = strtok(command, " ");
        while (token != NULL)
        {
            argv1[i] = token;
            token = strtok(NULL, " ");
            i++;
            if (token && !strcmp(token, "|"))
            {
                piping = 1;
                break;
            }
        }
        argv1[i] = NULL;
        argc1 = i;

        /* Is command empty */
        if (argv1[0] == NULL)
            continue;

        /* Does command contain pipe */
        if (piping)
        {
            i = 0;
            while (token != NULL)
            {
                token = strtok(NULL, " ");
                argv2[i] = token;
                i++;
            }
            argv2[i] = NULL;
        }

        /* Does command line end with & */
        if (!strcmp(argv1[argc1 - 1], "&"))
        {
            amper = 1;
            argv1[argc1 - 1] = NULL;
        }
        else
            amper = 0;

        if (argc1 > 1 && !strncmp(argv1[argc1 - 2], ">>", 2))
        {
            append = 1;
            argv1[argc1 - 2] = NULL;
            file = argv1[argc1 - 1];
        }

        else if (argc1 > 1 && !strcmp(argv1[argc1 - 2], ">"))
        {
            redirect = 1;
            argv1[argc1 - 2] = NULL;
            file = argv1[argc1 - 1];
        }

        else if (argc1 > 1 && !strcmp(argv1[argc1 - 2], "<"))
        {
            redirect_left = 1;
            argv1[argc1 - 2] = NULL;
            file = argv1[argc1 - 1];
        }

        else if (!strcmp(argv1[0], "prompt") && !strncmp(argv1[1], "=", 1) && argc1 == 3)
        {
            strcpy(prompt, argv1[2]);
            continue;
        }

        else if (!strcmp(argv1[0], "echo"))
        {
            if (!strcmp(argv1[1], "$?"))
            {
                printf("%d\n", status);
                continue;
            }
            else
            {

                for (int itr = 1; itr < i; ++itr)
                {
                    printf("%s ", argv1[itr]);
                }
                printf("\n");
                continue;
            }
        }

        else if (!strcmp(argv1[0], "cd"))
        {
            chdir(argv1[1]);
            continue;
        }
        /* for commands not part of the shell command language */

        if (fork() == 0)
        {
            /* redirection of IO ? */
            if (redirect)
            {
                fd = creat(file, 0660);
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
                /* stdout is now redirected */
            }

            if (append)
            {
                fd = open(file, O_CREAT | O_APPEND | O_RDWR, 0660);
                close(STDOUT_FILENO);
                dup(fd);
                close(fd);
            }

            if (redirect_left)
            {
                fd = open(file, O_RDONLY, 0660);
                close(STDIN_FILENO);
                dup(fd);
                close(fd);
            }

            if (piping)
            {
                pipe(fildes);
                if (fork() == 0)
                {
                    /* first component of command line */
                    close(STDOUT_FILENO);
                    dup(fildes[1]);
                    close(fildes[1]);
                    close(fildes[0]);
                    /* stdout now goes to pipe */
                    /* child process does command */
                    execvp(argv1[0], argv1);
                }
                /* 2nd command component of command line */
                close(STDIN_FILENO);
                dup(fildes[0]);
                close(fildes[0]);
                close(fildes[1]);
                /* standard input now comes from pipe */
                execvp(argv2[0], argv2);
            }
            else
                execvp(argv1[0], argv1);
        }
        /* parent continues over here... */
        /* waits for child to exit if required */
        if (amper == 0)
            retid = wait(&status);
    }

    return 0;
}
