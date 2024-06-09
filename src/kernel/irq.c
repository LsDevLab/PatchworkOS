#include "irq.h"

#include "apic.h"
#include "debug.h"
#include "pic.h"

static IrqHandler handlers[IRQ_AMOUNT][IRQ_MAX_HANDLER];

void irq_dispatch(TrapFrame* trapFrame)
{
    uint64_t irq = trapFrame->vector - IRQ_BASE;

    for (uint64_t i = 0; i < IRQ_MAX_HANDLER; i++)
    {
        if (handlers[irq][i] != NULL)
        {
            handlers[irq][i](irq);
        }
        else
        {
            break;
        }
    }

    // TODO: Replace with io apic
    pic_eoi(irq);
}

void irq_install(IrqHandler handler, uint8_t irq)
{
    for (uint64_t i = 0; i < IRQ_MAX_HANDLER; i++)
    {
        if (handlers[irq][i] == NULL)
        {
            handlers[irq][i] = handler;
            return;
        }
    }

    debug_panic("IRQ handler limit exceeded");
}