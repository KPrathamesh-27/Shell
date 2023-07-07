#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <ctype.h>

#define LINES 10000

char origPath[100][100] ={ "/usr/local/sbin/", "/usr/local/bin/", "/usr/sbin/", "/usr/bin/", "/sbin:/bin/", "/usr/game/", "/usr/local/games/", "/snap/bin/", "/snap/bin/"};
char* historyArr[LINES];
int historyLen;
int sigintreceived, sigstpreceived;
int gotinput;

char* execExists(char execFile[512]){
	int n = 10;

	for(int i=0; i<n; i++){
		if(strlen(origPath[i]) > 0){
			char* execPath = malloc(sizeof(char)*100);
			strcpy(execPath, origPath[i]);

			strcat(execPath, execFile);
			if(access(execPath, F_OK) == 0)
				return execPath;
		}
	}

	return NULL;
}

void yellow() {
  printf("\033[1;33m");
}
void reset () {
  printf("\033[0m");
}

typedef struct ArgInfo{
	char* arguments[20];
	char* streamFile;
	char* cd_folder;
	int redirectInput;
	int redirectOutput;
}ArgInfo;

typedef struct job{
    pid_t pid;
    char work[256];
    int status;
}job;

job *p;

typedef struct list
{
    job *t;
    struct list *next;
    
}list;

list *head;

char* parseCommand(char input[], int* k, int len){
	int iter = 0;
    int K = *k;

	while(K < len && input[K] == ' ') K++;
	char* cmd = malloc(sizeof(char)*512);

	while(K < len){
		if(input[K] == ' ' || input[K] == '>' || input[K] == '<'){
			break;
		}
		
		cmd[iter++] = input[K];
		K++;
	}
	*k = K;
	cmd[iter] = '\0';
	return cmd;
}

ArgInfo parseArguments(char cmd[], char input[], int k, int len, bool redirectOutput, bool redirectInput){
		char* arguments[20];
		
		for(int i=0; i<20; i++){
			arguments[i] = (char*)malloc(sizeof(char)*100);
		}
		int argc = 0;
		strcpy(arguments[argc++], cmd);

		char cd_folder[100] = {0};
		if(strcmp(cmd, "cd") == 0){
			int tmp = k;
			
			while(input[tmp] == ' ') tmp++;
			int cdJ = 0;

			while(tmp < len){
				cd_folder[cdJ++] = input[tmp++];	
			}
			cd_folder[cdJ] = '\0';
		}

		char streamFile[100] = {0};
		while(k < len){
			if(input[k] == ' ' || input[k] == ',' ){
				k++;
				continue;
			}

			if( input[k] == '<' && redirectInput){
				k++;
				break;
			}
			
			if( (input[k] == '>' && redirectOutput)){
				k++;
				break;
			}

			if(input[k] == '-'){
				char word[100] = {0};
				int j=0;
				while(input[k] != ' ' && k < len)
					word[j++] = input[k++];
				word[j] = '\0';
				strcpy(arguments[argc++], word);
				continue;
			}
			char word[100];
			int j=0;
			while(input[k] != ' '  && input[k] != ',' && k < len)
				word[j++] = input[k++];
			word[j] = '\0';
			strcpy(arguments[argc++], word);
		}

		if(redirectOutput || redirectInput){
			int j=0;
			while(input[k] == ' ') k++;
			while(input[k] != ' ' && k < len)
					streamFile[j++] = input[k++];
		        streamFile[j] = '\0';

		}

		for(int i=argc; i<20; i++)
			arguments[i] = NULL;

		ArgInfo ret;
		for(int i=0; i<20; i++){
			if(arguments[i]){
				ret.arguments[i] = (char*)malloc(sizeof(char)*100);
				strcpy(ret.arguments[i] , arguments[i]);
			}
			else ret.arguments[i] = NULL;
		}
		if(strlen(streamFile)>0){
			ret.streamFile = (char*)malloc(sizeof(char)*100);
			strcpy(ret.streamFile, streamFile);
		}
		else ret.streamFile = NULL;
		
		if(strlen(cd_folder)>0){
			ret.cd_folder = (char*)malloc(sizeof(char)*100);
			strcpy(ret.cd_folder, cd_folder);
		}
		else ret.cd_folder = NULL;

		return ret;
}

void handlePipes(char input[]){
	int n = strlen(input);

	char* commands[100];
	
	int commandCount = 0;
	commands[commandCount] = strtok(input, "|");

	while(input != NULL){
		commands[commandCount] =  input;
		char ch = '\0';
		strncat(commands[commandCount], &ch, 1);
		commandCount++;
		input = strtok(NULL, "|");
	}
	
	ArgInfo trimCommands[100];
	
	for(int i=0; i<commandCount; i++){
		int k = 0;
		int len = strlen(commands[i]);
		bool redirectInput = false, redirectOutput = false;

		if(strstr(commands[i], ">")) redirectOutput = true;
		if(strstr(commands[i], "<")) redirectInput = true;

		char* cmd = parseCommand(commands[i], &k, len);

		ArgInfo tmpArg = parseArguments(cmd, commands[i], k, len, &redirectOutput, redirectInput);
		tmpArg.redirectInput = redirectInput;
		tmpArg.redirectOutput = redirectOutput;
		

		trimCommands[i] = tmpArg;
		// check if it contains cd and exit
		for(int j=0; j<20; j++)
			if(trimCommands[i].arguments[j])
				if(strcmp(trimCommands[i].arguments[j], "cd") == 0)
					exit(0);
	}

	int fdBackup = 0;

	int pfd[2];
	for(int i=0; i<commandCount; i++){
		pipe(pfd);
		int pid = fork();
		if(pid < 0){
			printf("fork failed\n");
			exit(0);
		}

		if(pid == 0){

			close(pfd[0]);
			if(fdBackup){
				close(0);
				dup(fdBackup);
				char buf[1024];
				// read(fdBackup, buf, sizeof(buf));
				// printf("buff child: %s\n", buf);
			}
			if(i != commandCount-1){
				// printf("redirecting....\n");
				close(1);
				dup(pfd[1]);
				// close(pfd[1]);
			} 
			else close(pfd[1]);
			
			char* ret = execExists(trimCommands[i].arguments[0]);

			if(ret == NULL){
				printf("No path found for the given command\n");
				exit(0);
			}
			else{
				trimCommands[i].arguments[0] = ret;
				
				if(trimCommands[i].redirectInput){
					close(0);
						int fd = open(trimCommands[i].streamFile, O_RDONLY);
					if(fd < 0){
						printf("Failed to open %s\n", trimCommands[i].arguments[1]);
					}
				}
				if(trimCommands[i].redirectOutput){
					close(1);
					int fd = open(trimCommands[i].streamFile, O_WRONLY| O_TRUNC | O_CREAT , 0777); 
					if(fd < 0){
						printf("Failed to open %s\n", trimCommands[i].arguments[1]);
					}
				}
				execv(ret, trimCommands[i].arguments);
				printf("exec failed\n");
				exit(0);
			}
		}
		else{
			wait(0);
			if(i != commandCount-1){
				close(pfd[1]);
				// char buf[1024];
				// read(pfd[0], buf, sizeof(buf));
				// printf("buf par: %s\n", buf);
				// close(0);
				// dup(pfd[0]);
				fdBackup = pfd[0];
			}
		}
	}
	close(pfd[0]);
	close(pfd[1]);

	exit(0);
}

void fillhistoryArr(char* historyPath){
	int fd;
	if(access(historyPath, F_OK) == 0)
		fd = open(historyPath, O_RDONLY);
	else{
	 fd = open(historyPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
	 historyArr[0] = "";
	 historyLen = 1;
	 return;
	}
	char buff[1024];
	char cmd[512];
	int k=0;
	int l = 0;
	while(read(fd, buff, 1)){
		if(buff[0] == '\n'){ 
			cmd[k] = '\0';
			// if(historyArr[l] != NULL) free(historyArr[l]);
			historyArr[l] = (char*)malloc(sizeof(char)*(strlen(cmd)+1));
			strcpy(historyArr[l++], cmd);
			k = 0;
			continue;
		}

		cmd[k++] = buff[0];
	}
	historyArr[l++] = "";
	historyLen = l;
	return;
}

static struct termios old, new;

/* Initialize new terminal i/o settings */
void initTermios(int echo){
  tcgetattr(0, &old); /* grab old terminal i/o settings */
  new = old; /* make new settings same as old settings */
//new.c_lflag &= ~(ECHO | ECHOE | ICANON);
  new.c_lflag &= ~ICANON; /* disable buffered i/o */
//   new.c_cc[VMIN] = 1;
//    new.c_cc[VEOF] = _POSIX_VDISABLE;
//   new.c_cc[VTIME] = 50;
  new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
//   new.c_lflag &= ~ISIG;
//   new.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ISIG | ICRNL);

  tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void){
  tcsetattr(0, TCSANOW, &old);
}

char* pwd;
char prompt[1024];
int customPrompt;

void printPrompt(){
	if(customPrompt){
		yellow();
		printf( "%s", prompt );
		reset();
	}	
	else{
		yellow();
		printf("%s:$ ", pwd);
		reset();
	}
	return;
}


void inthandler(int signo){

	printf("\n");
	sigintreceived = 1;
	resetTermios();
	signal(SIGINT, inthandler);
	int pid = fork();
	// printf("pid = %d ppid = %d", getpid(), getppid());
	if(pid == 0){
		// printf("I am child\n");
		kill(2, getppid());
		// system("gcc -o a.out bettershell.c");
		// system("./a.out");
		if(!gotinput){
			printPrompt();
			initTermios(0);
		}
		exit(0);
	}
	else{
		wait(0);
		// printf("I am parent\n");
		// exit(0);
	}
	return;
}
pid_t cpid;

void signalhandler(){


    p->pid=cpid;
    printf("%d\n",cpid);
    if(cpid){
        kill(cpid,SIGSTOP);
        printf("[%d] Stopped\n",cpid);
        job *tmgp=(job*)malloc(sizeof(job));
        tmgp->pid=cpid;
        strcpy(tmgp->work,p->work);
        tmgp->status=0;
        // printf("%d %s\n",p->pid,p->work);
        list *t=(list*)malloc(sizeof(list));
        t->t=tmgp;
        t->next=NULL;
        if(head==NULL){
            head=t;
        }
        else{
            list *tmp=head;
            while (tmp->next)
            {
                tmp=tmp->next;
            }
            tmp->next=t;
        }
    }
}

void stophandler(int signo){
	if(!gotinput){
		return;
	}
//	printf("z handling\n");
	sigstpreceived = 1;
	int pid = fork();
	resetTermios();
	// signal(SIGTSTP, stophandler);
	signal(SIGINT, inthandler);
//	if(pid == 0){
//		// printf("I am child\n");
//		kill(20, getppid());
//		system("gcc -o a.out bettershell.c");
//		system("./a.out");
//		// if(!gotinput){
//			// printf("cus ");
//			// printPrompt();
//			// initTermios(0);
//		// }
//		exit(0);
//	}
//	else{
//		wait(0);
//		// printf("I am parent\n");
//		// exit(0);
//	}
	 if(!pid){
	       //printf("pid = %d, pp = %d signo = %d\n", getpid(), getppid(), signo);
	       kill(2, getpid());
	      // execl("./a.out", "./a.out", NULL);
	       exit(0);
	      // system("gcc -o bettershell bettershell.c");
	      // execl("./bettershell", "./bettershell", NULL);
	       //system("./bettershell");
	       exit(0);
	 }
	 else{
	 	wait(0);
	      // uncomment this
	       system("./a.out");
	       exit(0);
	 }

	return;
}

char getch_(int echo){
  char ch;
  initTermios(echo);
//   signal(SIGINT, inthandler);
//   signal(SIGTSTP, stophandler);

  ch = getchar();
  resetTermios();

  if(ch == 4){ 
	write(1, "\n", 1);
  	exit(0);
  }
  return ch;
}



int main(){
	char input[1024];
	pwd = malloc(sizeof(char)*200);
	prompt[1024];
	char entryDir[100];
	getcwd(entryDir, 100);
	customPrompt = 0;

	for(int i=0; i<LINES; i++)
		historyArr[i] = NULL;
	signal(SIGINT, inthandler);
	signal(SIGTSTP, stophandler);
	while(1){
		gotinput = 0;
		char historyPath[512] = {0};
		strcpy(historyPath, entryDir);
		strcat(historyPath, "/.history");
		fillhistoryArr(historyPath);

		char* cmd;
		// if(sigstpreceived){
		// 	sigstpreceived = 0;
		// 	continue;
		// }
		char ch = 'r'; 
		int cnt = historyLen-1; 
		int currLen = strlen(historyArr[cnt]);
		strcpy(input, historyArr[cnt]);
		int arrow = 0;
		while(ch != '\n'){
			getcwd(pwd, 100);
			printPrompt();


			printf("%s", historyArr[cnt]); 
			// printf("recieving input... \n");
			ch = getch_(0);
			// printf("got 1 input... \n");
			while(ch != '\n'){
				if(ch == 127){ 
					if(currLen){
					input[currLen-1] = '\0';
					printf("\b \b");
					currLen--;
					}
				}
				// arrows detection in input
				else if(ch == 27){
					ch = getch_(0);
					if( ch == 91)
					{
						ch = getch_(0);
						// up arrow
						if(ch == 65){
							if(cnt > 0)
								cnt--;
							currLen = strlen(historyArr[cnt]);
							strcpy(input, historyArr[cnt]);
							printf("\33[2K\r");
							arrow = 1;
							break;
						}

						// down arrow
						else if(ch == 66){
							if(cnt < historyLen-1)
								cnt++;
							printf("\33[2K\r");
							currLen = strlen(historyArr[cnt]);
							strcpy(input, historyArr[cnt]);
							arrow = 1;
							break;
						}

					}
				}
				else{
					strncat(input, &ch, 1);
					putchar(ch);
					currLen++;
				}
				ch = getch_(0);
			}
			// if(arrow) continue;
		}

		if(ch == '\n'){
			strncat(input, &ch, 1);
			putchar(ch);
		}
		int len = strlen(input);
		input[len-1] = '\0';
		// printf("input = ,%s,", input);
		len = strlen(input);

		if(strlen(input) > 0){
			gotinput = 1;
			int fd = open(historyPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
			if(historyLen > 1){
				if(strcmp(input, historyArr[historyLen-2])){
					write(fd, input, strlen(input));
					write(fd, "\n", 1);
				}
			}
			else{
					write(fd, input, strlen(input));
					write(fd, "\n", 1);
			}
			close(fd);
		}
		else continue;
		
		if(strcmp(input, "p")==0){
			for(int i=0; i<100; i++){
				if(origPath[i]){
					printf("%s:", origPath[i]);
				}
			}
			continue;
		}
		if(strstr(input, "|")){ 
			int f = fork();
			if(f == 0){
				handlePipes(input);
			}
			else{
				wait(0);
				continue;
			}
		}

		bool redirectInput = false, redirectOutput = false;

		if(strstr(input, ">")) redirectOutput = true;
		if(strstr(input, "<")) redirectInput = true;

		if(strncmp(input, "PS1=", 4) == 0){
			customPrompt = 1;
			char newPrompt[1024] = {0};
			for(int j=5; j<len-1; j++){
				newPrompt[j-5] = input[j];
			}
			if(strcmp(newPrompt, "\\w$") == 0){ 
				customPrompt = 0;
				chdir(entryDir);
			}
			else strcpy(prompt, newPrompt);
			
			continue;
		}

		if(strncmp(input, "PATH=", 5) == 0){
			char newPath[100][100];
			memset(newPath, 0, sizeof (newPath));
			int pathlen = strlen(input);
			int pathEntries = 0;

			for(int j=5; j<pathlen; j++){
				if(input[j] == ':'){
					pathEntries++;
					continue;
				}
				int k = 0;

				while(j < pathlen && input[j] != ':'){
				    	newPath[pathEntries][k] = input[j];
					k++; j++; 
				}
				if(input[j] == ':') j--;
			}

			for(int i=0; i<pathEntries+1; i++){
				char ch = '/';
	                        if(newPath[i][strlen(newPath[i])-1] != '/') strncat(newPath[i], &ch, 1);
				strcpy(origPath[i], newPath[i]);
			}
			for(int i=pathEntries+1; i<100; i++)
				strcpy(origPath[i],  "");
			
			for(int i=0; i<100; i++){
				if(strlen(origPath[i]))
					printf("%s:", origPath[i]);
			}
			printf("\n");
			continue;
		}

		if(strcmp(input, "history") == 0){
			
			int fd = open(historyPath, O_RDONLY);
			char buf[1024] = {0};

			while(read(fd, buf, sizeof(buf))){
				printf("%s", buf);
			}
			close(fd);
			continue;
		}

		if(strcmp(input, "exit") == 0) exit(0);

		int k = 0;
		int iter = 0;

		// parsed command	
		cmd = parseCommand(input, &k, len);
		
		// parsed arguments
		ArgInfo tmpArg = parseArguments(cmd, input, k, len, redirectOutput, redirectInput);
	
		if(strcmp(cmd, "cd") == 0){
			customPrompt = 1;
			char buf[100];
			if(chdir(tmpArg.cd_folder) == -1){
				char fileError[100] = {0};
					strcpy(fileError, tmpArg.cd_folder);
					strcat(fileError, ": No such file or directory exists\n");
				write(1, fileError,100);
			}
			strcpy(prompt, getcwd(buf, 100));
			char* ch = "$ ";
			strncat(prompt, ch, 2);
			continue;
		}

		int pid = cpid = fork();
		if(sigstpreceived){
			printf("Signal received\n");
		}
		if(pid == 0){
				char* ret = execExists(cmd);
				signal(SIGTSTP, stophandler);
				if(ret == NULL){
				    printf("No path found for the given command\n");
					exit(0);
				}
				else{
					tmpArg.arguments[0] = ret;
					if(redirectInput){
						close(0);
					        int fd = open(tmpArg.streamFile, O_RDONLY);
						if(fd < 0){
							printf("Failed to open %s\n", tmpArg.arguments[1]);
						}
					}
					if(redirectOutput){
						close(1);
						int fd = open(tmpArg.streamFile, O_WRONLY| O_TRUNC | O_CREAT , 0777); 
						if(fd < 0){
							printf("Failed to open %s\n", tmpArg.arguments[1]);
							exit(0);
						}
					}
					execv(ret, tmpArg.arguments);
					printf("exec failed\n");
					exit(0);
				}
		}
		else{ 
			wait(0);
		}
	}

   return 0;
}
