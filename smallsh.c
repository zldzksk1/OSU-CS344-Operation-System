#define _GNU_SOURCE					
#define _POSIX_C_SOURCE 200112L		
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>


#define INPUT_MAX 2048								//define the max input length and max number of arguments 
#define ARG_MAX 512

struct sigaction SIGINT_action = { 0 };				//global SIGINT var to use in child process

bool bgIgnore = false;								//fg only mode trigger
bool bgRun = false;									//bg mode trigger

int PIDcount = 0;									//count number of bg running
int PIDarr[512] = { 0 };							//store bg pid

int childExitMEthod = 0;							//store exit status
int count;											//Counts number of arguments 

void getInput(char* input[]);						//function to get user input
void checkPID(char* input[], int count);			//function to see wheather user entered '$$'
char* updatePID(char* input);						//replace $$ -> process id
void changDir(char* input);							//change director function when users input 'cd'
void getPWD();										//function to display current working directory
void callCMD(char* input[]);						//execute entered command
void exitHandler();									//function to check the child process status
void resetArgument(char* input[]);					//empty the user arguments
void SIGTSTPhandler(int signo);						//SIGTSTP handler function.
void pushPIDarr(int pid);							//function to track the bg process
void rmPIDarr(int pid);								//function to track the bg process
void killALLbg();									//function to kill all remained bg processes upon exit
void procRedir(char* input, int type);				//function to process the file redirection

int main() {
	char* argument[ARG_MAX];						//declare the argument that accpets 512 arguments
	bool run = true;
	
					
	SIGINT_action.sa_handler = SIG_IGN;				//ctrl^C and ctrl^Z handler, refer the code from module 5 example (5_3_signal)
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	struct sigaction SIGTSTP_action = { 0 };		//SIGTESTP var
	SIGTSTP_action.sa_handler = SIGTSTPhandler;		//assign customized behavior
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	do
	{
		printf(": ");									//print ptompter
		getInput(argument);								//get user input

		if (argument[0] == NULL)						//when user entered nothing, do nothing
		{
			fflush(stdout);
		}
		else if (argument[0][0] == '#')					//when user entered comments, do nothing
		{
			fflush(stdout);
		}
		else if (strcmp(argument[0], "exit") == 0)		//when user entered exit, run variable to false, ends the while loop
		{
			killALLbg();								//kill all bg processes which are not killed 
			run = false;
		}
		else if (strcmp(argument[0], "cd") == 0)		//when user entered cd, change directory
		{
			changDir(argument[1]);						//pass only the first argument which is the destnation, ignore remained arguments
			fflush(stdout);			
		}
		else if (strcmp(argument[0], "pwd") == 0)		//when user entered pwd, call getPWD() to display current working directory
		{
			getPWD();
			fflush(stdout);
		}
		else if (strcmp(argument[0], "status") == 0)	//when user entered status, print latest exit status
		{
			printf("exit value %d\n", childExitMEthod);
			fflush(stdout);
		}
		else                                            //execute commandline
		{
			callCMD(argument);
			fflush(stdout);
		}

		exitHandler();									//check process status
		resetArgument(argument);						//reset the arguments array
	} while (run);


	return 0;
}

/***********************************************************************************************************************************
**
**		getInput()
**		get an input from users and manuplate it for later function process 
**      referred the code from https://stackoverflow.com/questions/2693776/removing-trailing-newline-character-from-fgets-input
************************************************************************************************************************************/
void getInput(char* argument[])
{
	fflush(stdout);															//clear stdout
	bgRun = false;															//reset the bgRun trigger
	char input[INPUT_MAX];													//declare input var
	fgets(input, INPUT_MAX, stdin);											//get an input
	input[strcspn(input, "\n")] = 0;										//remove endline 
	char* token;															
	token = strtok(input, " ");
	while (token != NULL)													//save input on argument array;
	{
		argument[count] = token;												
		count++;															//counts number of arguments
		token = strtok(NULL, " ");											//get the next input
	}

	if (argument[0] != NULL)												//if argument == 0, user entered nothing
	{																		//else users enter values
		if (strcmp(argument[count - 1], "&") == 0)							//check whether user entered &
		{
			if (bgIgnore == true)											//if program if the fg only mode
			{
				argument[count - 1] = NULL;									//ignore the & symbol
				count--;													//decrease the count var
			}
			else                                                            //if program is not fg only mode
			{
				bgRun = true;												//bgRun on
				argument[count - 1] = NULL;									//delete & symbol
				count--;													//decrease the count var
			}
		}

		argument[count] = NULL;												//set the last as null to prevent seg fault
		checkPID(argument, count);											// call checkPID func to check $$ symbol
	}
}

/***********************************************************************************************************************************
**
**		checkPID()
**		Function to see whether users entered $$ symbol
**		If the arguments has $$ symbol, replace it with process ID number with updatePID
************************************************************************************************************************************/
void checkPID(char* input[], int count)
{
	//char buffer[INPUT_MAX];													//temp buffer to hold PID
	int i, j;
	for ( i = 0; i < count; i++)												//loop through the argument array
	{
		for ( j = 0; j < strlen(input[i]); j++)
		{
			if ((input[i][j] == '$') && (input[i][j + 1] == '$'))				//if a string include $$
			{
				j++;															//skip one char
				strcpy(input[i], updatePID(input[i]));							//update $$ with PID number
			}
		}
	}
}

/***********************************************************************************************************************************
**
**		checkPID()
**		Function to see whether users entered $$ symbol
**		If the arguments has $$ symbol, replace it with process ID number with updatePID
************************************************************************************************************************************/
char* updatePID(char* input)
{
	fflush(stdout);																//clear the stdout in case
	char returnChar[INPUT_MAX] = "";											//buffer to save the PID num
	char *charPtr = returnChar;													//set pointer to return the variable
	for (int i = 0; i < strlen(input); i++)
	{
		if ((input[i] == '$') && (input[i + 1] == '$'))							//if '$$' is found in a string
		{
			char buffer[INPUT_MAX];												//temp butter to save the PID
			sprintf(buffer, "%d", getpid());									//getPID and save it as string buffer
			strcat(returnChar, buffer);											//append it to returnChar var
			i++;																//skip one char
		}
		else
		{
			strncat(returnChar, &input[i], 1);									//else append single char to returnChar var
		}
	}

	return charPtr;																//return the full string
}

/***********************************************************************************************************************************
**
**		changDir()
**		Function to change the dicrectory
**		
************************************************************************************************************************************/
void changDir(char* input)
{
	if (input == NULL)													//if user entered cd with no destination 
	{
		chdir(getenv("HOME"));											//go to home directory
	}
	else                                                                //if user entered cd with a destination 
	{
		if (chdir(input) == -1)											//there is not such file or directory
		{
			printf("%s: no such file or directory\n", input);			//Prompt error msg
			exit(EXIT_FAILURE);											//exit
			fflush(stdout);												//clear the stdout
		}
		chdir(getcwd(input, sizeof(input)));							//if find directory, then chnage directory
	}
}

/***********************************************************************************************************************************
**
**		getPWD()
**		Function to get the current dicrectory and display it to users
**		referred the code from https://man7.org/linux/man-pages/man3/getcwd.3.html
************************************************************************************************************************************/
void getPWD()
{
	char path[INPUT_MAX];						//declare var to save the current directory as string
	getcwd(path, sizeof(path));					//get directory info
	printf("%s\n", path);						//display it
	fflush(stdout);								//clear the output
}


/***********************************************************************************************************************************
**
**		callCMD()
**		Function to execute the user inputs
**		referred a code : module 5 excample code which is 5_4_sortViaFiles.c
**						: module 4 example: 4_2_waitpid_exit.c			
************************************************************************************************************************************/
void callCMD(char* input[])
{
	pid_t spwanPID = fork();																//call fork()
	char inputFile[INPUT_MAX], outputFile[INPUT_MAX];										//declare all variables
	bool inputCheck = false, outputCheck = false;
	bool inputRedir = false, outputRedir = false;

	switch (spwanPID)																		
	{
		case -1:
			perror("fork() failed\n");
			fflush(stdout);
			break;
		case 0:																				//when child process is called
			fflush(stdout);																	//clear the stdout

			if (bgRun == false || bgIgnore == true) 
			{																				//if the program is running on fg, accpet the SIGINT
				SIGINT_action.sa_handler = SIG_DFL;											
				sigaction(SIGINT, &SIGINT_action, NULL);
			}

			for (int i = 0; input[i] != NULL; i++)											//inputfile and outputfile handling
			{
				if (strcmp(input[i], "<") == 0)												// < read file symbol found
				{														
					inputCheck = true;														//turn on the input process trigger
					input[i] = NULL;														//remove <
					if (input[i + 1] == NULL)
					{
						inputRedir = true;													//if inputfile name is not specified, set inputRedir true
					}
					strcpy(inputFile, input[i + 1]);										//save the inputfile name														
				}

				else if (strcmp(input[i], ">") == 0)										// > output file symbol found
				{
					outputCheck = true;														//turn on the output process trigger	
					input[i] = NULL;														//remove >
					if (input[i + 1] == NULL)
					{
						outputRedir = true;													//if outputfile name is not specified, set outputRedir true
					}
					strcpy(outputFile, input[i + 1]);										//save the outputfile name
				}
			}

			if (bgRun == true)																//if process run in background
			{
				if (inputRedir == true)														//and inputfile redirection is not specified			
				{
					procRedir(inputFile, 3);												//process the inputfile as dev/null/
				}

				if (outputRedir == true)													//and outputfile redirection is not specified
				{
					procRedir(outputFile, 4);												//process the outputfile as dev/null/
				}
			}


			if (bgRun == false)																//process run on foreground
			{
				if (inputCheck)																//process the inputfile
				{
					procRedir(inputFile, 1);
				}

				if (outputCheck)															//process the output file
				{
					procRedir(outputFile, 2);
				}
			}


			if (execvp(input[0], input) == -1)												//run command
			{
				perror(input[0]);															//if can't run the command, inform users an error msg
				exit(1);																	//exit with code 1
			}
			break;
	default:																				//parents process
		if (bgRun == true && bgIgnore == false)												//when bg mode
		{
			waitpid(spwanPID, &childExitMEthod, WNOHANG);									//call child process, and dont wait it with WNOHANG
			printf("background pid is %d\n", spwanPID);										//inform the childprocess PID
			pushPIDarr(spwanPID);															//save the bg PID on PID array
			fflush(stdout);																	//clear the std out

		}
		else if (bgRun == false || bgIgnore == true)										//when it is fg mode
		{
			waitpid(spwanPID, &childExitMEthod, 0);											//call child process and wait until it ends the its process
			if (!WIFEXITED(childExitMEthod))										
			{																				//if it is terminated by signal
				printf("terminated by signal %d\n", WTERMSIG(childExitMEthod));				//Inform user the program is terminated
				fflush(stdout);																//clear the stdout
			}
			else																			//when program is exited
			{
				childExitMEthod = WEXITSTATUS(childExitMEthod);								//save the exit status to global var to use it later
			}
		}
		exitHandler();
		break;
	}
}

/***********************************************************************************************************************************
**
**		exitHandler()
**		Function to see any child process is done
**		referred a code : https://linux.die.net/man/3/waitpid
**						  https://stackoverflow.com/questions/19823489/how-to-know-if-a-process-which-run-in-the-background-finished-or-not
************************************************************************************************************************************/
void exitHandler()
{
	pid_t spwanedPID;											
	spwanedPID = waitpid(-1, &childExitMEthod, WNOHANG);			//Check any child process is done with their process

	while (spwanedPID > 0)											//if yes
	{																//check the exit Method
		if (WIFEXITED(childExitMEthod))								//when it is exited by program
		{
			printf("background pid %d is done: exit value %d\n", spwanedPID, WEXITSTATUS(childExitMEthod));
			rmPIDarr(spwanedPID);									//remove the exited PID from PID array
			fflush(stdout);
		}
		else														//when it is terminated 
		{
			printf("background pid %d is done: terminated by signal %d\n", spwanedPID, WTERMSIG(childExitMEthod));
			rmPIDarr(spwanedPID);									//remove the terminated PID from PID array
			fflush(stdout);
		}
		spwanedPID = waitpid(-1, &childExitMEthod, WNOHANG);		//check other child process if it has
	}	
}

/***********************************************************************************************************************************
**
**		resetArgument()
**		reset the user arguments for the new inputs
**
************************************************************************************************************************************/
void resetArgument(char* input[])									//loop it as much as the argument count variable
{
	for (int i = 0; i < count; i++) 
	{
		input[i] = NULL;											//assign null
	}
	count = 0;														//reset the arguments count to 0
}

/***********************************************************************************************************************************
**
**		SIGTSTPhandler()
**		Function to set the behavior of the SIGTSTP
**		referred the code from module 5 example code 5_3_singal_2.c-2
************************************************************************************************************************************/
void SIGTSTPhandler(int signo)
{
	if (bgIgnore == false)														//if current is not fg only mode
	{
		char* message = "Entering foreground-only mode (& is now ignored)\n";	//prompt the msg
		write(1, message, 50);
		fflush(stdout);															//clear stdout
		bgIgnore = true;														//fb only mode trigger set to true
	}
	else																		//if current is fg only mode
	{
		char* message = "Exiting foreground-only mode\n";						//prompt the msg
		write(1, message, 30);
		fflush(stdout);															//clear stdout
		bgIgnore = false;														//fb only mode trigger set to false
	}
	printf(": ");																//print ":" since RESART FLAG is used
	fflush(stdout);																//clear the stdout
}

/***********************************************************************************************************************************
**
**		pushPIDarr()
**		save bg PID in PIDarr to track bg processes
**
************************************************************************************************************************************/
void pushPIDarr(int pid)
{
	PIDarr[PIDcount] = pid;					//save passed PID into array
	PIDcount++;								//increase the PIDcount (indication of number of value in the array
}

/***********************************************************************************************************************************
**
**		rmPIDarr()
**		remove the PID which is saved PIDarr which is hold bg process id 
**		
************************************************************************************************************************************/
void rmPIDarr(int pid)
{
	for (int i = 0; i < PIDcount; i++)			//loop through the array
	{
		if (PIDarr[i] == pid)					//if PID is found
		{
			PIDarr[i] = 0;						//delete
			if (i == PIDcount - 1)				//if I indicate the last element of the array
			{
				PIDcount--;						//simply decrease the PIDcount
			}
			else                                //if PID is removed from beging/meddile of the array 
			{									//bring values foward
				int j;
				for(j = i; j < PIDcount - 1; j++)
				{
					PIDarr[j] = PIDarr[j + 1];
				}
				PIDarr[j] = 0;					//last value is set to 0
				PIDcount--;						//then decrease the PIDcount;
			}
		}
	}
}

/***********************************************************************************************************************************
**
**		killALLbg()
**		Kill all bgprocess when a user wnats to exit if the bg process is alive
**		reffered code from https://www.geeksforgeeks.org/signals-c-language/
**						   https://piazza.com/class/kjc3320l16c2f1?cid=366
************************************************************************************************************************************/
void killALLbg()
{
	for (int i = 0; i < PIDcount; i++)							//check the array if it holds any the running bg pid 
	{
		if (PIDarr[i] != 0)										//if it has
		{
			int stat;
			pid_t pid = PIDarr[i];
			kill(pid, SIGKILL);									//kill the pid
			wait(&stat);
			if (WIFSIGNALED(stat))
			{
				printf("background process %d is terminated to exit safely\n", pid);	//prompt the result
				fflush(stdout);
			}
		}
	}
}


/***********************************************************************************************************************************
**
**		procRedir()
**		Function to process the redirection
**		1 = foreground/input specified     | 2 = foreground/output specified 
**		3 = background/input not specified | 4 = background/output not specified
************************************************************************************************************************************/
void procRedir(char* input, int type)
{
	int result, inputTarget, outputTarget;											//initialize all variables
	if (type == 1)														
	{
		inputTarget = open(input, O_RDONLY);										//read only mode
		if (inputTarget == -1)														//when can't open the file
		{
			printf("Can't open %s for input\n", input);								//inform users an error msg
			fflush(stdout);															//clear the stdout and exit with code 1
			exit(1);
		}
		result = dup2(inputTarget, 0);												//dup the input target
		if (result == -1) {															//if you can't dup, inform an error msg
			perror("source dup2()");
			fflush(stdout);
			exit(2);																//exit with code 2
		}
		fcntl(inputTarget, F_SETFD, FD_CLOEXEC);									//close the file
	}
	else if (type == 2)																
	{
		outputTarget = open(input, O_WRONLY | O_CREAT | O_TRUNC, 0644);				//wrting, creat, trunc mode to meet the assignment criteria
		if (outputTarget == -1)														//when can't open the file
		{
			printf("Can't open %s for output\n", input);							//inform users an error msg
			fflush(stdout);															//clear the stdout and exit with code 1
			exit(1);
		}
		result = dup2(outputTarget, 1);												//dup the input target
		if (result == -1) {															//if you can't dup, inform an error msg
			perror("source dup2()");
			fflush(stdout);
			exit(2);																//exit with code 2
		}
		fcntl(outputTarget, F_SETFD, FD_CLOEXEC);									//close the file
	}

	else if (type == 3)																//when it is input and redirecion isn't specified by a user
	{
		inputTarget = open("/dev/null", O_RDONLY);									//set redirection to /dev/null read only mode 
		if (inputTarget == -1)														//when can't open the file
		{
			printf("Can't open /dev/null for input\n");								//inform users an error msg
			fflush(stdout);															//clear the stdout and exit with code 1
			exit(1);
		}
		result = dup2(inputTarget, 0);												//dup the input target
		if (result == -1) {															//if you can't dup, inform an error msg
			perror("source dup2()");
			fflush(stdout);
			exit(2);																//exit with code 2
		}
		fcntl(inputTarget, F_SETFD, FD_CLOEXEC);									//close the file
	}

	else if (type == 4)																//when it is output and redirecion isn't specified by a user
	{
		outputTarget = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);		//set redirection to /dev/null wrting, creat, trunc mode to meet the assignment criteria
		if (outputTarget == -1)														//when can't open the file
		{
			printf("Can't open /dev/null for output\n");							//inform users an error msg
			fflush(stdout);															//clear the stdout and exit with code 1
			exit(1);
		}
		result = dup2(outputTarget, 1);												//dup the input target
		if (result == -1) {															//if you can't dup, inform an error msg
			perror("source dup2()");
			fflush(stdout);
			exit(2);																//exit with code 2
		}
		fcntl(outputTarget, F_SETFD, FD_CLOEXEC);									//close the file
	}
}
