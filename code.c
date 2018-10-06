#include <stdio.h>
#include <stdlib.h>
#include <stdio_ext.h>  // for the locking macros
#include <string.h>
#include <fcntl.h> // for the open function
#include <unistd.h>   // for STDIN/STDOUT_FILENO

/* "An Interactive shell in C"
    
    Compile: gcc code.c 

 The program is divided into 3 modules: 1) READLINE 2) PARSER 3) EXECUTION 

 Module wise capabilites:
		1) a handy prompt [READLINE]
		2) eliminating trailing and in between whitespaces [PARSER]
		3) ability to retain whitespaces in special names [PARSER]
		4) redirecting I/O [EXECUTION]
		5) PATH search	[EXECUTION]
		6) shell buitlins such as "cd" and "exit" [EXECUTION]

 Note: The shell implemented here does not escape spaces using backward slashes. So if an argument contains whitespaces,
	   surround it with double quotes.
*/

char* ps1;
int token_count;

/* readline returns a pointer to the line read or NULL if the user simply presses enter */

char* readline(char* prompt){
	printf("%s:> ", prompt);

	char* str = NULL;
	size_t n = 0;

	ssize_t x = getline(&str, &n, stdin);  // reads a line including '\n' and returns no of characters read

	if(x==-1)
		perror("getline");  // perror: prints the error message as per the current value of errno

	else if (x==1){   // i.e. only '\n' is read
 		free(str); 
		return NULL;
	}

	return str;
}

/* parseTokens returns an array of strings, therefore the double "**" */

char** parseTokens(char* str){

	int i, j, len = strlen(str);
	
	len=len-2; // -2 to exclude '\n' and '\0'
	while(str[len]==' ') len--;  // removing trailing whitespaces
	str[++len] = ' '; // adding an extra space at the end to delilmit the last word with a space as well 

	token_count=0;
	
	char** arr = (char**)malloc(0);  // this is not same as making it point to NULL  
	int index = 0;  // to index into arr
	int lastIndex = 0;  // to remember the last occurence of a whitespace

	
	for(i=0;i<=len;i++){   // note: <=len and not just <len since we have already excluded the unnecessary
		if(str[i]=='\"'){   // remember to use the single quotes and not the double quotes
			int j = i+1;
			while(++i<len && str[i]!='\"'); // finding tokens enclosed withing double quotes
			if(i==len){
				puts("err: could not find the corresponding closing quote.");
				free(str);
				int k;                        // releasing back the resources
				for(k=0;k<index;k++)
					free(arr[k]);
				free(arr);
				exit(EXIT_FAILURE);
			}

			token_count++;
			arr = realloc(arr, sizeof(char*)*token_count);  // allocating more size as and when needed
			int sz = i-j; // no of character between the double quotes
			char* tk = (char*)malloc(sz);
			strncpy(tk, str+j, sz);
			arr[index++] = tk;
		}
		else if(str[i] == ' '){
			if(i>0 && str[i-1]==' '){
				lastIndex = i+1;
				continue;
			}
			token_count++;
			arr = realloc(arr, sizeof(char*)*token_count);  // remember to catch the value back in arr
			int sz = i - lastIndex;
			char* tk = (char*)malloc(sz + 1);
			strncpy(tk, str+lastIndex, sz);
			tk[sz] = '\0';  // null terminating each token (may not be essential but a good practise)
			lastIndex = i + 1; 
			arr[index++] = tk;
			
		}
	}
	
	return arr;
}

void changeDirectory(char* newDir){
	if(chdir(newDir)==-1)
		perror("chdir");
	ps1 = getcwd(NULL, 0);  // updating the value of the present working directory
}

int main(int argc, char const *argv[])
{
	__fsetlocking(stdin, FSETLOCKING_BYCALLER);  // removing implicit locking improves I/O speed
	__fsetlocking(stdout, FSETLOCKING_BYCALLER);  // safe since we are not multi-threading it

	ps1 = getcwd(NULL, 0);  // getcwd: "get current working directory"

	while(1){
		
		char* line = readline(ps1);  // module1: readline
		
		if(line==NULL)
			continue;
		
		
		int pipesPresent = 0;
		char** tokens = parseTokens(line); // module2: parser
		free(line);

		/* Checking for shell butilins (functions that change the state of the shell itself) */

		if(strcmp(tokens[0],"cd")==0){
			char* dir = (token_count==1) ? "/home/a-star/" : tokens[1];
			changeDirectory(dir); // defined above
			int i;
			for(i=0;i<token_count;i++)
				free(tokens[i]);
			free(tokens);
			continue;
		}

		if(strcmp(tokens[0], "exit")==0){
			int i;
			for(i=0;i<token_count;i++)
				free(tokens[i]);
			free(tokens);
			free(ps1);
			exit(EXIT_SUCCESS);
		}

		/* Resuming with module3: execute */

		pid_t rt = fork();
		
		
		if(rt == -1) perror("fork");   // error
		if(rt == 0)	 {       // inside the child process
			
			int new_out, new_in;  // for new file descriptors [needed for redirecting the I/O]
			int saved_in, saved_out;  // for saved file descriptors
			
			char* args[token_count+1]; // changing from char** to char*[] since that is the specified format for execvp
			int i=0;
			for(i=0;i<token_count;i++)
				args[i] = tokens[i];
			args[i] = NULL;  // very important to null terminate the array otherwise u will get a BAD address error
			
			int j, lindex = i; // lindex - last index of the redirection token

		
			if(i>2) {
				for(j=i-2; j>0; j--){
					if(pipesPresent >= 2)
						break;
					char* ct = args[j];  // ct- current token
					if(strlen(ct)==1){
						char pipe = ct[0];
						if(pipe=='>'){
							lindex = j;
							++pipesPresent;
							new_out = open(args[j+1], O_CREAT | O_RDWR, 0666);  // writing at times require reading privileges as well
							if(new_out == -1)                   // the 0 above is very important
								perror("write_open");
							saved_out = dup(STDOUT_FILENO);
							dup2(new_out, STDOUT_FILENO);
							close(new_out);  // note: closing them right away
						}

						else if(pipe=='<'){
							lindex = j;
							++pipesPresent;
							new_in = open(args[j+1], O_RDONLY);  // writing at times require reading privileges as well
							if(new_in == -1)
								perror("read_open");
							saved_in = dup(STDIN_FILENO);
							dup2(new_in, STDIN_FILENO);
							close(new_in);
						}
					}
				}
			}

			for(j=lindex;j<i;j++)
				args[j] = NULL;  // removing the unnecessary arguments

			if(execvp(tokens[0], args)==-1){
				perror("execvp");
				dup2(saved_in, STDIN_FILENO);  // restoring the original file descriptors
				dup2(saved_out, STDOUT_FILENO);  // in case execvp is not successful
				close(saved_in);
				close(saved_out);
			}
		}
		
		else {   // Inside the parent
			if(wait(NULL)==-1)  // waiting for the child to exit
				perror("wait");
			int i;
			for(i=0;i<token_count;i++)
				free(tokens[i]);
			free(tokens);
			}
		}

	return 0;
}

