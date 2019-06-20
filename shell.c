#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FALSE 0
#define TRUE 1

//Variable to store the commands and args
char *CMD[1024];
//Flags
int FIRST = TRUE;
int LAST = FALSE, REDIRECT = FALSE, EXIT = FALSE;
//Variable to store the file to redirect to
char REDIRECT_FILE[1024];

//Display prompt for use input
char *prompt(){
	char *cmd = NULL;
	size_t size;
	printf("$ ");
	if (getline(&cmd, &size, stdin) != -1){
		//remove newline
		cmd[strcspn(cmd, "\n")] = 0;
		return cmd;
	}
	perror("[*] Program terminating\n");
	return NULL;
}

//Executes the current command
int execute(int in){
	//Declare variables
	int pid;
	int fd[2];
	int status = 1;

	status = pipe(fd);
	//Failed pipe case
	if(status==-1){ perror("Pipe error\n"); return -1;}

	pid = fork();

	//Failed fork case
	if (pid==-1){ perror("Fork error\n"); return -1;}

	//Child process executes the command
	if (pid==0){

		//Redirection case has file_fd as output pipe
		if(REDIRECT){
			FILE *fp = fopen(REDIRECT_FILE, "w");
			if(fp==NULL){
				printf("Cannot open file %s\n", REDIRECT_FILE);
				return -1;
			}
			int file_fd = fileno(fp);

			dup2(in, STDIN_FILENO);
			dup2(file_fd, STDOUT_FILENO);
		}
		//If it's first command, there's no input pipe
		else if(FIRST && !LAST){
			dup2(fd[1], STDOUT_FILENO);
		}
		//If it's neither first nor last, it has input and output pipe
		else if (!LAST && !FIRST){
			dup2(in, STDIN_FILENO);
			dup2(fd[1], STDOUT_FILENO);
		}
		//If it's the last, there's only input pipe
		else if (!FIRST && LAST){
			dup2(in, STDIN_FILENO);
		}
		//Builtin cd command
		if(strcmp(CMD[0], "cd") == 0){
			chdir(CMD[1]);
		}
		else if(execvp(CMD[0], CMD) == -1){
			printf("Command \"%s\" cannot be executed ", CMD[0]);
			exit(1);
		}
		close(fd[0]);
	}
	//Parent
	else{
		//Wait for children to finish executing
		wait(&status);

		if(REDIRECT){
			close(fd[1]);
			return 0;
		}

		if(LAST){
			FILE *input = fdopen(fd[0], "r");
			char buf[1024];
			//close(in);
			close(fd[1]);
			while(1){
				fgets(buf, 1024, input);
				if (feof(input)) break;
				printf("%s", buf);
			}
			fclose(input);
			printf("\n");
			return 0;
		}
		close(fd[1]);
	}
	return fd[0];
}

//Reset all global variables
int reset(){
	//Rest first and last flag
	LAST = FALSE;
	FIRST = TRUE;
	//Clear the CMD
	for(int i=0; i<1024; i++) CMD[i] = '\0';
	//Clear the REDIRECT_FILE
	REDIRECT_FILE[0] = '\0';
	REDIRECT = FALSE;
	return 0;
}

//Parse user input to an array of commands
int parse_and_execute(char *input){

	//Check if it's our shell's built-in command to exit
	if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
		EXIT = TRUE;
		exit(0);
	}
	//Declare variables
	char *next = input;
	int IN_QUOTES = FALSE;
	//Length of one block in a command
	int len = 0;
	//Number of arguments in a command
	int n=0;
	//Keep track of the last pipe
	int last_cmd_fd;


	//Parse the commands and call execute()
	while(*next){
		//Skip beginning white spaces
		while(*next == ' ') next++;
		input = next;
		//Read in the next chunk
		while(IN_QUOTES || (*next != ' ' && *next != '\0')){
			//Case quotes
			if(*next == '\"' || *next == '\''){
				//Two cuts work here
				//if(!IN_QUOTES) *next = ' ';
				//len--;
				IN_QUOTES = !IN_QUOTES;
				//Original code
				if(!IN_QUOTES) input++;
				len--;
			}
			//Case not in quotes
			if(!IN_QUOTES) {
				//Execute the current command and go to next command
				if(*next =='|' || *next == '>'){
					printf("\n");
					if(*next == '>'){
						REDIRECT = TRUE;
						++next;
						//Skip all whitespaces
						while(*next == ' ') next++;
						strcpy(REDIRECT_FILE, next);
					}

					//Gets the fd of the last command executed
					last_cmd_fd = execute(last_cmd_fd);

					//Set FIRST to FALSE after first command
					if(FIRST) FIRST = FALSE;

					memset(CMD, 0, sizeof(CMD));
					n = 0;
					break;
				}
			}
			len++; next++;
		}
		//Copy the commands out and print it
		if(len!=0){
			//Set the arguments in the global variable
			CMD[n] = (char *)malloc(len+1);
			strncpy(CMD[n], input, len);
			CMD[n][len] = '\0';
			//Advance to the next argument
			n++;
		}
		//Proceed to the next chunk
		input = ++next;
		len = 0;
	}

	//Execute last command
	LAST = TRUE;
	//Don't need to execute if it's redirection
	if(!REDIRECT){
		last_cmd_fd = execute(last_cmd_fd);
	}
	//Reset all global variables
	reset();
	return 0;
}
//Runs the shell
int main(int argc, char* argv[]){
	printf("|***************************************|\n");
	printf("|                                       |\n");
	printf("| Welcome! Type any command to execute. |\n");
	printf("| Type exit/quit to exit the shell.     |\n");
	printf("|                                       |\n");
	printf(" ***************************************\n");
	while(TRUE){
		//Display user prompt to retrieve user input
		char *input = prompt();
		//Check if received a command
		if(input==NULL){
			perror("[*] No command specified, exiting...\n");
			exit(1);
		}
		parse_and_execute(input);
		if(EXIT) break;
	}
	exit(0);
}
