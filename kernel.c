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
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "uartio.h"
#include "mm.h"
#include "spctl.h"
#include "kernel.h"

#include "gpio.h"

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

// task states
#define STATE_INVALID           0 // no task
#define STATE_STOPPED           1 // stopped, can be resumed
#define STATE_UNRUN             2 // task has never been run
#define STATE_READY             3 // has run, can resume at any time
#define STATE_DELAYED           4 // has run, but now awaiting timer
#define STATE_BLOCKED_MUTEX     5 // has run, but now blocked by semaphore
#define STATE_BLOCKED_SEMAPHORE 6 // has run, but now blocked by semaphore

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks

// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = false;          // preemption (true) or cooperative (false)

// tcb
#define NUM_PRIORITIES   8
struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread (add of task fn)
    void *spInit;                  // original top of stack
    void *sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint8_t srd[NUM_SRAM_REGIONS]; // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
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

    NVIC_ST_RELOAD_R = 39999; // (40Mhz / 1khz) - 1
                      // clock source        enable int           enable systick
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_ENABLE;
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void)
{
    bool ok;
    static uint8_t task = 0xFF;
    static uint8_t last_task[7] = {0, 0, 0, 0, 0, 0, 0};
    ok = false;

    if (priorityScheduler)
    {
        uint8_t highest_priority = 0;
        task = last_task[highest_priority] + 1;

        while (!ok)
        {
            ok = (tcb[task].priority == highest_priority && (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN));
            if (!ok)
            {
                task++;
                if (task >= MAX_TASKS)
                {
                    task = 0;
                }
                if (task == last_task[highest_priority] + 1)
                {
                    highest_priority++;
                    if (highest_priority > 7)
                        highest_priority = 0;
                    task = last_task[highest_priority] + 1;
                }
            }
        }
        last_task[highest_priority] = task;
        return task;
    }
    else
    {
        while (!ok)
        {
            task++;
            if (task >= MAX_TASKS)
                task = 0;
            ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
        }
        return task;
    }
}

// REQUIRED: modify this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0
// fn set TMPL bit, and PC <= fn
void startRtos(void)
{
    taskCurrent = rtosScheduler();
    applySramSrdMasks(tcb[taskCurrent].srd);
    setPsp((uint32_t)tcb[taskCurrent].sp); // sets psp to first tasks' sp
    tcb[taskCurrent].state = STATE_READY;
    usePsp();
    setPcTmpl(tcb[taskCurrent].pid); // enables privileged mode
}

// REQUIRED:
// add task if room in task list - done
// store the thread name
// allocate stack space and store top of stack in sp and spInit - done
// set the srd bits based on the memory allocation - done
bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
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

            tcb[i].state = STATE_UNRUN;
            tcb[i].pid = fn;
            tcb[i].spInit = (void *) ((uint32_t)mallocFromHeap(stackBytes) + (stackBytes - 1));
            tcb[i].sp = tcb[i].spInit;
            tcb[i].priority = priority;
            CopyStrings((char*)name, tcb[i].name);
            // tcb[i].name[0] = i + 65;
            //tcb[i].name[1] = 0;
            generateSramSrdMasks(tcb[i].srd, tcb[i].spInit, stackBytes); //spinit - (stackBytes + 1)

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
    __asm("    SVC #8");
}

// REQUIRED: modify this function to stop a thread
// REQUIRED: remove any pending semaphore waiting, unlock any mutexes
void stopThread(_fn fn)
{
    __asm("    SVC #10");
}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
    __asm("    SVC #12");
}

uint32_t getPid(const char name[])
{
    __asm("    SVC #7");
}

void getMutexInfo(IPCS_MUT_DATA *mutex_data, uint8_t mutex)
{
    __asm("    SVC #13");
}

void getSemaphoreInfo(IPCS_SEM_DATA *sem_data, uint8_t semaphore)
{
    __asm("    SVC #14");
}

void getTcb(PS_DATA *ps_data)
{
    __asm("    SVC #16");
}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield(void)
{
    __asm("    SVC #1");
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm("    SVC #2");
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex)
{
    __asm("    SVC #3");
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex)
{
    __asm("    SVC #4");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{
    __asm("    SVC #5");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm("    SVC #6");
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr(void)
{
    uint8_t task;
    for (task = 0; task < taskCount; task++)
    {
        if (tcb[task].state == STATE_DELAYED)
        {
            tcb[task].ticks--;

            if (!tcb[task].ticks)
                tcb[task].state = STATE_READY;
        }
    }

    if (preemption)
    {
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    }

    // togglePinValue(PORTD,1);
    // NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PENDSTCLR;
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr(void)
{
    strRegs();

    tcb[taskCurrent].sp = (void *)getPsp();
    taskCurrent = rtosScheduler();
    setPsp((uint32_t)tcb[taskCurrent].sp);
    applySramSrdMasks(tcb[taskCurrent].srd);

    if (tcb[taskCurrent].state == STATE_READY)
        ldrRegs();
    else
    {
        tcb[taskCurrent].state = STATE_READY;
        runProgram(tcb[taskCurrent].pid);
    }

}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void)
{
    uint32_t svc_num = *(uint32_t *)(*(getPsp() + 6) - 2) & 0xFF;

    switch (svc_num)
    {
        case 1: // yield
        {
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            break;
        }
        case 2: // sleep
        {
            uint32_t ticks = *getPsp();

            tcb[taskCurrent].state = STATE_DELAYED;
            tcb[taskCurrent].ticks = ticks;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            break;
        }
        case 3: // lock
        {
            int8_t mutex = *getPsp();
            // if mutex free
            if (mutexes[mutex].lock == false)
            {
                // lock it man
                mutexes[mutex].lock = true;
                // specify who locked it
                mutexes[mutex].lockedBy = taskCurrent;
            }
            else
            {
                // add task to mutex queue
                mutexes[mutex].processQueue[mutexes[mutex].queueSize] = taskCurrent;
                // inc waiting list
                mutexes[mutex].queueSize++;
                // state -> blocked mutex
                tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;
                // pendsv
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            }
            break;
        }
        case 4: // unlock
        {
            int8_t mutex = *getPsp();
            // if you locked it
            if (mutexes[mutex].lockedBy == taskCurrent)
            {
                // unlock it man
                mutexes[mutex].lock = false;

                // if people are waiting (waitors)
                if (mutexes[mutex].queueSize)
                {
                    uint8_t ager = 0;
                    // mark next one as ready
                    tcb[mutexes[mutex].processQueue[ager]].state = STATE_READY;
                    mutexes[mutex].lockedBy = mutexes[mutex].processQueue[0];
                    // decrement wait count, age all waitors
                    mutexes[mutex].queueSize--;
                    for (ager = 0; ager < (MAX_MUTEX_QUEUE_SIZE - 1); ager++)
                    {
                        mutexes[mutex].processQueue[ager] = mutexes[mutex].processQueue[ager + 1];
                    }
                    mutexes[mutex].lock = true;
                }
            }
            break;
        }
        case 5: // wait
        {
            int8_t semaphore = *getPsp();
            // decrement count number if there is a count
            if (semaphores[semaphore].count > 0)
            {
                semaphores[semaphore].count--;
            }
            else
            {
                // place task in queue
                semaphores[semaphore].processQueue[semaphores[semaphore].queueSize] = taskCurrent;
                // increment queue count
                semaphores[semaphore].queueSize++;
                // state goes to wait
                tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;

                tcb[taskCurrent].semaphore = semaphore;
                // switch task
                NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
            }
            break;
        }
        case 6: // post
        {
            int8_t semaphore = *getPsp();

            semaphores[semaphore].count++;
            // if someone is in the queue
            if (semaphores[semaphore].queueSize)
            {
                uint8_t ager = 0;
                // set task to ready
                tcb[semaphores[semaphore].processQueue[ager]].state = STATE_READY;
                // decrement queue count
                semaphores[semaphore].queueSize--;
                // delete old waitor in list
                for (ager = 0; ager < (MAX_SEMAPHORE_QUEUE_SIZE - 1); ager++)
                {
                    semaphores[semaphore].processQueue[ager] = semaphores[semaphore].processQueue[ager + 1];
                }
                // decrement count
                semaphores[semaphore].count--;
            }
            break;
        }
        case 7: // getPid
        {
            uint8_t task = 0;
            bool ok = false;

            while(!ok && task < MAX_TASKS)
            {
                ok = (stringsEqual(tcb[task].name, (char *)*getPsp()));
                if (!ok)
                    task++;
            }

            if (ok)
                strPid((uint32_t)tcb[task].pid);

            break;
        }
        case 8: // restart thread
        {
            uint8_t task = 0;
            bool ok = false;

            while(!ok && task < MAX_TASKS)
            {
                ok = (tcb[task].pid == (void *)*getPsp());
                if (!ok)
                    task++;
            }

            if (ok)
            {
                // tcb[task].sp = tcb[task].spInit;
                tcb[task].state = STATE_READY;
            }

            break;
        }
        case 9: // preempt
        {
            preemption = *getPsp();
            break;
        }
        case 10: // stop thread
        {
            uint8_t task = 0;
            bool ok = false;

            while(!ok && task < MAX_TASKS)
            {
                ok = (tcb[task].pid == (void *)*getPsp());
                if (!ok)
                    task++;
            }

            if (tcb[task].state == STATE_BLOCKED_MUTEX)
            {
                ok = false;

                uint8_t queue_index = 0;
                while (!ok && queue_index < (MAX_MUTEX_QUEUE_SIZE - 1))
                {
                    ok = (mutexes[tcb[task].mutex].processQueue[queue_index] == task);
                    if (!ok)
                        queue_index++;
                }

                uint8_t ager = queue_index;
                // decrement wait count, age all waitors
                mutexes[tcb[task].mutex].queueSize--;
                for (ager = 0; ager < (MAX_MUTEX_QUEUE_SIZE - 1); ager++)
                {
                    mutexes[tcb[task].mutex].processQueue[ager] = mutexes[tcb[task].mutex].processQueue[ager + 1];
                }
            }

            if (mutexes[tcb[task].mutex].lockedBy == task)
            {
                mutexes[tcb[task].mutex].lock = false;

                // if people are waiting (waitors)
                if (mutexes[tcb[task].mutex].queueSize)
                {
                    uint8_t ager = 0;
                    // mark next one as ready
                    tcb[mutexes[tcb[task].mutex].processQueue[ager]].state = STATE_READY;
                    mutexes[tcb[task].mutex].lockedBy = mutexes[tcb[task].mutex].processQueue[0];
                    // decrement wait count, age all waitors
                    mutexes[tcb[task].mutex].queueSize--;
                    for (ager = 0; ager < (MAX_MUTEX_QUEUE_SIZE - 1); ager++)
                    {
                        mutexes[tcb[task].mutex].processQueue[ager] = mutexes[tcb[task].mutex].processQueue[ager + 1];
                    }
                    mutexes[tcb[task].mutex].lock = true;
                }
            }

            if (tcb[task].state == STATE_BLOCKED_SEMAPHORE)
            {
                semaphores[tcb[task].semaphore].count++;
                // if someone is in the queue
                if (semaphores[tcb[task].semaphore].queueSize)
                {
                    uint8_t ager = 0;
                    // set task to ready
                    tcb[semaphores[tcb[task].semaphore].processQueue[ager]].state = STATE_READY;
                    // decrement queue count
                    semaphores[tcb[task].semaphore].queueSize--;
                    // delete old waitor in list
                    for (ager = 0; ager < (MAX_SEMAPHORE_QUEUE_SIZE - 1); ager++)
                    {
                        semaphores[tcb[task].semaphore].processQueue[ager] = semaphores[tcb[task].semaphore].processQueue[ager + 1];
                    }
                    // decrement count
                    semaphores[tcb[task].semaphore].count--;
                }
            }

            tcb[task].state = STATE_STOPPED;

            break;
        }
        case 11: // reboot
        {
            NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            break;
        }
        case 12: // thread priority
        {
            uint8_t task = 0;
            bool ok = false;

            while(!ok && task < MAX_TASKS)
            {
                ok = (tcb[task].pid == (void *)*getPsp());
                if (!ok)
                    task++;
            }

            if (ok)
            {
                tcb[task].priority = *(getPsp() + 1);
            }

            break;
        }
        case 13: // get mutex data
        {
            uint8_t mutex_queue = 0;

            uint8_t mutex = *(getPsp() + 1);

            // IPCS_MUT_DATA *mut_data = *getPsp();

            // CopyStrings(tcb[mutexes[mutex].lockedBy].name, mutex->lockedByName);

            /*
            while (mutex_queue != mutexes[mutex].queueSize)
            {

            }
            */

            break;
        }
        case 14: // get sem data
        {


            break;
        }
        case 15: // sched
        {
            priorityScheduler = *getPsp();

            break;
        }
        case 16: // ps
        {
            uint8_t task = 0;
            PS_DATA *ps_data = (PS_DATA*)getPsp();

            while(task < taskCount)
             {
                 CopyStrings(tcb[task].name, (ps_data->name[task]));
                 ps_data->pid[task] = (uint32_t)tcb[task].pid;
                 task++;
             }

            break;
        }
    }
}

