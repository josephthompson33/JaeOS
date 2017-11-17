/* This file maintains a round-robin process scheduler and performs 
 * deadlock detection.
 */ 

#include "../h/const.h"
#include "../h/types.h"
#include "../e/initial.e"
#include "../e/pcb.e"

#include "/usr/include/uarm/libuarm.h"

/* define external global variables that the scheduler uses */
extern int procCount;
extern int sftBlkCount;
extern pcb_PTR currProc;
extern pcb_PTR readyQueue;
extern int semD[MAX_DEVICES];

/* global vars for maintaining cpu time usage */
cpu_t TODStarted;
cpu_t currentTOD;
extern void debug(int a, int b, int c, int d);

/********************** SCHEDULER FUNCTION *********************************/
/* implementation of a scheduler that uses a round-robin scheduling 
	algorithm. */
void scheduler() {

	/* was someone just running? */
	/* this means a process was running and was then blocked 
	 * or returned to readyQ for some reason */
	if(currProc != NULL){
		/* save how much time current process used on CPU */
		/* subtract current time from global start time to get this ^ */
		currentTOD = getTODLO();
		currProc->cpu_time = (currProc->cpu_time)
							+ (currentTOD - TODStarted);
	}

	/* start next process in the Proccess Queue assuming readyQueue is
		not empty */
	if (!emptyProcQ(readyQueue)) { // if ready queue is not empty
		/* start the next process in the ready queue */
		currProc = removeProcQ(&readyQueue);
		TODStarted = getTODLO(); 
		setTIMER(QUANTUM);
		LDST(&(currProc->p_s));
	} 
	else { // if the ready queue is empty
			currProc = NULL; // no running process

			/* if Proccess Count == 0, then HALT the program */
			// i.e. all processed finished & there are no more processes
			if (procCount == 0) {
				HALT();
			}

			/* deadlock when procCount > 0 and sftBlkCount == 0. If this 
			happens, we panic */
			if(procCount > 0 && sftBlkCount == 0) {
				PANIC();
			}

			/* If procCount > 0 and sftBlkCount > 0 enter a Wait state.
			i.e. the processor is waiting for an interrupt to occur */
			if(procCount > 0 && sftBlkCount > 0) {
				// 16:00
				// set some local variable interrupts to be enabled and in kernel mode
				// state_t * status = ALLOFF | STATUS_SYS_MODE | INTSDISABLED
				// set status to be current status
				// setSTATUS(status); ?

				// set control register to ensure VM is off
				WAIT(); 
			}
		}

}

