/*
* This file will contain syscalls for handling a number
* of different exception scenarios.
*/

#include "../h/const.h"
#include "../h/types.h"
#include "../e/initial.e"
#include "../e/scheduler.e"
#include "../e/interrupts.e"
#include "../e/pcb.e"
#include "../e/asl.e"

#include "/usr/include/uarm/libuarm.h"

/* global variables taken from initial */
extern int procCount;
extern int sftBlkCount;
extern pcb_PTR currProc;
extern pcb_PTR readyQueue;
extern int semD[MAX_DEVICES];

/* globals taken from sceduler */
extern cpu_t TODStarted;
extern cpu_t currentTOD;

/* global function from interrupts*/
extern void copyState(state_PTR src, state_PTR dest);
extern void debug(int a, int b, int c, int d);
  /*just like in interrupts we will need to copy the state
   of the processor to handle what went wrong*/



/**************************************************************************************************/

void tlbManager() {
  debug(5, 14, 15, 4096);
  /* goes to passUpOrDie and sees if a sys5 exception vector
  has been found for the offending process */
  state_t *caller = TLB_OLDAREA; //ie. address of old area defined in uARMTypes.h;
  passUpOrDie(caller, TLBTRAP);

}

void pgmTrapHandler() {
  debug(1, 2, 3, 4);
   /* goes to passUpOrDie and sees if a sys5 exception vector
  has been found for the offending process */
  state_t *caller = PGMTRAP_OLDAREA; //ie. address of old area defined in uARMTypes.h;
  passUpOrDie(caller, PROGTRAP);
}

void syscallHandler(){
  debug(6, 7, 8, 9);
  //state_PTR caller = (state_PTR) SYSCALLOLDAREA;
  state_t *caller = SYSBK_OLDAREA; /*cpu state when syscallhandler was called*/
  /*int sysRequest = caller->s_r0; /*register 0 will contain an int that
                                represents which syscall we want to do*/
  int sysRequest = caller->a1; /*register 2 will contain an int that
                                represents which syscall we want to do*/
  //int callerStatus = caller -> s_status;
  int callerStatus = caller-> cpsr;
  if((sysRequest > 0) && (sysRequest < 9) && ((callerStatus & STATUS_SYS_MODE) != STATUS_SYS_MODE)){
    //    25:50                                this might be STATUS_USER_MODE^ (not sure which one)
    //set cause register to be priveledged instruction
    state_t *program = PGMTRAP_OLDAREA; //save program state to proper spot in table
    copyState(caller, program);
    pgmTrapHandler();
    
  }
  /*set the program counter to the next instruction */
  //caller->s_pc = caller->s_pc + 4;
  debug(sysRequest, 12, 13, 14);
  /* now switch on sysRequest to see what syscall to use*/
  switch(sysRequest){
  case CREATETHREAD:
    sysOne(caller);
  case TERMINATETHREAD:
    sysTwo();
  case VERHOGEN:
    sysThree(caller);
  case PASSEREN:
    sysFour(caller);
  case SPECTRAPVEC:
    sysFive(caller);
  case GETCPUTIME:
    sysSix(caller);
  case WAITCLOCK:
    sysSeven(caller);
  case WAITIO:
    sysEight(caller);
  default:
    passUpOrDie(caller);
    break;
  }
  /*it shouldnt make it here*/
  PANIC();
}

void sysOne(state_t* caller){
  /*This function will be sys1 call, for creating a process*/
  pcb_t *newProc = allocPcb();

  if(newProc == NULL){ // new process cannot be created because no more free Pcbs
    caller -> a1 = -1; // FAILURE.. error code of -1 is placed/returned in the caller’s A1
    LDST(caller); /*return CPU to caller */
  }
  ++procCount;

  /* make new process a child of current process */
  /*temp -> p_prnt = currProc;*/
  insertChild(currProc, newProc);
  
  /* add to the ready queue */
  insertProcQ(&readyQueue, newProc);

  /* copy CPU state in A2 to new process */
 // copyState((state_t*) caller -> s_r1, &(newProc -> p_s)); //the physical address of a processor state in A2
  copyState((state_t*) caller -> a2, &(newProc -> p_s));

  /* set return value */
  caller -> a1 = 0; // SUCCESS.. return the value 0 in the caller’s A1

  /* return CPU to caller */
  LDST(caller);

}

void sysTwo(){
  /* This function will be sys2, for terminating a process
   * Also recursively removes all children of head. */

  if(emptyChild(currProc)){
    /* current process has no children */
    outChild(currProc);
    freePcb(currProc);
    --procCount;
  } 
  else {
    /*children need to be recursively deleted*/
    sysTwoRecursion(currProc);
  }

  /* no current process anymore */
  currProc = NULL;
  /* call scheduler */
  scheduler();

}

void sysTwoRecursion(pcb_t *head){
  /*this function recursively deletes the children of head*/
  //there are children
  while(!emptyChild(head)){ 
      sysTwoRecursion(removeChild(head)); //remove all children
      }
      if(head -> p_semAdd != NULL){
        /* process blocked, remove self from ASL */
        int* sem = head -> p_semAdd;
        outBlocked(head);

        /* check if blocked on device */
        if(sem >= &(semD[0]) && sem <= &(semD[MAX_DEVICES-1])){ 
          sftBlkCount--;
        } 
        else {
         ++(*sem); /* increment semaphore */
        }

      } else if (head == currProc) {
        /* remove process from it's parent */
        outChild(currProc);
      } else {
        /* remove process from readyQueue */
        outProcQ(&readyQueue, head);
      }
      /* free self after we have no more children */
      freePcb(head);
      --procCount;
}

void sysThree(state_t* caller){
  /*This function will perform a v operation on a semaphore*/
  pcb_t *newProc = NULL;
  int *semV = caller->a1;
  ++semV; /* increment semaphore */
  if((semV) <= 0) {
    /* something is waiting on the semaphore */
    newProc = removeBlocked(semV);
    if(newProc != NULL){
      /* add it to the ready queue */
      insertProcQ(&readyQueue, newProc);
    } else {
      /* nothing was waiting on semaphore */
    }
  }
  /* always return control to caller */
  LDST(caller);
}

void sysFour(state_t* caller){
  /*This function will perform a P operation on a semaphore*/
  //int* semV = (int*) caller->s_r1;
  int* semV = caller->a1;
  --semV; /* decrement semaphore */
  if(semV < 0){
    /* something already has control of the semaphore */
    copyState(caller, &(currProc -> p_s));
    insertBlocked(semV, currProc);
    scheduler();
  }
  /* nothing had control of the sem, return control to caller */
  LDST(caller);

}

void sysFive(state_t* caller){
  /*this function returns an exception state vector*/
  //switch(caller->s_r1) {
    switch(caller->a1) {
    case TLBTRAP:
      if(currProc->tlbNew != NULL) {
        sysTwo(); /* already called for this type */
      }
      /* assign exception vector values */
      currProc->tlbNew = (state_PTR) caller->a4; //a4 instead of s_r3? pg.21 of jaeOS document
      currProc->tlbOld = (state_PTR) caller->a3; //a3 instead of s_r2?
      break;
      
    case PROGTRAP:
      if(currProc->pgmTrpNew != NULL) {
        sysTwo();
      }
      /* assign exception vector values */
      currProc->pgmTrpNew = (state_PTR) caller->a4;
      currProc->pgmTrpOld = (state_PTR) caller->a3;
      break;
      
    case SYSTRAP:
      if(currProc->sysNew != NULL) {
          sysTwo(); /* already called for this type */
      }
      /* assign exception vector values */
      currProc->sysNew = (state_PTR) caller->a4;
      currProc->sysOld = (state_PTR) caller->a3;
      break;
    }
   LDST(caller); //loadstate of the process that called the exception handler
}

int sysSix(state_t *caller){
  /*get cpu time, give it to the correct process, then return to what we were doing*/
  cpu_t now;
  //STCK(now); 
  now = getTODLO();//calculate what now is
  /*give the time to the current process*/
  currProc->cpu_time = currProc->cpu_time + (now - TODStarted);
  LDST(caller);//exit exception handler
}

void sysSeven(state_t *caller){
  /*Wait for the sys clock to get back from whatever it was doing
   its assistance is needed. */
  int* semV = (int*) &(semD[MAX_DEVICES-1]);
  --(*semV); /* decrement semaphore */
  insertBlocked(semV, currProc);
  copyState(caller, &(currProc -> p_s)); /* store state back in currProc*/
  ++sftBlkCount;
  scheduler();
}

void sysEight(state_t *caller){
  /*the program needs somthing from an io device to continue, in that event
   call this function. */
  debug(caller->a2, caller->a3, caller->a4, caller->a1);
  int index;
  int *sem;
  int lineNum = caller->a2; 
  int deviceNum = caller->a3; 
  int read = caller->a4; //a4? /* terminal read / write */
  
  if(lineNum < INT_DISK || lineNum > INT_TERMINAL){
    sysTwo(); /* illegal IO wait request */
  }
  
  /* compute which device */
  if(lineNum == INT_TERMINAL && read == TRUE){
    /* terminal read operation */
    index = DEV_PER_INT * (lineNum - DEVWOSEM + read) + deviceNum;
  } else {
    /* anything else */
    index = DEV_PER_INT * (lineNum - DEVWOSEM) + deviceNum;
  }
  sem = &(semD[index]);
  --(*sem);
  if(*sem < 0) {
    insertBlocked(sem, currProc);
    copyState(caller, &(currProc -> p_s));
    ++sftBlkCount;
    scheduler();
  }
  LDST(caller);
}

void passUpOrDie(state_PTR caller, int reason){
/*if an exception vector has been set for whatever unhandled operation that
  has been encountered, the error is passed up to the handler. */

/* if a sys5 has been called*/ 
  switch(reason){
    case SYSTRAP: /* syscall exception i.e. vector = 2 */
      if(currProc -> sysNew != NULL){
        /* yes a sys trap created */
        copyState(caller, currProc -> sysOld);
        LDST(currProc->sysNew);
      }
    break;
    case TLBTRAP: /* TLB trap exception i.e vector = 0 */
      if(currProc -> tlbNew != NULL){
        /* a tlb trap was created */
        copyState(caller, currProc -> tlbOld);
        LDST(currProc -> tlbNew);
      }
    break;
    case PROGTRAP: /* pgmTrp exception i.e vector = 1 */
      if(currProc -> pgmTrpNew != NULL){
        /* a pgm trap was created */
        copyState(caller, currProc -> pgmTrpOld);
        LDST(currProc -> pgmTrpNew);
      }
    break;
  }
  sysTwo(); /* if no vector defined, kill process */
}
