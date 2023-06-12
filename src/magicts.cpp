#include "magicts.h"

#include <stdlib.h> /* NULL, malloc, free */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libevdev/libevdev.h>

struct TouchscreenContext
{
    int filedescriptor;
    struct libevdev *dev;
    float minx, miny;
    float maxx, maxy;

    int slot;
    struct TouchData touches;
    char screen_id[32];
};

#define MAX_TOUCHSCREENS 16

struct MagicTouchContext
{
    int screencount;
    char *screen_ids[MAX_TOUCHSCREENS];
    TouchscreenContext screen_contexts[MAX_TOUCHSCREENS];
};

static bool
supports_mt_events(struct libevdev *dev)
{
    bool result = libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)        &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID) &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X)  &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y);
    return result;
}

struct TouchscreenCandidate
{
    int filedescriptor;
    struct libevdev *dev;
};

/* Return the number of candidates found */
static int
get_devices(int candidatecount, struct TouchscreenCandidate *candidates)
{
    int touchscreens_found = 0;

    /*
     * event stream files are found as /dev/input/eventN,
     * but we don't know N. So we loop from 0 and up until
     * we find an event stream that supports the required
     * event types, or we find no more event streams.
     * The hardcoded '1000' is just for sanity, there are
     * typically only ~10 event streams.
     * Another option is to use dirent to iterate over the
     * actual files in the directory, or glob to get all the
     * files matching the pattern, but both are more complex/
     * uses dynamic memory allocation, and offer little benefit.
     * If someone reads this and knows a more elegant way of finding
     * the wanted event streams, I'd love a PR :)
     */
    char candidate[32]; /* "/dev/input/event" is 16 characters */
    for(int i=0; i<1000; ++i)
    {
        snprintf(candidate, 32, "/dev/input/event%d", i);
        struct stat fileinfo;
        int error = stat(candidate, &fileinfo);

        if(error)
        {
            /*
            * ENOENT means the file path does not exist,
            * so we've looped by all the eventNs and should
            * give up. If the error is something else, we
            * report on stderr
            */
            if(errno != ENOENT)
            {
                fprintf(stderr, "Error testing event candidates: %s\n", strerror(errno));
            }

            break;
        }

        if(S_ISCHR(fileinfo.st_mode))
        {
            printf("Testing candidate %s...", candidate);
            int fd = open(candidate, O_RDONLY);
            if (fd >= 0)
            {
                libevdev *dev = NULL;
                int rc = libevdev_new_from_fd(fd, &dev);
                if (rc < 0)
                {
                    putchar('\n');
                    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
                }

                if(supports_mt_events(dev))
                {
                    printf(" and it supports multitouch events. Using this one.\n");
                    candidates[touchscreens_found].dev = dev;
                    candidates[touchscreens_found].filedescriptor = fd;
                    ++touchscreens_found;
                    if(touchscreens_found < candidatecount)
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    printf(" but it doesn't support multitouch events.\n");
                    libevdev_free(dev);
                }
            }
            else
            {
                printf(" but it can't be opened.\n");
            }
        }
    }

    return touchscreens_found;
}

static void
get_info(struct libevdev *dev, int axis, float *min, float *max)
{
    const struct input_absinfo *abs;
    abs = libevdev_get_abs_info(dev, axis);
    *min = (float)abs->minimum;
    *max = (float)abs->maximum;
}

static void
read_event(struct libevdev *dev, struct input_event *ev)
{
    int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, ev);
    assert(rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC);

    if (rc == LIBEVDEV_READ_STATUS_SYNC)
    {
        /* Re-synchronize */
    }
}

static void
handle_packet(struct libevdev *dev,
              int *slot,
              float minx, float maxx, float miny, float maxy,
              struct TouchData *touches)
{
    struct input_event ev;
    do
    {
        read_event(dev, &ev);
        if(ev.type == EV_ABS)
        {
            switch(ev.code)
            {
                case ABS_MT_SLOT:
                {
                    *slot = ev.value;
                } break;
                case ABS_MT_TRACKING_ID:
                {
                    touches->id[*slot] = ev.value;
                } break;
                case ABS_MT_POSITION_X:
                {
                    float x = (float)ev.value / maxx;
                    touches->x[*slot] = x;
                } break;
                case ABS_MT_POSITION_Y:
                {
                    float y = (float)ev.value / maxy;
                    touches->y[*slot] = y;
                } break;
            }
        }
    }
    while(ev.type != SYN_REPORT);
}

void *
magicts_initialize(void)
{
    struct MagicTouchContext *ctx =
        (struct MagicTouchContext *)malloc(sizeof(struct MagicTouchContext));
    memset(ctx, 0, sizeof(struct MagicTouchContext));

    if(ctx)
    {
        TouchscreenCandidate candidates[MAX_TOUCHSCREENS] = { 0 };
        ctx->screencount = get_devices(MAX_TOUCHSCREENS, candidates);

        for(int i=0; i<ctx->screencount; ++i)
        {
            TouchscreenCandidate *candidate = &candidates[i];
            TouchscreenContext *screen = &ctx->screen_contexts[i];

            screen->filedescriptor = candidate->filedescriptor;
            screen->dev = candidate->dev;

            get_info(screen->dev, ABS_MT_POSITION_X, &screen->minx, &screen->maxx);
            get_info(screen->dev, ABS_MT_POSITION_Y, &screen->miny, &screen->maxy);

            strncpy(screen->screen_id, libevdev_get_uniq(screen->dev), 32);
            ctx->screen_ids[i] = screen->screen_id;

            for(int j=0; j<NUM_TOUCHES; ++j)
            {
                screen->touches.id[j] = -1;
            }
        }

        if(ctx->screencount <= 0)
        {
            fprintf(stderr, "No compatible multi touchscreens found\n");
            free(ctx);
            ctx = NULL;
        }
    }
    else
    {
        fprintf(stderr, "Failed to allocate MagicTouchContext\n");
    }

    return ctx;
}

int
magicts_get_screencount(void *ctxPtr)
{
    int result = 0;
    if(ctxPtr)
    {
        struct MagicTouchContext *ctx = (MagicTouchContext *)ctxPtr;
        result = ctx->screencount;
    }

    return result;
}

char **
magicts_get_screenids(void *ctxPtr)
{
    char **result = 0;
    if(ctxPtr)
    {
        struct MagicTouchContext *ctx = (MagicTouchContext *)ctxPtr;
        result = ctx->screen_ids;
    }

    return result;
}

struct TouchData
magicts_update(void *ctxPtr)
{
    if(!ctxPtr)
    {
        struct TouchData emptyTouchData = { 0 };
        for(int i=0; i<NUM_TOUCHES; ++i)
        {
            emptyTouchData.id[i] = -1;
        }

        return emptyTouchData;
    }

    struct MagicTouchContext *ctx =  (struct MagicTouchContext *)ctxPtr;

    int touchcount = 0;
    TouchData touches = { 0 };

    for(int i=0; i<NUM_TOUCHES; ++i)
    {
        touches.id[i] = -1;
    }

    for(int i=0; i<ctx->screencount; ++i)
    {
        TouchscreenContext *screen = &ctx->screen_contexts[i];
        while(libevdev_has_event_pending(screen->dev))
        {
            puts("Events");
            handle_packet(screen->dev,
                          &screen->slot,
                          screen->minx, screen->maxx, screen->miny, screen->maxy,
                          &screen->touches);
        }

        for(int j=0; j<NUM_TOUCHES; ++j)
        {
            if(screen->touches.id[j] >= 0)
            {
                strncpy(touches.screen[touchcount], screen->screen_id, 32);
                touches.id[touchcount] = screen->touches.id[j];
                touches.x[touchcount] = screen->touches.x[j];
                touches.y[touchcount] = screen->touches.y[j];
                ++touchcount;
            }
        }
    }

    return touches;
}

void
magicts_finalize(void *ctxPtr)
{
    if(ctxPtr)
    {
        struct MagicTouchContext *ctx =  (struct MagicTouchContext *)ctxPtr;
        for(int i=0; i<ctx->screencount; ++i)
        {
            ctx->screen_ids[i] = 0;
            libevdev_free(ctx->screen_contexts[i].dev);
            close(ctx->screen_contexts[i].filedescriptor);

        }

        free(ctxPtr);
    }
}

