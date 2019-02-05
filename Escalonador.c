
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#define L1QUANTUM 1
#define L2QUANTUM 2
#define L3QUANTUM 4

//#define SIGHD SIGUSR1
//#define w4IO SIGUSR2

#define ever ;;

/*struct for sigaction signal handler takeover*/
struct sigaction psa;
struct timespec t;

/*process struct*/
typedef struct Processo{ 
	int pid;
	char name[20];
	char **argv;
	int *CPUBurstDur;
	int CurrLevel;
	int BurstDelta;
	int CurrBurst;
	int IOTimer;
}Process;

Process *CurrProc; //process currently being run
Process **l1, **l2, **l3, **IO; //F1, F2, F3 and IO process list
Process **Processes; //A list of all processes to be run
int NumProc;


/*sends first process in line to the back, shifting everyone else to the left*/
void move(Process **l) { 
	if (!l[1]) return; //not enough processes to shuffle
	for (int k = 1; k < NumProc; k++){
		Process *aux = l[k-1];
		l[k-1] = l[k];
		l[k] = aux;
	}
}

/*remove the first processes, shifting everyone else to the left*/
void removep(Process **l) { 
	for (int k = 1; k < NumProc; k++) {
		l[k-1] = l[k];
	}
	l[NumProc-1] = NULL;
}

/*add a process to the first available spot*/
void add(Process **l, Process *p) { 
	for (int c = 0; c < NumProc; c++) {
		if (!l[c]){
			l[c] = p; break;
		}
	}
}

/*not really a signal handler, but its manually called when a process has ran out its IOTimer*/
void IOEndHandler() { 
	Process *p = IO[0];
	printf("Process <%d> Finished IO\n", p->pid);
	Process **l, **lm;
	removep(IO);
	switch (p->CurrLevel) {
		case 1: l=l1; lm=l1; break;
		case 2: l=l2; lm=l1; break;
		case 3: l=l3; lm=l2; break;
	}
	if (p->BurstDelta<0) {
		if (p->CurrLevel != 1) p->CurrLevel--;
		add(lm, p); //add to lower level (same level if already lowest)
	}
	else if (p->BurstDelta == 0)
		add(l, p);
}

/*runs through IO process list, refreshing the IOTimers and checking to see if any ran out*/
void checkIO() {
	for (int c = 0; c < NumProc; c++) {
		if (IO[c]) {
			IO[c]->IOTimer--;
			if (IO[c]->IOTimer == 0)
				IOEndHandler();
		}
	}
}

/*fired when a process desires to start IO*/
void IOBeginHandler(int Signal) {
	printf("Process <%d> Requested IO\n", CurrProc->pid); 
	switch(CurrProc->CurrLevel) {
		case 1: removep(l1); break; //remove process from current level
		case 2: removep(l2); break;
		case 3: removep(l3); break;
	}
	CurrProc->IOTimer = 3;
	add(IO, CurrProc); //add current process to Performing IO list
}

/*fires when a process has ended*/
void PTHandler (int Signal) {
	printf("Process <%d> Terminated\n", CurrProc->pid);
	Process *p = CurrProc;
	removep(Processes);
	switch (p->CurrLevel) {
		case 1: removep(l1); break;
		case 2: removep(l2); break;
		case 3: removep(l3); break;
	}
}

/*this function calculates what will happen to the process and runs it*/
void runProcess(int quantum, int level) {
	t.tv_sec = 0;
	t.tv_nsec = 888888888;
	Process **l, **lp;
	int c=0, status;
	switch (level) { //get current level process list
		case 1: l=l1; lp=l2; break;
		case 2: l=l2; lp=l3; break;
		case 3: l=l3; lp=l3; break;
	}

	while (CurrProc->CPUBurstDur[c]<=0) c++; //find next burst

	CurrProc->CurrBurst = CurrProc->CPUBurstDur[c];
	CurrProc->CPUBurstDur[c] -= quantum;
	CurrProc->BurstDelta = CurrProc->CurrBurst-quantum; //if positive, theres still time left. If 0, ran for exactly the quantum, if negative, it didnt go over the quantum
	if (CurrProc->CurrBurst>quantum) {
		for (int j = 0; j < quantum; j++) {
			kill(CurrProc->pid, SIGCONT);
			sleep(1);
			kill(CurrProc->pid, SIGSTOP);
			checkIO();
		}
		if(level != 3) { //increase process level, if it isnt already the highest
			removep(l);
			add(lp, CurrProc);
			CurrProc->CurrLevel++;
		}
		else
			move(l3); //sends processes to the back of the line
	}
	else {
		kill(CurrProc->pid, SIGCONT);
		for (int j = 0; j < CurrProc->CurrBurst; j++) {
			nanosleep(&t, NULL);
			checkIO();
		}
		pause();
		//waitpid(CurrProc->pid,&status,WUNTRACED); //wait for signal sync	
	}
}

/*reads parameters from stdin*/
void interpretador(){
	char trash[5];	
	char aux[20];

	/*INTERPRETADOR*/
	printf("How many processes will be run?\n");
	scanf("%d", &NumProc);
	l1 = malloc(NumProc*sizeof(Process*));
	l2 = malloc(NumProc*sizeof(Process*));
	l3 = malloc(NumProc*sizeof(Process*));
	IO = malloc(NumProc*sizeof(Process*));
	Processes = malloc(NumProc*sizeof(Process*));
	for (int c = 0; c<NumProc; c++) {
		Process *p = malloc(sizeof(Process));
		printf("Type in the process name and CPU Bursts\n (Usage: >exec p_name ( 2, 5, 10)\n>");
		scanf("%s %s %[^\n]",trash, p->name,aux);
		char s[]=" , ";
		char *token;
		char **aux2 = malloc(strlen(aux) * sizeof(char*));
		token = strtok(aux, s);
		if(strcmp(token, "(") == 0)
			token = strtok(NULL, s);
		int i = 0;
		while (token) {
			aux2[i] = token;
			token = strtok(NULL, s);
			i++;
		}
		p->CPUBurstDur = malloc(i * sizeof(int));
		p->argv = malloc((i+2) * sizeof(char*));
		for (int k = 0; k < i; k++) {
			p->CPUBurstDur[k] = atoi(aux2[k]);
			p->argv[k+1] = (char *)&p->CPUBurstDur[k];
		}
		p->argv[0] = p->name;
		p->argv[i+1] = NULL;
		Processes[c] = p;
	}
}

/*sets current process and calls runProcess with the appropriate quantum*/
void escalonador() {
	int k;
	for (k = 0; k < NumProc; k++) {
		Process *p = Processes[k];
		if ((p->pid=fork()) == 0) {
			execv((const char*)p->argv[0], (char * const*)p->argv);
			exit(1);
		}
		else
			kill(p->pid, SIGSTOP); //halts all children before running
	}
	
	psa.sa_handler = IOBeginHandler;
	sigaction(SIGUSR2, &psa, NULL);
	signal(SIGTSTP, PTHandler);
	for (k = 0; k < NumProc; k++) {
		Process *p = Processes[k];
		p->CurrLevel = 1;
		l1[k] = p; //sends all processes to level 1
	}
	
	for (ever) {
		while (l1[0]) { //l1 is not empty
			CurrProc = l1[0];
			runProcess(L1QUANTUM, 1);
		}
		while (l2[0]) { //l2 is not empty
			CurrProc = l2[0];
			runProcess(L2QUANTUM, 2);
		}
		while (l3[0]) { //l3 is not empty
			CurrProc = l3[0];
			runProcess(L3QUANTUM, 3);	
		}
		if (!Processes[0]) {
			printf("Nothing left to run, exiting...\n");
			break;
		}
		if (!l1[0] && !l2[0] && !l3[0]) { //all running IO
			sleep(1);			
			checkIO();	
		}
	}
}

int main(void) {
	interpretador(); //calls function to get process parameters
	escalonador(); //begin 
	return 0;
}
