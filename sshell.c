/**
 * @file sshell.c
 * @author Adi Dahari, Shahak Nir
 * @brief 
 * @version 1.0
 * @date 2022-01-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

#define OUT 0               // Output direction
#define APP 1               // Append direction
#define IN 2                // Input direction
int status;                 // For saving last command's status
char *prompt = "hello";     // Inital prompt message

void handler(int sig);
void pipeTask(char *cmd);
void asyncTask(char *cmd);
void redirectTask(char *cmd, int direction);
void basicTask(char *cmd);

int main()
{
    // Assigning handler for Ctrl+C
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
/*
This function converts a char* to a char** of commands.
As pipes uses it too, a delimeter can be chosen (' ' or '|').
For easier funcionality, if delimeter is '|' it removes whitespaces from start and end of a command.
@param {char**} parsed_cmd - Pre-Initialized array of char* for saving the commands for easy use.
@param {char*} cmd - the initial command to be parsed given as char* (string).
@param {const char*} delimeter - the char(s) to seperate each command by.
@returns {int} - The number of commands returned. 
*/
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

/*
This function is the Signal Handler provided to signal() in main.
If the user hits Ctrl+C a message is printed and the program does not terminate.
*/
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

/*
This function handles piped commands.
1st step: 
    1a. Using the parseCmd function to seperate each piped command
        and getting number of piped commands.
    1b. initializing char ** object for parsing inner commands
    1c. initializing an 2D-array(int) with size: ((number of piped commands) * 2)
        for the piping of each command (except the last one!)
2nd step: 
    2a. Using the parseCmd function to seperate command's parts
        and getting number of parts each command has.
    2b. handling pipes and I/O management for correct piping stream. (described in-line)

@param {char*} cmd - The whole piped command as char*(string).
*/
void pipeTask(char *cmd)
{
    char *parsed_cmd[50];
    int cmds = parseCmd(parsed_cmd, cmd, "|"); // cmds = Number of piped commands
    char *inner_cmd[50];
    int fd[cmds][2];
    for (int i = 0; i < cmds; ++i) // Looping through all piped commands.
    {
        int inner_cmds = parseCmd(inner_cmd, parsed_cmd[i], " "); // inner_cmds = Number of parts in current command
        if (i != cmds - 1) // Not the last command! pipe
            pipe(fd[i]);

        if (fork() == 0) // Child #1
        {
            if (i != cmds - 1) // Not the last command! Switching its pipe output to stdout, closing in/out of pipe
            {
                dup2(fd[i][1], 1);
                close(fd[i][0]);
                close(fd[i][1]);
            }
            if (i != 0) // Not parent! Switching previous commands pipe input to stdin and closing its pipe in/out 
            {
                dup2(fd[i - 1][0], 0);
                close(fd[i - 1][0]);
                close(fd[i - 1][1]);
            }
            execvp(inner_cmd[0], inner_cmd); //execute commnad.
        }
        if (i != 0) // Not 1st command! closing previous commands in/out pipes
        {
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL); // Wait for next commands return, as it has been piped and needs to wait for next commands to finish.
    }
}


/*
This function handles the asynchronous command '&'.
It allows the program to read while working' without waiting for previous command to finish.

@param {char*} cmd - The command as char*(string), which ends with '&'.
*/
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

/*
This function handles redirections: {'<', '>', '>>'}.
each direction defer from each other by the definition of: OUT = output, APP = append, IN = input.

@param {char*} cmd - The command as char*(string).
@param {int} direction - {OUT, APP, IN}.
*/
void redirectTask(char *cmd, int direction)
{

    if (fork() == 0) // Child
    {
        char *parsed_cmd[50];
        int cmds = parseCmd(parsed_cmd, cmd, " ");
        int fd; // For redirection
        switch (direction)
        {
        case OUT:
            fd = creat(parsed_cmd[cmds - 1], 0660); // Create or overwrite file.
            dup2(fd, 1); // Easier than dup, handles closure of I/O
            break;

        case APP:
            fd = open(parsed_cmd[cmds - 1], O_CREAT | O_APPEND | O_RDWR, 0660); // Create or append to file
            dup2(fd, 1);
            break;

        case IN:
            fd = open(parsed_cmd[cmds - 1], O_RDONLY, 0660); // Getting file as an input
            dup2(fd, 0);
            break;

        default:
            break;
        }

        parsed_cmd[cmds - 2] = parsed_cmd[cmds - 1] = NULL; // Removing unneccessary parts of command.
        execvp(parsed_cmd[0], parsed_cmd); // Execuiting command.
    }
    else
        wait(&status); // Parent - wait for child to finish.
}

/*
This function handles parent's functionality
or basic Shell commands (such as 'ls', 'grep', 'sort' etc..).

@param {char*} cmd - The command as char*(string).
*/
void basicTask(char *cmd)
{
    char *parsed_cmd[50];
    parseCmd(parsed_cmd, cmd, " ");
    if (!strcmp(parsed_cmd[0], "cd")) // cd - Change current working directory
        chdir(parsed_cmd[1]);
    else if (!strcmp(parsed_cmd[0], "prompt")) // prompt - Change prompt message.
        prompt = parsed_cmd[2];
    else if (!strcmp(parsed_cmd[0], "echo") && !strcmp(parsed_cmd[1], "$?")) // echo $? - Echo last commands status.
        printf("%d\n", status);
    else if (fork() == 0) // Not any of above - basic Shell command, execute if child
        execvp(parsed_cmd[0], parsed_cmd);
    else // Not any of above - not a child also - wait for child to finish and save its returned status.
        wait(&status);
}