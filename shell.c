#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#define MaxLineLength 1024
#define MAX_HISTORY_COUNT 100

//112003077 Prathamesh Kusalkar

char history_list[MAX_HISTORY_COUNT][MaxLineLength];
int history_count = 0;

void add_to_history(char *Arguments)
{
    if (history_count < MAX_HISTORY_COUNT)
    {
        strcpy(history_list[history_count], Arguments);
        history_count++;
    }
    else
    {
        for (int i = 0; i < MAX_HISTORY_COUNT - 1; i++)
        {
            strcpy(history_list[i], history_list[i + 1]);
        }
        strcpy(history_list[MAX_HISTORY_COUNT - 1], Arguments);
    }
}

void display_history()
{
    for (int i = 0; i < history_count; i++)
    {
        printf("%d: %s\n", i + 1, history_list[i]);
    }
}

void ParseCommandLineArguments(char *CommandArr, char **Arguments, char **InputFile, char **OutputFile, char **Ps1)
{
    int i;
    Arguments[0] = strtok(CommandArr, " ");
    for (i = 1; (Arguments[i] = strtok(NULL, " ")); i++)
        ;

    for (int j = 0; j < i; j++)
    {
        if (!strcmp(Arguments[j], "<"))
        {
            if (j + 1 >= i)
            {
                fprintf(stderr, "No Input File Detected\n");
                continue;
            }
            else if (access(Arguments[j + 1], F_OK) == -1)
            {
                fprintf(stderr, "%s: %s\n", Arguments[j + 1], strerror(errno));
                continue;
            }
            *InputFile = Arguments[j + 1];
            Arguments[j] = NULL;
        }
        else if (!strcmp(Arguments[j], ">"))
        {
            if (j + 1 >= i)
            {
                fprintf(stderr, "No Output File Detected\n");
                continue;
            }
            *OutputFile = Arguments[j + 1];
            Arguments[j] = NULL;
        }
        else if (strncmp(Arguments[j], "PS1=", 4) == 0)
        {
            if (strlen(Arguments[j]) <= 4)
            {
                fprintf(stderr, "Write String in Prompt\n");
                continue;
            }
            *Ps1 = Arguments[j] + 4;
            if ((*Ps1)[0] != '\"' || (*Ps1)[strlen(*Ps1) - 1] != '\"')
            {
                fprintf(stderr, "Should be in the format '\"String\"'\n");
                continue;
            }
            (*Ps1)++;
            (*Ps1)[strlen(*Ps1) - 1] = '\0';
        }
    }
}

void RedirectInput(char *InputFile)
{
    int fd = open(InputFile, O_RDONLY);
    if (fd < 0)
    {
        perror("Cannot Open Input File");
        exit(1);
    }
    if (dup2(fd, 0) < 0)
    {
        perror("Input FileDup2 Failed");
        exit(1);
    }
    close(fd);
}

void RedirectOutput(char *OutputFile)
{
    int fd = open(OutputFile, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
    {
        perror("Cannot Open Output File");
        exit(1);
    }
    if (dup2(fd, 1) < 0)
    {
        perror("Dup2 Failed Output File");
        exit(1);
    }
    close(fd);
}

void ExecuteCommand(char **Arguments)
{
    char *paths[] = {"/bin", "/usr/bin", NULL};
    int i = 0;
    while (paths[i])
    {
        char fullpath[MaxLineLength];
        sprintf(fullpath, "%s/%s", paths[i], Arguments[0]);
        if (access(fullpath, F_OK) != -1 && execv(fullpath, Arguments) < 0)
        {
            perror("Exec Failed");
            exit(1);
        }
        i++;
    }
    if (strncmp(Arguments[0], "PS1=", 4) == 0 || strcmp(Arguments[0], "exit") == 0 || fgets(Arguments[0], MaxLineLength, stdin) == NULL)
        ;
    else
        perror("Command Not Found");
    exit(1);
}

void ExecuteCommandWithRedirection(char **Arguments, char *InputFile, char *OutputFile)
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
    {
        perror("Forking Failed");
        exit(1);
    }
    else if (pid == 0)
    { // child
        if (InputFile)
            RedirectInput(InputFile);
        else if (OutputFile)
            RedirectOutput(OutputFile);
        ExecuteCommand(Arguments);
    }
    else
    {
        wait(NULL); // parent
    }
}

int IsDir(char *Arguments)
{
    DIR *dirptr;
    if (access(Arguments, F_OK) != -1)
    {
        if ((dirptr = opendir(Arguments)) != NULL)
        {
            closedir(dirptr);
        }
        else
            return 0;
    }
    else
        return -1;
    return 1;
}

void ChangeDirectory(char **Arguments)
{
    if (Arguments[1])
    {
        if (chdir(Arguments[1]) != 0)
        {
            perror("Cannot Change Directory cd Failed");
        }
    }
    else
    {
        char *home = getenv("HOME");
        if (home == NULL && IsDir)
        {
            perror("HOME Not Set");
        }
        else
        {
            if (chdir(home) != 0)
            {
                perror("Cannot Change Directory cd Failed");
            }
        }
    }
}

void DisplayPrompt(char *Ps1)
{
    if (Ps1)
    {
        if (strcmp(Ps1, "\\w$") == 0)
        {
            char Cwd[MaxLineLength];
            if (getcwd(Cwd, sizeof(Cwd)) != NULL)
            {
                printf("%s$ ", Cwd);
            }
        }
        else
            printf("%s", Ps1);
    }
    else
    {
        char Cwd[MaxLineLength];
        if (getcwd(Cwd, sizeof(Cwd)) != NULL)
        {
            printf("%s$ ", Cwd);
        }
    }
}

int main(int Argc, char *Argv[])
{
    int RunningCondition = 1;
    char CommandArr[MaxLineLength];
    char *Arguments[MaxLineLength];
    char *InputFile = NULL, *OutputFile = NULL;
    char *Ps1 = NULL;

    while (RunningCondition)
    {
        DisplayPrompt(Ps1);
        if (fgets(CommandArr, MaxLineLength, stdin) == NULL)
        {
            printf("\n\t You Pressed Ctrl-D key to Terminate shell\n");
            RunningCondition = 0;
        }
        CommandArr[strlen(CommandArr) - 1] = '\0';
        if (strcmp(CommandArr, "exit") == 0)
        {
            printf("\n\t You Pressed exit command to Terminate shell\n");
            RunningCondition = 0;
        }
        add_to_history(CommandArr);
        ParseCommandLineArguments(CommandArr, Arguments, &InputFile, &OutputFile, &Ps1);
        if (strcmp(Arguments[0], "cd") == 0)
        {
            ChangeDirectory(Arguments);
        }
        else if (strcmp(Arguments[0], "h") == 0)
        {
            display_history();
        }
        else
        {
            ExecuteCommandWithRedirection(Arguments, InputFile, OutputFile);
        }
        InputFile = NULL;
        OutputFile = NULL;
    }
    return 0;
}
