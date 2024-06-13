#include "compositor.h"

#include "_AUX/rect_t.h"
#include "list.h"
#include "lock.h"
#include "queue.h"
#include "sched.h"
#include "sys/win.h"
#include "sysfs.h"
#include "tty.h"

#include <errno.h>
#include <stdlib.h>

static Lock lock;

static List windows;
static Resource compositor;

static GopBuffer frontbuffer;
static pixel_t* backbuffer;

static _Atomic(bool) redrawNeeded;

static void message_queue_init(MessageQueue* queue)
{
    memset(queue->queue, 0, sizeof(queue->queue));
    queue->readIndex = 0;
    queue->writeIndex = 0;
    lock_init(&queue->lock);
}

static void message_queue_push(MessageQueue* queue, msg_t type, uint64_t size, void* data)
{
    LOCK_GUARD(&queue->lock);

    queue->queue[queue->writeIndex] = (Message){
        .size = size,
        .type = type,
    };
    queue->writeIndex = (queue->writeIndex + 1) % MESSAGE_QUEUE_MAX;
}

static bool message_queue_pop(MessageQueue* queue, Message* out)
{
    LOCK_GUARD(&queue->lock);

    if (queue->readIndex == queue->writeIndex)
    {
        return false;
    }

    *out = queue->queue[queue->readIndex];
    queue->readIndex = (queue->readIndex + 1) % MESSAGE_QUEUE_MAX;

    return true;
}

static void window_cleanup(File* file)
{
    Window* window = file->internal;

    list_remove(window);
    free(window->buffer);
    free(window);
}

static uint64_t window_ioctl(File* file, uint64_t request, void* buffer, uint64_t length)
{
    Window* window = file->internal;

    switch (request)
    {
    case IOCTL_WIN_DISPATCH:
    {
        if (length != sizeof(ioctl_win_dispatch_t))
        {
            return ERROR(EINVAL);
        }

        ioctl_win_dispatch_t* dispatch = buffer;

        Message message;
        if (SCHED_WAIT(message_queue_pop(&window->messages, &message), dispatch->timeout) == ERR)
        {
            return ERR;
        }

        memcpy(dispatch->data, message.data, message.size);
        dispatch->type = MSG_INIT;

        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t window_flush(File* file, const void* buffer, uint64_t size, const rect_t* rectIn)
{
    Window* window = file->internal;
    LOCK_GUARD(&window->lock);

    if (size != window->width * window->height * sizeof(pixel_t))
    {
        return ERROR(EINVAL);
    }

    if (rectIn == NULL)
    {
        memcpy(window->buffer, buffer, size);
    }
    else
    {
        volatile rect_t rect = *rectIn;
        if (rect.right > frontbuffer.width || rect.bottom > frontbuffer.height || rect.left > frontbuffer.width ||
            rect.top > frontbuffer.height)
        {
            return ERROR(EINVAL);
        }

        for (uint64_t y = rect.top; y < rect.bottom; y++)
        {
            uint64_t offset = y * sizeof(pixel_t) * window->width + rect.left * sizeof(pixel_t);

            memcpy((void*)((uint64_t)window->buffer + offset), (void*)((uint64_t)buffer + offset),
                (rect.right - rect.left) * sizeof(pixel_t));
        }
    }

    atomic_store(&redrawNeeded, true);
    return 0;
}

static uint64_t compositor_ioctl(File* file, uint64_t request, void* buffer, uint64_t length)
{
    LOCK_GUARD(&lock);

    switch (request)
    {
    case IOCTL_WIN_INIT:
    {
        // Check if write has already been successfully called.
        if (file->internal != NULL)
        {
            return ERROR(EBUSY);
        }

        if (length != sizeof(ioctl_win_init_t))
        {
            return ERROR(EINVAL);
        }

        const ioctl_win_init_t* info = buffer;
        if (info->width > frontbuffer.width || info->height > frontbuffer.height || info->x > frontbuffer.width ||
            info->y > frontbuffer.height || info->x + info->width > frontbuffer.width ||
            info->y + info->height > frontbuffer.height)
        {
            return ERROR(EINVAL);
        }

        Window* window = malloc(sizeof(Window));
        list_entry_init(&window->base);
        window->x = info->x;
        window->y = info->y;
        window->width = info->width;
        window->height = info->height;
        window->buffer = calloc(info->width * info->height, sizeof(pixel_t));
        lock_init(&window->lock);
        message_queue_init(&window->messages);
        LOCK_GUARD(&window->lock);

        file->internal = window;
        file->cleanup = window_cleanup;
        file->methods.flush = window_flush;
        file->methods.ioctl = window_ioctl;

        message_queue_push(&window->messages, MSG_INIT, 0, NULL);

        list_push(&windows, window);
        atomic_store(&redrawNeeded, true);
        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t compositor_open(Resource* resource, File* file)
{
    file->methods.ioctl = compositor_ioctl;
    return 0;
}

static void compositor_draw_windows(void)
{
    LOCK_GUARD(&lock);

    Window* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);

        // Copy one line at a time from window buffer to backbuffer
        for (uint64_t y = 0; y < window->height; y++)
        {
            uint64_t bufferOffset =
                (window->x * sizeof(pixel_t)) + (y + window->y) * sizeof(pixel_t) * frontbuffer.pixelsPerScanline;
            uint64_t windowOffset = y * sizeof(pixel_t) * window->width;

            memcpy((void*)((uint64_t)backbuffer + bufferOffset), (void*)((uint64_t)window->buffer + windowOffset),
                window->width * sizeof(pixel_t));
        }
    }
}

static void compositor_loop(void)
{
    while (1)
    {
        memset(backbuffer, 100, frontbuffer.size);
        compositor_draw_windows();
        memcpy(frontbuffer.base, backbuffer, frontbuffer.size);

        SCHED_WAIT(atomic_load(&redrawNeeded), UINT64_MAX);
        atomic_store(&redrawNeeded, false);
    }
}

void compositor_init(GopBuffer* gopBuffer)
{
    tty_start_message("Compositor initializing");

    frontbuffer = *gopBuffer;
    frontbuffer.base = vmm_kernel_map(NULL, gopBuffer->base, gopBuffer->size);
    backbuffer = malloc(frontbuffer.size);

    list_init(&windows);
    lock_init(&lock);
    atomic_init(&redrawNeeded, true);

    resource_init(&compositor, "win", compositor_open, NULL);
    sysfs_expose(&compositor, "/srv");

    sched_thread_spawn(compositor_loop, THREAD_PRIORITY_MAX);

    tty_end_message(TTY_MESSAGE_OK);
}
