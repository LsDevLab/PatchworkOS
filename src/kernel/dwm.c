#include "dwm.h"

#include "_AUX/rect_t.h"
#include "debug.h"
#include "list.h"
#include "lock.h"
#include "sched.h"
#include "sys/io.h"
#include "sys/mouse.h"
#include "sys/win.h"
#include "sysfs.h"
#include "vfs.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/math.h>

// TODO: Implement rectangle subtraction

static resource_t dwm;

static surface_t frontbuffer;
static surface_t backbuffer;
static rect_t clientArea;

static list_t windows;
static lock_t windowsLock;

static list_t panels;
static lock_t panelsLock;

static point_t mousePos;
static file_t* mouse;

static _Atomic(bool) redrawNeeded;

static void dwm_update_client_area(void)
{
    rect_t newArea;
    RECT_INIT_DIM(&newArea, 0, 0, backbuffer.width, backbuffer.height);

    window_t* panel;
    LIST_FOR_EACH(panel, &panels)
    {
        uint64_t leftDist = panel->pos.x + panel->surface.width;
        uint64_t topDist = panel->pos.y + panel->surface.height;
        uint64_t rightDist = backbuffer.width - panel->pos.x;
        uint64_t bottomDist = backbuffer.height - panel->pos.y;

        if (leftDist <= topDist && leftDist <= rightDist && leftDist <= bottomDist)
        {
            newArea.left = MAX(panel->pos.x + panel->surface.width, newArea.left);
        }
        else if (topDist <= leftDist && topDist <= rightDist && topDist <= bottomDist)
        {
            newArea.top = MAX(panel->pos.y + panel->surface.height, newArea.top);
        }
        else if (rightDist <= leftDist && rightDist <= topDist && rightDist <= bottomDist)
        {
            newArea.right = MIN(panel->pos.x, newArea.right);
        }
        else if (bottomDist <= leftDist && bottomDist <= topDist && bottomDist <= rightDist)
        {
            newArea.bottom = MIN(panel->pos.y, newArea.bottom);
        }
    }

    clientArea = newArea;
}

static void dwm_window_cleanup(file_t* file)
{
    window_t* window = file->internal;

    switch (window->type)
    {
    case WIN_WINDOW:
    {
        LOCK_GUARD(&windowsLock);
        list_remove(window);
    }
    break;
    case WIN_PANEL:
    {
        LOCK_GUARD(&panelsLock);
        list_remove(window);

        dwm_update_client_area();
    }
    break;
    }

    window_free(window);
}

static uint64_t dwm_ioctl(file_t* file, uint64_t request, void* buffer, uint64_t length)
{
    switch (request)
    {
    case IOCTL_DWM_CREATE:
    {
        if (length != sizeof(ioctl_dwm_create_t))
        {
            return ERROR(EINVAL);
        }
        const ioctl_dwm_create_t* create = buffer;

        window_t* window = window_new(&create->pos, create->width, create->height, create->type, file, dwm_window_cleanup);
        if (window == NULL)
        {
            return ERR;
        }

        LOCK_GUARD(&window->lock);

        switch (window->type)
        {
        case WIN_WINDOW:
        {
            LOCK_GUARD(&windowsLock);
            list_push(&windows, window);
        }
        break;
        case WIN_PANEL:
        {
            LOCK_GUARD(&panelsLock);
            list_push(&panels, window);

            dwm_update_client_area();
        }
        break;
        default:
        {
            debug_panic("Invalid window type");
        }
        }

        dwm_redraw();
        return 0;
    }
    case IOCTL_DWM_SIZE:
    {
        if (length != sizeof(ioctl_dwm_size_t))
        {
            return ERROR(EINVAL);
        }

        ioctl_dwm_size_t* size = buffer;
        size->outWidth = RECT_WIDTH(&clientArea);
        size->outHeight = RECT_HEIGHT(&clientArea);

        return 0;
    }
    default:
    {
        return ERROR(EREQ);
    }
    }
}

static uint64_t dwm_open(resource_t* resource, file_t* file)
{
    file->ops.ioctl = dwm_ioctl;
    return 0;
}

static void dwm_draw_panels(void)
{
    LOCK_GUARD(&panelsLock);

    window_t* panel;
    LIST_FOR_EACH(panel, &panels)
    {
        LOCK_GUARD(&panel->lock);

        if (!panel->invalid)
        {
            continue;
        }
        panel->invalid = false;

        rect_t screenRect;
        RECT_INIT_DIM(&screenRect, 0, 0, backbuffer.width, backbuffer.height);

        rect_t destRect;
        RECT_INIT_DIM(&destRect, panel->pos.x, panel->pos.y, panel->surface.width, panel->surface.height);

        if (!RECT_CONTAINS(&screenRect, &destRect))
        {
            continue;
        }

        point_t srcPoint = {0};
        gfx_transfer(&backbuffer, &panel->surface, &destRect, &srcPoint);
    }
}

static void dwm_draw_windows(void)
{
    LOCK_GUARD(&windowsLock);

    window_t* window;
    LIST_FOR_EACH(window, &windows)
    {
        LOCK_GUARD(&window->lock);

        rect_t destRect;
        RECT_INIT_DIM(&destRect, window->pos.x + clientArea.left, window->pos.y + clientArea.top, window->surface.width,
            window->surface.height);
        RECT_FIT(&destRect, &clientArea);

        point_t srcPoint = {
            .x = destRect.left - (window->pos.x + clientArea.left),
            .y = destRect.top - (window->pos.y + clientArea.top),
        };

        gfx_transfer(&backbuffer, &window->surface, &destRect, &srcPoint);
    }
}

static void dwm_poll(void)
{
    while (!atomic_load(&redrawNeeded))
    {
        poll_file_t poll[] = {(poll_file_t){.file = mouse, .requested = POLL_READ}};
        if (vfs_poll(poll, 1, SEC / CONFIG_DWM_FPS) != 0)
        {
            mouse_event_t event;
            FILE_CALL(mouse, read, &event, sizeof(mouse_event_t));

            rect_t cursor;
            RECT_INIT_DIM(&cursor, mousePos.x, mousePos.y, 32, 32);
            gfx_transfer(&frontbuffer, &backbuffer, &cursor, &mousePos);

            mousePos.x += event.deltaX;
            mousePos.y += event.deltaY;
            mousePos.x = CLAMP(mousePos.x, 0, backbuffer.width - 32);
            mousePos.y = CLAMP(mousePos.y, 0, backbuffer.height - 32);

            RECT_INIT_DIM(&cursor, mousePos.x, mousePos.y, 32, 32);
            gfx_rect(&frontbuffer, &cursor, 0xFF00FF00);
        }
    }
    atomic_store(&redrawNeeded, false);
}

static void dwm_loop(void)
{
    while (1)
    {
        // Temp
        gfx_rect(&backbuffer, &clientArea, 0xFF007E81);

        dwm_draw_windows();
        dwm_draw_panels();

        for (uint64_t y = 0; y < backbuffer.height; y++)
        {
            memcpy(&frontbuffer.buffer[y * frontbuffer.stride], &backbuffer.buffer[y * backbuffer.stride],
                backbuffer.width * sizeof(pixel_t));
        }

        rect_t cursor;
        RECT_INIT_DIM(&cursor, mousePos.x, mousePos.y, 32, 32);
        gfx_rect(&frontbuffer, &cursor, 0xFF00FF00);

        dwm_poll();
    }
}

void dwm_init(gop_buffer_t* gopBuffer)
{
    frontbuffer.buffer = gopBuffer->base;
    frontbuffer.height = gopBuffer->height;
    frontbuffer.width = gopBuffer->width;
    frontbuffer.stride = gopBuffer->stride;

    backbuffer.buffer = calloc(gopBuffer->width * gopBuffer->height, sizeof(pixel_t));
    backbuffer.height = gopBuffer->height;
    backbuffer.width = gopBuffer->width;
    backbuffer.stride = backbuffer.width;

    RECT_INIT_DIM(&clientArea, 0, 0, backbuffer.width, backbuffer.height);
    list_init(&windows);
    lock_init(&windowsLock);
    list_init(&panels);
    lock_init(&panelsLock);
    atomic_init(&redrawNeeded, true);

    mousePos.x = backbuffer.width / 2;
    mousePos.y = backbuffer.height / 2;
    mouse = vfs_open("sys:/mouse/ps2");

    resource_init(&dwm, "dwm", dwm_open, NULL);
    sysfs_expose(&dwm, "/server");
}

void dwm_start(void)
{
    sched_thread_spawn(dwm_loop, THREAD_PRIORITY_MAX);
}

void dwm_redraw(void)
{
    atomic_store(&redrawNeeded, true);
}
