#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char *argv[]) {
	int c;
	for (int i = 1; i < argc; i++) {
		for (c = 0; c < *argv[i]; c++){
			printf("Process <%d>\n", getpid());
			sleep(1);
		}
		if (i == argc-1) break;
		kill(getppid(), SIGUSR2);
		kill(getpid(), SIGSTOP);
	}
	kill(getppid(), SIGTSTP);
	return 0;
}
