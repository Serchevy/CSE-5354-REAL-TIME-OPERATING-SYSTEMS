// Kernel functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "mm.h"
#include "kernel.h"
#include "uart0.h"
#include "asp.h"
#include "shell.h"

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// mutex
typedef struct _mutex
{
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
typedef struct _semaphore
{
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];

// Service Calls
#define RTOS_START      0
#define TASK_SWITCH     1
#define TASK_SLEEP      2
#define TASK_LOCK       3
#define TASK_UNLOCK     4
#define TASK_WAIT       5
#define TASK_POST       6
#define TASK_MALLOC     7

#define SHELL_REBOOT    8
#define SHELL_PS        9
#define SHELL_IPCS      10
#define SHELL_KILL      11
#define SHELL_PKILL     12
#define SHELL_PI        13
#define SHELL_PREEMPT   14
#define SHELL_SCHED     15
#define SHELL_PIDOF     16
#define SHELL_RUN_PROC  17
#define SHELL_MEMINFO   18

#define RESTART_THREAD  19
#define SET_PRIORITY    20

// 1ms interrupt with SysTick
#define RELOAD_1MS      39999       // 1ms Interrupt for 40 MHz System Clock

#define FIX_PCT         10e3
#define SYS_CLK         40e6

// task states
#define STATE_INVALID           0 // no task
#define STATE_STOPPED           1 // stopped, all memory freed
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_MUTEX     4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_SEMAPHORE 5 // has run, but now blocked by semaphore

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

//Ping-Pong flag
bool ping = true;


// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = true;           // preemption (true) or cooperative (false)

// tcb
#define NUM_PRIORITIES   16
struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread (add of task fn)
    void *spInit;                  // original top of stack
    void *sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
    uint32_t size;                 // Size of task (needed for restarThread)
    uint32_t timeElpA;             // Used for CPU%
    uint32_t timeElpB;             // Used for CPU%
} tcb[MAX_TASKS];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex)
{
    bool ok = (mutex < MAX_MUTEXES);
    if (ok)
    {
        mutexes[mutex].lock = false;
        mutexes[mutex].lockedBy = 0;
    }
    return ok;
}

bool initSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    {
        semaphores[semaphore].count = count;
    }
    return ok;
}

// REQUIRED: initialize systick for 1ms system timer
void initRtos(void)
{
    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
    }

    // SysTick Config
    NVIC_ST_RELOAD_R = RELOAD_1MS;
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;

    static uint8_t lastTaskRan[NUM_PRIORITIES] = {0};           // Keep track of last task that was ran at x priority level
    uint8_t highPrio = NUM_PRIORITIES;
    uint8_t i;

    if(priorityScheduler) {
        for(i=0; i<taskCount; i++) {
            if(tcb[i].state == STATE_READY && tcb[i].currentPriority < highPrio)
                highPrio = tcb[i].currentPriority;              // find a task that is ready & has the highest priority
        }

        task = lastTaskRan[highPrio];                           // Get last task ran at this priority level

        while(!ok) {
            task = (task+1) % taskCount;                        // Start at next task. Wrap around if needed
            ok = (tcb[task].state == STATE_READY && tcb[task].currentPriority == highPrio);
        }                                                       // Find task ready at this priority level

        lastTaskRan[highPrio] = task;                           // Update last task ran (the one about to be schedule)
    }
    else {
        while (!ok)
        {
            task++;
            if (task >= MAX_TASKS)
                task = 0;
            ok = (tcb[task].state == STATE_READY);
        }
    }
    return task;
}

// REQUIRED: modify this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0
// fn set TMPL bit, and PC <= fn
void startRtos(void)
{
    setPSP((uint32_t *)0x20008000);         // Might cause an MPU fault. Top of SRAM needs access
    setASP();
    setTMPL();
    __asm(" SVC #0 ");
}

// REQUIRED:
// add task if room in task list
// store the thread name
// allocate stack space and store top of stack in sp and spInit
// set the srd bits based on the memory allocation
// initialize the created stack to make it appear the thread has run before
bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;

    void *baseAdd = 0;

    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))
        {
            found = (tcb[i++].pid ==  fn);
        }
        if (!found)
        {
            // find first available tcb record
            i = 0;
            while (tcb[i].state != STATE_INVALID) {i++;}
            tcb[i].state = STATE_READY;
            tcb[i].pid = fn;

            baseAdd = mallocFromHeap(stackBytes);                       // Allocate space
            taskCurrent++;

            tcb[i].sp = (void *)((uint32_t)baseAdd + stackBytes);       // Top of stack
            tcb[i].spInit = tcb[i].sp;                                  // Top of stack
            tcb[i].priority = priority;
            tcb[i].currentPriority = priority;
            tcb[i].srd = createNoSramAccessMask();
            addSramAccessWindow(&tcb[i].srd, baseAdd, stackBytes);

            strgcopy(tcb[i].name, name);                                // Copy name

            tcb[i].size = stackBytes;                                   // Store size

            // Make it look as though it has ran before
            uint32_t *p = tcb[i].sp;
            *(--p) = 0x01000000;        // XPSR -> Thumb bit set
            *(--p) = (uint32_t)fn;      // PC   -> Function address
            *(--p) = 0x0000DEAD;        // LR
            *(--p) = 0x0000000C;        // R12
            *(--p) = 0x00000003;        // R3
            *(--p) = 0x00000002;        // R2
            *(--p) = 0x00000001;        // R1
            *(--p) = 0x00000000;        // R0
            *(--p) = 0x00000004;        // R4
            *(--p) = 0x00000005;        // R5
            *(--p) = 0x00000006;        // R6
            *(--p) = 0x00000007;        // R7
            *(--p) = 0x00000008;        // R8
            *(--p) = 0x00000009;        // R9
            *(--p) = 0x0000000A;        // R10
            *(--p) = 0x0000000B;        // R11
            *(--p) = 0xFFFFFFFD;        // LR
            tcb[i].sp = (void *)p;                                      // Update SP

            tcb[i].mutex = MAX_MUTEXES;                                 // Has no mutex
            tcb[i].semaphore = MAX_SEMAPHORES;                          // Has no semaphore

            tcb[i].timeElpA = 0;
            tcb[i].timeElpB = 0;

            // increment task count
            taskCount++;
            ok = true;
        }
    }
    return ok;
}

// REQUIRED: modify this function to restart a thread
void restartThread(_fn fn)
{
    __asm(" SVC #19 ");     // SVC for Running Process
}

// REQUIRED: modify this function to stop a thread
// REQUIRED: remove any pending semaphore waiting, unlock any mutexes
void stopThread(_fn fn)
{
    __asm(" SVC #11 ");     // SVC for kill having the PID
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
    __asm(" SVC #20 ");     // SVC call to set prio
}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield(void)
{
    __asm(" SVC #1 ");        // SVC call to context switch
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm(" SVC #2 ");        // SVC call to sleep
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm(" SVC #3 ");        // SVC call to lock
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
    __asm(" SVC #4 ");        // SVC call to unlock
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{
    __asm(" SVC #5 ");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm(" SVC #6 ");
}

// mallocFromHeap: Wrapper function
void *_mallocFromHeap(uint32_t size) {
    __asm(" SVC #7 ");

    return (void *)getR0();
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr(void)
{
    static uint16_t ms = 0;
    uint8_t i;

    for(i=0; i<taskCount; i++) {
        if(tcb[i].state == STATE_DELAYED) {     // Check if task has to sleep

            tcb[i].ticks--;                     // Decrement once --> -1ms

            if(tcb[i].ticks == 0)               // If sleep time has expired
                tcb[i].state = STATE_READY;     // Task is now ready
        }
    }

    ms++;

    if(ms == 1000) {                            // When a seconds elapses
        ms = 0;                                 // Zero out ms

        ping = !ping;                           // swap flag, (tells buffer to write to)

        if(ping) {                              // if ping we using A, so clear old data
            for(i=0; i<taskCount; i++)
                tcb[i].timeElpA = 0;
        }
        else {                                  // else pong we using B, so clear old data
            for(i=0; i<taskCount; i++)
                tcb[i].timeElpB = 0;
        }
    }

    if(preemption)
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;   // if pre-emption Yield

}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
__attribute__((naked))
void pendSvIsr(void)
{
    // If the MPU DERR or IERR bits are set, clear them
    if(NVIC_FAULT_STAT_R & (NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR)) {
        NVIC_FAULT_STAT_R = NVIC_FAULT_STAT_DERR | NVIC_FAULT_STAT_IERR;
        //putsUart0("Call from MPU\n");     // This causes a Weird  Error. Fixed if commented out!
    }

    pushRegsOnPSP();
    tcb[taskCurrent].sp = getPSP();                     // save PSP

    WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;                   // End timer

    if(ping)                                            // If ping, Write to A
        tcb[taskCurrent].timeElpA += WTIMER0_TAV_R;
    else                                                // Else pong, Write to B
        tcb[taskCurrent].timeElpB += WTIMER0_TAV_R;

    taskCurrent = rtosScheduler();                      // Call Scheduler

    WTIMER0_TAV_R = 0;                                  // Zero out timer
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;                    // Start Timer

    applySramAccessMask(tcb[taskCurrent].srd);          // Restore SRD bits for next task
    setPSP(tcb[taskCurrent].sp);                        // Restore PSP

    tcb[taskCurrent].sp = (void *)(getPSP() + 9);       // About to POP 9 regs, update accordingly
    popRegsOnPSP();
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void)
{
    uint32_t *PSP = getPSP();
    uint8_t *PC = (uint8_t *)(*(PSP+6));                        // Get PC
    uint8_t svcNum = *(PC-2);                                   // Get SVC Number

    // Variable often used in most switch cases
    uint8_t i, j, k;
    uint8_t mutex;
    uint8_t sema;
    uint8_t *q;

    void *pid;
    char *name;

    switch(svcNum) {
        case RTOS_START: {                                      // Start RTOS
            taskCurrent = rtosScheduler();                      // Call Scheduler
            WTIMER0_CTL_R |= TIMER_CTL_TAEN;
            applySramAccessMask(tcb[taskCurrent].srd);          // Restore SRD bits for next task
            setPSP(tcb[taskCurrent].sp);                        // Restore PSP
            tcb[taskCurrent].sp = (void *)(getPSP() + 9);       // About to POP 9 Regs
            popRegsOnPSP();
            break;
        }
        case TASK_SWITCH: {                                     // Task Switching
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;           // PendSV call
            break;
        }
        case TASK_SLEEP: {                                      // Sleep
            tcb[taskCurrent].ticks = *PSP;                      // Get ms
            tcb[taskCurrent].state = STATE_DELAYED;             // Set state to delay
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;           // yield (pendSV)
            break;
        }
        case TASK_LOCK: {                                       // Lock
            mutex = *PSP;                                       // Get mutex
            tcb[taskCurrent].mutex = mutex;                     // Set Mutex in tcb
            q = mutexes[mutex].processQueue;                    // ptr to queue (easier to access)

            if(!mutexes[mutex].lock) {                          // If free
                mutexes[mutex].lock = true;                     // lock it
                mutexes[mutex].lockedBy = taskCurrent;          // Lock by current task
            }
            else {                                              // else
                if(priorityInheritance) {
                    if(tcb[mutexes[mutex].lockedBy].priority > tcb[taskCurrent].priority)           // If whoever hold it has a lower priority
                        tcb[mutexes[mutex].lockedBy].currentPriority = tcb[taskCurrent].priority;   // Elevate its priority
                }

                q[mutexes[mutex].queueSize] = taskCurrent;      // Add task to queue
                mutexes[mutex].queueSize++;                     // Increase queue size
                tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;   // Set task state to mutex blocked
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;       // yield
            }
            break;
        }
        case TASK_UNLOCK: {                                     // Unlock
            mutex = *PSP;                                       // Call taskUlock passing mutex and task
            taskUnlock(mutex, taskCurrent);
            break;
        }
        case TASK_WAIT: {                                       // Semaphore wait
            sema = *PSP;                                        // Get semaphore num
            tcb[taskCurrent].semaphore = sema;
            q = semaphores[sema].processQueue;                  // ptr to queue (easier to access)

            if(semaphores[sema].count > 0) {                    // If there is a count
                semaphores[sema].count--;                       // Decrement count
                return;
            }
            else {                                                  // else
                q[semaphores[sema].queueSize] = taskCurrent;        // Place task in queue
                semaphores[sema].queueSize++;                       // increment size
                tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;   // Set task state to semaphore blocked
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;           // yield
            }
            break;
        }
        case TASK_POST: {                                       // Semaphore post
            sema = *PSP;
            tcb[taskCurrent].semaphore = sema;
            q = semaphores[sema].processQueue;                  // ptr to queue (easier to access)

            semaphores[sema].count++;                           // Increase count

            if(semaphores[sema].queueSize) {                    // If there is a queue
                i = 0;

                tcb[q[i]].state = STATE_READY;                  // First on queue set to ready
                semaphores[sema].queueSize--;                   // Decrement queue size
                semaphores[sema].count--;                       // Decrement count

                for(i=0; i<MAX_SEMAPHORE_QUEUE_SIZE-1; i++) {   // Dequeue
                    q[i] = q[i+1];
                }
                q[i] = 0;
            }
            break;
        }
        case TASK_MALLOC: {                                             // Malloc From Heap
            uint32_t size = *PSP;                                       // Get size to allocate
            void *baseAdd = mallocFromHeap(size);                       // Call mallocFromHeap
            addSramAccessWindow(&tcb[taskCurrent].srd, baseAdd, size);  // Update SRD bits
            applySramAccessMask(tcb[taskCurrent].srd);                  // Apply updated access
            *PSP = (uint32_t)baseAdd;                                   // return based Address
            break;
        }
        case SHELL_REBOOT: {                                    // Reboot System
            NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            break;
        }
        case SHELL_PS: {                                        // Print data from tcb
            PS *p = (PS *)*PSP;                                 // Get pointer to struct on shell
            uint64_t timeElap;

            for(i=0; i<taskCount; i++) {                        // for every valid task get
                strgcopy(p[i].name, tcb[i].name);               // name
                p[i].PID = (uint32_t)tcb[i].pid;                // PID

                if(ping)                                        
                    timeElap = tcb[i].timeElpB;                 // if ping we writing to A, so read B
                else
                    timeElap = tcb[i].timeElpA;                 // if pong we writing to B, so read A

                p[i].cpu = (timeElap * FIX_PCT)/SYS_CLK;        // %CPU

                p[i].state = tcb[i].state;                      // state
                p[i].sem = tcb[i].semaphore;                    // semaphore
                p[i].mtx = tcb[i].mutex;                        // mutex
            }

            break;
        }
        case SHELL_IPCS: {                                      // Semaphore and Mutex Usage
            IPCS *p = (IPCS *)*PSP;

            for(i=0; i<MAX_SEMAPHORES; i++) {                   // Populate semaphores info
                q = semaphores[i].processQueue;                 // ptr to queue (easier to access)

                p[i].sCount = semaphores[i].count;              // Get count
                p[i].qSize = semaphores[i].queueSize;           // Get queue size

                for(j=0; j<semaphores[i].queueSize; j++)        // Of those on queue, get pids
                    p[i].q[j] = (uint32_t)tcb[q[j]].pid;
            }

            for(j=0; j<MAX_MUTEXES; j++) {                      // Then populate mutex info
                q = mutexes[j].processQueue;                    // ptr to queue (easier to access)

                p[i+j].mLock = mutexes[j].lock;                 // Get state of mutex
                p[i+j].qSize = mutexes[j].queueSize;            // Get queue size

                for(k=0; k<mutexes[j].queueSize; k++)           // Of those on queue, get pids
                    p[i+j].q[k] = (uint32_t)tcb[q[k]].pid;
                                                                // Get pid of the one locking it
                p[i+j].mLockedBy = (uint32_t)tcb[mutexes[j].lockedBy].pid;
            }
            break;
        }
        case SHELL_KILL: {                                      // kill based on PID
            pid = (void *)*PSP;                                 // Get task PID

            for(i=0; i<taskCount; i++) {                        // Iterate thru tcb looking for PID match
                if(tcb[i].pid == pid) {
                    if(tcb[i].state != STATE_STOPPED) {         // Make sure task hasn't already been killed
                        taskKill(i);                            // Once found, kill task
                        *PSP = 1;                               // Return success flag
                        return;
                    }
                    *PSP = 0xFF;                                // If task has been killed, return no flag
                    return;
                }
            }
            *PSP = 0;                                           // Else return failure flag
            break;
        }
        case SHELL_PKILL: {                                     // kill based on name
            name = (char *)*PSP;                                // Get task name

            for(i=0; i<taskCount; i++) {                        // Iterate thru tcb looking for name match
                if(strgcmp(name, tcb[i].name )) { 
                    if(tcb[i].state != STATE_STOPPED) {         // Make sure task hasn't already been killed
                        taskKill(i);                            // Once found, kill task
                        *PSP = 1;                               // Return success flag
                        return;
                    }
                    *PSP = 0xFF;                                // Else return NO flag
                    return;
                }
            }
            *PSP = 0;                                           // Else return failure flag
            break;
        }
        case SHELL_PI: {                                        // Priority Inheritance ON|OFF
            bool isEnable = *PSP;
            priorityInheritance = isEnable;
            break;
        }
        case SHELL_PREEMPT: {                                   // Pre-epmtion ON|OFF
            bool isEnable = *PSP;
            preemption = isEnable;
            break;
        }
        case SHELL_SCHED: {                                     // scheduling RR|PIRO
            bool prioEnable = *PSP;
            priorityScheduler = prioEnable;
            break;
        }
        case SHELL_PIDOF: {                                     // Get task PID having its name
            name = (char *)*PSP;

            for(i=0; i<taskCount; i++) {
                if(strgcmp(name, tcb[i].name)) {                // Iterate thru tcb looking for name match
                    *PSP = (uint32_t)tcb[i].pid;                // Once found set the *PSP = PID
                    return;
                }
            }
            *PSP = 0;                                           // If not found, return 0 (NULL)
            break;
        }
        case SHELL_RUN_PROC: {                                  // Run a task by name
            name = (char *)*PSP;

            for(i=0; i<taskCount; i++) {
                if(strgcmp(name, tcb[i].name)) {                // Iterate thru tcb looking for name match
                    if(tcb[i].state == STATE_STOPPED) {         // If task has been killed
                        taskRestart(i);                         // Restart it
                        *PSP = 1;                               // Set success flag
                        return;
                    }
                    *PSP = 0xFF;                                // Task Running? Set no flag
                    return;
                }
            }
            *PSP = 0;                                           // No name match, set failure flag
            break;
        }
        case SHELL_MEMINFO: {                                   // print threads memory usage info
            MEM *mem = (MEM *)*PSP;

            for(i=0; i<HCB_MAX_SIZE; i++) {                     // Iterate thru HCB
                if(HCB_table[i].size) {                         // If there is a size, look at its metadata
                    mem[i].pid = (uint32_t)HCB_table[i].PID;    // Store PID
                    mem[i].baseAdd = (uint32_t)HCB_table[i].ptr;// Store base address
                    mem[i].size = HCB_table[i].size;            // Store size of allocation
                }
            }
            break;
        }
        case RESTART_THREAD: {
            pid = (void *)*PSP;

            for(i=0; i<taskCount; i++) {                        // Iterate thru tcb looking for PID match
                if(tcb[i].pid == pid) {
                    if(tcb[i].state == STATE_STOPPED) {         // Only if task has been killed
                        taskRestart(i);                         // Restart it
                        return;
                    }
                }
            }
            break;
        }
        case SET_PRIORITY: {
            pid = (void *)*PSP;
            uint8_t prio = *(PSP+1);

            for(i=0; i<taskCount; i++) {                        // Iterate thru tcb looking for name match
                if(tcb[i].pid == pid) {
                    tcb[i].currentPriority = prio;              // Once found, set priority passed
                    return;
                }
            }
            break;
        }
        default:
            putsUart0("Somehow we got here... :(\n");
            break;
    }
}

// REQUIRED: SVC Mutex Unlock
void taskUnlock(uint8_t mutex, uint8_t task)
{
    tcb[task].mutex = mutex;                            // Set Mutex in tcb
    uint8_t *q = mutexes[mutex].processQueue;
    uint8_t i = 0;

    if(mutexes[mutex].lockedBy == task) {               // If task locking mutex matches task passed proceed

        if(priorityInheritance)                         // If there is pi, currentPrio has been changed
            tcb[task].currentPriority = tcb[task].priority;

        if(mutexes[mutex].queueSize) {                  // if there is a queue
            tcb[q[i]].state = STATE_READY;              // First on queue is set to ready
            mutexes[mutex].lockedBy = q[i];             // Mutex now locked by first on queue
            mutexes[mutex].queueSize--;                 // Decrement queue size

            for(i=0; i<MAX_MUTEX_QUEUE_SIZE-1; i++) {   // Dequeue
                q[i] = q[i+1];
            }
            q[i] = 0;
        }
        else {
            mutexes[mutex].lock = false;                // unlock it
            mutexes[mutex].lockedBy = 0;                // no one has lock it
        }
    }
    else {
        taskKill(task);                                 // Else, kill task trying to unlock mutex &
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;       // yield
    }
}


// REQUIRED: Restart Thread
void taskRestart(uint8_t task) {
    uint8_t curr = taskCurrent;                                     // temporarily store taskCurrent

    taskCurrent = task;                                             // make currentTask = task passed, needed for logic in malloc
    void *baseAdd = mallocFromHeap(tcb[task].size);                 // Reallocate memory
    taskCurrent = curr;                                             // Restore currentTask

    tcb[task].sp = (void *)((uint32_t)baseAdd + tcb[task].size);    // Reset Stack Pointer
    tcb[task].spInit = tcb[task].sp;                                // Reset PS initial
    addSramAccessWindow(&tcb[task].srd, baseAdd, tcb[task].size);   // Add its respective access

    // Make it look as though it has ran before
    uint32_t *p = tcb[task].sp;
    *(--p) = 0x01000000;                // XPSR -> Thumb bit set
    *(--p) = (uint32_t)tcb[task].pid;   // PC   -> Function address
    *(--p) = 0x0000DEAD;                // LR
    *(--p) = 0x0000000C;                // R12
    *(--p) = 0x00000003;                // R3
    *(--p) = 0x00000002;                // R2
    *(--p) = 0x00000001;                // R1
    *(--p) = 0x00000000;                // R0
    *(--p) = 0x00000004;                // R4
    *(--p) = 0x00000005;                // R5
    *(--p) = 0x00000006;                // R6
    *(--p) = 0x00000007;                // R7
    *(--p) = 0x00000008;                // R8
    *(--p) = 0x00000009;                // R9
    *(--p) = 0x0000000A;                // R10
    *(--p) = 0x0000000B;                // R11
    *(--p) = 0xFFFFFFFD;                // LR
    tcb[task].sp = (void *)p;

    tcb[task].state = STATE_READY;                                  // Set state to READY

    if(strgcmp(tcb[task].name, "ReadKeys"))                         // ReadKeys  is a special case, must increase count in its semaphore for it run properly
        semaphores[tcb[task].semaphore].count++;
}


// REQUIRED: Kernel Kill Function
void taskKill(uint8_t task)
{
    uint8_t mutex = tcb[task].mutex;                        // Get mutex associated with task
    uint8_t sema = tcb[task].semaphore;                     // Get semaphore associated with task
    uint8_t i;

    if(mutex != MAX_MUTEXES) {                              // check if it has a mutex
        if(tcb[task].state == STATE_BLOCKED_MUTEX) {        // Check if it's blocked by a mutex
            if(mutexes[mutex].queueSize) {                  // check if there is a queue in mutex
                uint8_t *qM = mutexes[mutex].processQueue;

                for(i=0; i<MAX_MUTEX_QUEUE_SIZE-1; i++) {   // if task is in queue, Dequeue it
                    if(qM[i] == task) {
                        qM[i] = qM[i+1];
                        mutexes[mutex].queueSize--;
                    }
                }
                if(qM[i] == task) {
                    qM[i] = 0;
                    mutexes[mutex].queueSize--;
                }
            }
        }
        else {                                              // else, task is holding mutex
            taskUnlock(mutex, task);                        // unlock it
        }
    }

    if(sema != MAX_SEMAPHORES) {                            // check if it has a semaphore
        if(tcb[task].state == STATE_BLOCKED_SEMAPHORE) {    // Check is it is blocked by semaphore
            if(semaphores[sema].queueSize) {                // check if there is a queue in semaphore
                uint8_t *qS = semaphores[sema].processQueue;

                for(i=0; i<MAX_MUTEX_QUEUE_SIZE-1; i++) {   // if task is in queue, dequeue it
                    if(qS[i] == task) {
                        qS[i] = qS[i+1];
                        semaphores[sema].queueSize--;
                    }
                }
                if(qS[i] == task) {
                    qS[i] = 0;
                    semaphores[sema].queueSize--;
                }
            }
        }
    }

    freeToHeap(tcb[task].spInit);                       // Free memory
    tcb[task].srd = createNoSramAccessMask();           // Remove its access, update SRD bits
    tcb[task].state = STATE_STOPPED;                    // Set state to stopped
}

//----------------------------------------------------
// Other helper functions
//----------------------------------------------------
void *getPID() {
    return tcb[taskCurrent].pid;
}
