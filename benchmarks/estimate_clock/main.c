#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include "utils.h"
#include "timer.h"
#include <string.h>



int main(int argc, char**argv){
	int status, local_pid, i=0;
	unsigned long long exec_time;
	unsigned long long tot_time;
	unsigned long long seconds;
	
	if(argc!=2){
		printf("usage: ./a.out <seconds>\n");
		exit(0);
	}
	seconds=atoi(argv[1]);
	clock_timer_start(exec_time);
	sleep(seconds);

	tot_time = clock_timer_value(exec_time);
	printf("Timer  (clocks): %llu\n",tot_time);
	printf("Timer  (time): %llu\n",seconds);
	printf("Clocks per us: %f\n",tot_time/1000.0/1000.0/seconds);
		
	return 0;
}
