#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

#define OUT 0 // Output direction
#define APP 1 // Append direction
#define IN 2  // Input direction
int status;
char *prompt = "hello";

void handler(int sig);
void pipeTask(char *cmd);
void asyncTask(char *cmd);
void redirectTask(char *cmd, int direction);
void basicTask(char *cmd);

int main()
{
    signal(SIGINT, handler);
    char cmd[1024], saved_cmd[1024];
    while (1)
    {
        // Prints prompt and gets command from sdtin
        printf("%s: ", prompt);
        fgets(cmd, 1024, stdin);
        cmd[strlen(cmd) - 1] = '\0';

        /*
        Checking for quit / repeat last command requests.
        else: saving current command
        */
        if (!strcmp(cmd, "quit"))
            break;

        else if (!strcmp(cmd, "!!"))
            strcpy(cmd, saved_cmd);

        else
            strcpy(saved_cmd, cmd);

        // Checking if command contains a pipe
        if (strchr(cmd, '|'))
            pipeTask(cmd);

        // Checking for asynchronic command
        else if (strchr(cmd, '&'))
            asyncTask(cmd);

        /*  Checking for redirection commands */

        // Create / Overwrite
        else if (strchr(cmd, '>') && !strstr(cmd, ">>"))
            redirectTask(cmd, OUT);

        // Input file to command
        else if (strchr(cmd, '<'))
            redirectTask(cmd, IN);

        // Append to file
        else if (strstr(cmd, ">>"))
            redirectTask(cmd, APP);

        else
            basicTask(cmd);
    }
}

int parseCmd(char **parsed_cmd, char *cmd, const char *delimeter)
{
    char *token;
    token = strtok(cmd, delimeter);
    int counter = -1;

    while (token)
    {
        parsed_cmd[++counter] = malloc(strlen(token) + 1);
        strcpy(parsed_cmd[counter], token);
        if (delimeter == "|")
        {
            if (parsed_cmd[counter][strlen(token) - 1] == ' ')
                parsed_cmd[counter][strlen(token) - 1] = '\0';
            if (parsed_cmd[counter][0] == ' ')
                memmove(parsed_cmd[counter], parsed_cmd[counter] + 1, strlen(token));
        }
        parsed_cmd[counter][strlen(token) + 1] = '\0';
        token = strtok(NULL, delimeter);
    }
    parsed_cmd[++counter] = NULL;
    return counter;
}

void handler(int sig)
{
    char *msg = "You typed Control-C!\n";
    char final_msg[50];
    strcpy(final_msg, msg);
    strcat(final_msg, prompt);
    strcat(final_msg, ": ");
    strcat(final_msg, "\0");
    write(1, final_msg, strlen(final_msg));
}

void pipeTask(char *cmd)
{
    char *parsed_cmd[50];
    int cmds = parseCmd(parsed_cmd, cmd, "|");
    char *inner_cmd[50];
    int fd[cmds][2];
    for (int i = 0; i < cmds; ++i)
    {
        int inner_cmds = parseCmd(inner_cmd, parsed_cmd[i], " ");
        if (i != cmds - 1)
            pipe(fd[i]);

        if (fork() == 0)
        {
            if (i != cmds - 1)
            {
                dup2(fd[i][1], 1);
                close(fd[i][0]);
                close(fd[i][1]);
            }
            if (i != 0)
            {
                dup2(fd[i - 1][0], 0);
                close(fd[i - 1][0]);
                close(fd[i - 1][1]);
            }
            execvp(inner_cmd[0], inner_cmd);
        }
        if (i != 0)
        {
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL);
    }
}

void asyncTask(char *cmd)
{
    if (fork() == 0)
    {
        char *parsed_cmd[50];
        int cmds = parseCmd(parsed_cmd, cmd, " ");
        parsed_cmd[cmds - 1] = NULL;
        execvp(parsed_cmd[0], parsed_cmd);
    }
}

void redirectTask(char *cmd, int direction)
{

    if (fork() == 0)
    {
        char *parsed_cmd[50];
        int cmds = parseCmd(parsed_cmd, cmd, " ");
        int fd;
        switch (direction)
        {
        case OUT:
            fd = creat(parsed_cmd[cmds - 1], 0660);
            dup2(fd, 1);
            break;

        case APP:
            fd = open(parsed_cmd[cmds - 1], O_CREAT | O_APPEND | O_RDWR, 0660);
            dup2(fd, 1);
            break;

        case IN:
            fd = open(parsed_cmd[cmds - 1], O_RDONLY, 0660);
            dup2(fd, 0);
            break;

        default:
            break;
        }

        parsed_cmd[cmds - 2] = parsed_cmd[cmds - 1] = NULL;
        execvp(parsed_cmd[0], parsed_cmd);
    }
    else
        wait(&status);
}

void basicTask(char *cmd)
{
    char *parsed_cmd[50];
    parseCmd(parsed_cmd, cmd, " ");
    if (!strcmp(parsed_cmd[0], "cd"))
        chdir(parsed_cmd[1]);
    else if (!strcmp(parsed_cmd[0], "prompt"))
        prompt = parsed_cmd[2];
    else if (!strcmp(parsed_cmd[0], "echo") && !strcmp(parsed_cmd[1], "$?"))
        printf("%d\n", status);
    else if (fork() == 0)
        execvp(parsed_cmd[0], parsed_cmd);
    else
        wait(&status);
}