#include "worker_pool.h"

#include <libc/string.h>

#include "idt/idt.h"
#include "apic/apic.h"
#include "utils/utils.h"
#include "tty/tty.h"
#include "madt/madt.h"
#include "master/master.h"
#include "debug/debug.h"
#include "worker/interrupts/interrupts.h"
#include "worker/scheduler/scheduler.h"
#include "worker/program_loader/program_loader.h"
#include "worker/trampoline/trampoline.h"
#include "lib-asym.h"
#include "queue/queue.h"
#include "vfs/vfs.h"
#include "worker/process/process.h"

static Worker workers[MAX_WORKER_AMOUNT];
static uint8_t workerAmount;

static Idt idt;

static void worker_pool_startup()
{
    memset(workers, 0, sizeof(Worker) * MAX_WORKER_AMOUNT);
    workerAmount = 0;

    worker_trampoline_setup();

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE) && 
            record->localApicId != master_apic_id())
        {
            if (!worker_init(workers, workerAmount, record->localApicId))
            {    
                tty_print("Worker "); 
                tty_printi(record->cpuId); 
                tty_print(" failed to start!");
                tty_end_message(TTY_MESSAGE_ER);
            }
            workerAmount++;
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }

    worker_trampoline_cleanup();
}

void worker_pool_init()
{
    tty_start_message("Worker Pool initializing");

    worker_idt_populate(&idt);

    worker_pool_startup();

    tty_end_message(TTY_MESSAGE_OK);
}

void worker_pool_send_ipi(Ipi ipi)
{
    for (uint8_t i = 0; i < workerAmount; i++)
    {
        worker_send_ipi(worker_get(i), ipi);
    }
}

//Temporary
void worker_pool_spawn(const char* path)
{        
    Process* process = process_new(PROCESS_PRIORITY_MIN);
    File* file;
    Status status = vfs_open(&file, path, FILE_FLAG_READ);
    if (status != STATUS_SUCCESS)
    {
        return;
    }
    if (load_program(process, file) != STATUS_SUCCESS)
    {
        process_free(process);
        return;
    }
    vfs_close(file);

    for (uint8_t i = 0; i < workerAmount; i++)
    {
        scheduler_acquire(worker_get(i)->scheduler);
    }

    uint64_t bestLength = -1;
    Scheduler* bestScheduler = 0;
    for (uint8_t i = 0; i < workerAmount; i++)
    {
        Scheduler* scheduler = worker_get(i)->scheduler;
        uint64_t length = (scheduler->runningProcess != 0);
        for (int64_t priority = PROCESS_PRIORITY_MAX; priority >= PROCESS_PRIORITY_MIN; priority--) 
        {
            length += queue_length(scheduler->queues[priority]);
        }

        if (bestLength > length)
        {
            bestLength = length;
            bestScheduler = scheduler;
        }
    }

    scheduler_push(bestScheduler, process);

    for (uint8_t i = 0; i < workerAmount; i++)
    {
        scheduler_release(worker_get(i)->scheduler);
    }
}

Idt* worker_idt_get()
{
    return &idt;
}

uint8_t worker_amount()
{
    return workerAmount;
}

Worker* worker_get(uint8_t id)
{
    return &workers[id];
}

Worker* worker_self()
{
    uint64_t id = read_msr(MSR_WORKER_ID);
    if (id >= MAX_WORKER_AMOUNT)
    {
        debug_panic("Invalid worker");
        return 0;
    }

    return &workers[id];
}

Worker* worker_self_brute()
{
    uint8_t apicId = local_apic_id();
    for (uint16_t i = 0; i < MAX_WORKER_AMOUNT; i++)
    {
        Worker* worker = worker_get((uint8_t)i);

        if (worker->present && worker->apicId == apicId)
        {
            return worker;
        }
    }    

    debug_panic("Unable to find worker");
    return 0;
}