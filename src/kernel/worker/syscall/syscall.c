#include "syscall.h"

#include "tty/tty.h"
#include "worker_pool/worker_pool.h"

#include "worker/program_loader/program_loader.h"

#include <lib-syscall.h>

void syscall_exit(InterruptFrame* interruptFrame)
{    
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);             

    scheduler_exit(worker->scheduler);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);             
}

void syscall_spawn(InterruptFrame* interruptFrame)
{
    //TODO: Implement safe string length check
    const char* path = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));
    if ((uint64_t)path > USER_ADDRESS_SPACE_TOP)
    {
        SYSCALL_SET_RESULT(interruptFrame, -1);
        return;
    }

    Worker* worker = worker_self();
    scheduler_acquire(worker->scheduler);

    Process* process = process_new(PROCESS_PRIORITY_MIN);
    if (load_program(process, path))
    {
        scheduler_push(worker->scheduler, process);
        scheduler_release(worker->scheduler);

        SYSCALL_SET_RESULT(interruptFrame, process->id);
    }
    else
    {
        process_free(process);
        SYSCALL_SET_RESULT(interruptFrame, -1);
    }
}

void syscall_sleep(InterruptFrame* interruptFrame)
{
    Worker* worker = worker_self();

    scheduler_acquire(worker->scheduler);             

    Blocker blocker =
    {
        .timeout = time_nanoseconds() + SYSCALL_GET_ARG1(interruptFrame) * NANOSECONDS_PER_MILLISECOND
    };

    scheduler_block(worker->scheduler, interruptFrame, blocker);
    scheduler_schedule(worker->scheduler, interruptFrame);

    scheduler_release(worker->scheduler);             
}

Syscall syscallTable[] =
{
    [SYS_EXIT] = (Syscall)syscall_exit,
    [SYS_SPAWN] = (Syscall)syscall_spawn,
    [SYS_SLEEP] = (Syscall)syscall_sleep
};

void syscall_handler(InterruptFrame* interruptFrame)
{   
    uint64_t selector = interruptFrame->rax;

    //Temporary for testing
    if (selector == SYS_TEST)
    {
        tty_acquire();

        Worker const* worker = worker_self();

        const char* string = page_directory_get_physical_address(SYSCALL_GET_PAGE_DIRECTORY(interruptFrame), (void*)SYSCALL_GET_ARG1(interruptFrame));

        Point cursorPos = tty_get_cursor_pos();
        tty_set_cursor_pos(0, 16 * (worker->id + 2));

        tty_print("WORKER: "); 
        tty_printx(worker->id); 
        tty_print(" PROCESS AMOUNT: "); 
        tty_printx(scheduler_process_amount(worker->scheduler));
        if (worker->scheduler->runningProcess != 0)
        {
            tty_print(" PID: "); 
            tty_printx(worker->scheduler->runningProcess->id);
        }
        tty_print(" | ");
        tty_print(string);

        tty_set_cursor_pos(cursorPos.x, cursorPos.y);

        tty_release();
        return;
    }

    if (selector < sizeof(syscallTable) / sizeof(Syscall))
    {
        syscallTable[selector](interruptFrame);
    }
    else
    {
        SYSCALL_SET_RESULT(interruptFrame, -1);
    }
}
