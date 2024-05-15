// Shell functions
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
#include "gpio.h"
#include "uart0.h"
#include "uartio.h"
#include "kernel.h"
#include "shell.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void printHelp(void)
{
    putsUart0("Commands:\n");
    // reboot
    putsUart0(" reboot\t\t\tReboots the microcontroller\n");
    // ps
    putsUart0(" ps\t\t\tDisplays the process (thread) status\n");
    // ipcs
    putsUart0(" ipcs\t\t\tDisplays the inter-process (thread) communication status\n");
    // kill
    putsUart0(" kill pid\t\tKills the process (thread) with the matching PID\n");
    // Pkill
    putsUart0(" Pkill proc_name\tKills the thread based on the process name\n");
    // preempt
    putsUart0(" preempt ON|OFF\t\tTurns preemption on or off\n");
    // sched
    putsUart0(" sched PRIO|RR\t\tSelected ity or round-robin scheduling\n");
    // pidof
    putsUart0(" pidof proc_name\tDisplays the PID of the process (thread)\n");
    // run
    putsUart0(" run proc_name\t\tRuns the selected program in the background\n");
    // malloc
    putsUart0(" malloc size\t\tAllocates memory based on the given size\n");
    // free
    putsUart0(" free address\t\tFrees memory based on base hex address of a sub-region\n");
    // dm
    putsUart0(" dm\t\t\tDisplays the current memory addresses and their sizes\n");
    // clr
    putsUart0(" clr\t\t\tClears terminal window\n");
    // help
    putsUart0(" help\t\t\tDisplays this message again\n\n");
}

void ps(void)
{
    PS_DATA ps_data;
    getTcb(&ps_data);

    uint8_t task = 0;

    char buf[MAX_CHARS];

    for (task = 0; task < 12; task++)
    {
        putsUart0(ps_data.name[task]);
        putsUart0("\t\t");
        putsUart0(IntToString(ps_data.pid[task], buf));
        putsUart0("\n");
    }

    putsUart0("ps called\n");
}

void ipcs(void)
{

    IPCS_MUT_DATA resource_data;
    IPCS_SEM_DATA pressed_data;
    IPCS_SEM_DATA released_data;
    IPCS_SEM_DATA flash_data;

    // getMutexInfo(&resource_data);
    // getSemaphoreInfo(&sem_data);

    putsUart0("ipcs called\n");
}

void kill(uint8_t pid) // need 2 test
{
    stopThread((_fn)pid);

    char buf[MAX_CHARS];

    putsUart0(IntToString(pid, buf));
    putsUart0(" killed\n");
}

void Pkill(const char name[]) // need 2 test
{
    uint32_t pid = getPid(name);

    stopThread((_fn)pid);

    putsUart0((char*)name);
    putsUart0(" killed\n");
}

void reboot(void) // done
{
    __asm("    SVC #11");
}

void pidof(const char name[]) // done
{
    uint32_t pid = getPid(name);

    char buf[MAX_CHARS];

    putsUart0("The pid of ");
    putsUart0((char*)name);
    putsUart0(" is: ");
    putsUart0(IntToString(pid, buf));
    putsUart0("\n");
}

void run(const char name[]) // have to test with stopThread()
{
    uint32_t pid = getPid(name);
    restartThread((_fn)pid);

    putsUart0((char*)name);
    putsUart0(" now running\n");
}

void preempt(bool on) // done
{
    __asm("    SVC #9");
}

void sched(bool prio_on) // done
{
    __asm("    SVC #15");
}

// REQUIRED: add processing for the shell commands through the UART here
void shell(void)
{
    while (true)
    {
        if (kbhitUart0())
        {
            USER_DATA data;
            clearField(&data);

            uint8_t pid = 0;
            char* proc_name = 0;
            char* ptr = 0;
            bool valid = false;

            getsUart0(&data);
            parseFields(&data);

            if (isCommand(&data, "reboot", 0) || isCommand(&data, "r", 0))
            {
                putsUart0("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                putsUart0("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                reboot();
            }

            if (isCommand(&data, "ps", 0))
            {
                ps();

                valid = true;
            }

            if (isCommand(&data, "ipcs", 0))
            {
                ipcs();

                valid = true;
            }

            if (isCommand(&data, "kill", 1))
            {
                pid = getFieldInteger(&data, 1);

                kill(pid);

                valid = true;
            }

            if (isCommand(&data, "Pkill", 1))
            {
                proc_name = getFieldString(&data, 1);

                Pkill(proc_name);

                valid = true;
            }

            if (isCommand(&data, "preempt", 1) || isCommand(&data, "pre", 1))
            {
                ptr = getFieldString(&data, 1);
                if (stringsEqual("ON", ptr) || stringsEqual("on", ptr))
                {
                    preempt(true);
                    valid = true;
                }
                if (stringsEqual("OFF", ptr) || stringsEqual("off", ptr))
                {
                    preempt(false);
                    valid = true;
                }

                if (!valid)
                {
                    putsUart0("Invalid preemption setting, enter 'ON' or 'OFF'\n\n");
                    valid = true;
                }
            }

            if (isCommand(&data, "sched", 1))
            {
                ptr = getFieldString(&data, 1);
                if (stringsEqual("PRIO", ptr) || stringsEqual("prio", ptr))
                {
                    sched(true);
                    valid = true;
                }
                if (stringsEqual("RR", ptr) || stringsEqual("rr", ptr))
                {
                    sched(false);
                    valid = true;
                }

                if (!valid)
                {
                    putsUart0("Invalid scheduler, only available are 'PRIO' or 'RR'\n\n");
                    valid = true;
                }
            }

            if (isCommand(&data, "pidof", 1))
            {
                proc_name = getFieldString(&data, 1);

                pidof(getFieldString(&data, 1));

                valid = true;
            }


            else if (isCommand(&data, "run", 1))
            {
                proc_name = getFieldString(&data, 1);

                run(proc_name);

                valid = true;
            }

            else if (isCommand(&data, "clr", 0) || isCommand(&data, "c", 0))
            {
                putsUart0("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
                putsUart0("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

                valid = true;
            }

            else if (isCommand(&data, "help", 0) || isCommand(&data, "h", 0))
            {
                printHelp();

                valid = true;
            }

            else if (!valid)
                putsUart0("Command not valid\n\n");

            clearField(&data);
        }
        yield();
    }
}
