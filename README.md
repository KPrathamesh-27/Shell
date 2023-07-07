# Shell
Shell program in C Language

>Read commands in a loop and run commands without complete path names and handle arguments+options for commands.
>Show prompt with current working directory in the prompt and allow user to change the prompt to a particular string and revert back to CWD prompt.
This should be done with following two commands, with specified syntax:
PS1="whatever string you want "
PS1="\w$"
>Implemented "cd" command. Note that "cd" is always an internal command of shell, and not an executable. It affects the cwd of the shell itself.
>Exit gracefully on typing "exit" or ctrl-D 
>You will NOT be using execvp(), but rather implement a version of execvp() on your own.
>Implement input redirection and output redirection
>Handle multiple pipes
>Handle "history" command
