#include "debug.h"

#include "pmm.h"
#include "regs.h"
#include "smp.h"
#include "splash.h"
#include "time.h"

static int8_t xPos;
static int8_t yPos;

/*static void debug_set_x(int8_t x)
{
    xPos = x;
    tty_set_column((tty_column_amount() - DEBUG_COLUMN_AMOUNT * DEBUG_COLUMN_WIDTH) / 2 + x * DEBUG_COLUMN_WIDTH);
}

static void debug_set_y(int8_t y)
{
    yPos = y;
    tty_set_row((tty_row_amount() - DEBUG_ROW_AMOUNT) / 2 + y);
}

static void debug_start(const char* message)
{
    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    Pixel white;
    white.a = 255;
    white.r = 255;
    white.g = 255;
    white.b = 255;

    tty_set_scale(DEBUG_TEXT_SCALE);

    debug_set_x(0);
    debug_set_y(-1);

    tty_set_foreground(red);

    tty_print("KERNEL PANIC - ");
    tty_print(message);

    tty_set_foreground(white);
}

static void debug_move(const char* name, uint8_t x, uint8_t y)
{
    debug_set_x(x);
    debug_set_y(y);

    if (name != NULL)
    {
        tty_put('[');
        tty_print(name);
        tty_put(']');
    }

    debug_set_x(x);
    debug_set_y(y + 1);
}

static void debug_print(const char* string, uint64_t value)
{
    tty_print(string);
    tty_printx(value);

    debug_set_x(xPos);
    debug_set_y(yPos + 1);
}*/

void debug_init(GopBuffer* gopBuffer, BootFont* screenFont)
{
    SPLASH_FUNC();

    /*win_default_theme(&theme);

    font.scale = SPLASH_NAME_SCALE;
    font.glyphs = malloc(screenFont->glyphsSize);
    memcpy(font.glyphs, screenFont->glyphs, screenFont->glyphsSize);

    surface.buffer = gopBuffer->base;
    surface.height = gopBuffer->height;
    surface.width = gopBuffer->width;
    surface.stride = gopBuffer->stride;*/
}

void debug_panic(const char* message)
{
    /*while (1)
    {
        asm volatile("cli");

        tty_acquire();

        uint32_t oldRow = tty_get_row();
        uint32_t oldColumn = tty_get_column();

        debug_start(message);

        debug_move("Memory", 0, 0);
        debug_print("Free Pages = ", pmm_free_amount());
        debug_print("Reserved Pages = ", pmm_reserved_amount());

        debug_move("Other", 2, 0);
        debug_print("Current Time = ", time_uptime());
        debug_print("Cpu id = ", smp_self()->id);

        tty_set_scale(1);
        tty_set_row(oldRow);
        tty_set_column(oldColumn);

        tty_release();

        smp_send_ipi_to_others(IPI_HALT);
    }*/

    while (1)
        ;
}

void debug_exception(TrapFrame const* trapFrame, const char* message)
{
    /*while (1)
    {
        asm volatile("cli");

        tty_acquire();

        uint32_t oldRow = tty_get_row();
        uint32_t oldColumn = tty_get_column();

        debug_start(message);

        debug_move("Trap Frame", 0, 0);
        if (trapFrame != NULL)
        {
            debug_print("Vector = ", trapFrame->vector);
            debug_print("Error Code = ", trapFrame->errorCode);
            debug_print("RIP = ", trapFrame->rip);
            debug_print("RSP = ", trapFrame->rsp);
            debug_print("RFLAGS = ", trapFrame->rflags);
            debug_print("CS = ", trapFrame->cs);
            debug_print("SS = ", trapFrame->ss);

            debug_move("Registers", 2, 0);
            debug_print("R9 = ", trapFrame->r9);
            debug_print("R8 = ", trapFrame->r8);
            debug_print("RBP = ", trapFrame->rbp);
            debug_print("RDI = ", trapFrame->rdi);
            debug_print("RSI = ", trapFrame->rsi);
            debug_print("RDX = ", trapFrame->rdx);
            debug_print("RCX = ", trapFrame->rcx);
            debug_print("RBX = ", trapFrame->rbx);
            debug_print("RAX = ", trapFrame->rax);

            debug_move(NULL, 3, 0);
            debug_print("CR2 = ", cr2_read());
            debug_print("CR3 = ", cr3_read());
            debug_print("CR4 = ", cr4_read());
            debug_print("R15 = ", trapFrame->r15);
            debug_print("R14 = ", trapFrame->r14);
            debug_print("R13 = ", trapFrame->r13);
            debug_print("R12 = ", trapFrame->r12);
            debug_print("R11 = ", trapFrame->r11);
            debug_print("R10 = ", trapFrame->r10);
        }
        else
        {
            tty_print("Panic occurred outside of interrupt");
        }

        debug_move("Memory", 0, 13);
        debug_print("Locked Pages = ", pmm_reserved_amount());
        debug_print("Unlocked Pages = ", pmm_free_amount());

        debug_move("Other", 2, 13);
        debug_print("Current Time = ", time_uptime());
        debug_print("Cpu Id = ", smp_self()->id);

        tty_set_scale(1);
        tty_set_row(oldRow);
        tty_set_column(oldColumn);

        tty_release();

        smp_send_ipi_to_others(IPI_HALT);
    }*/
    while (1)
        ;
}
