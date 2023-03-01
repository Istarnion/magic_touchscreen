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

/* Return file descriptor for the opened device, or 0 */
static int
get_device(struct libevdev **dev)
{
    int fd = 0;

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
            fd = open(candidate, O_RDONLY);
            if (fd >= 0)
            {
                int rc = libevdev_new_from_fd(fd, dev);
                if (rc < 0)
                {
                    putchar('\n');
                    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
                    fd = 0;
                    break;
                }

                if(supports_mt_events(*dev))
                {
                    printf(" and it supports multitouch events. Using this one.\n");
                    break;
                }
                else
                {
                    printf(" but it doesn't support multitouch events.\n");
                }

                libevdev_free(*dev);
                *dev = NULL;
                fd = 0;
            }
            else
            {
                printf(" but it can't be opened.\n");
                fd = 0;
            }
        }
    }

    return fd;
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
    struct TouchscreenContext *ctx =
        (struct TouchscreenContext *)malloc(sizeof(struct TouchscreenContext));

    if(ctx)
    {
        ctx->dev = NULL;
        ctx->filedescriptor = get_device(&ctx->dev);
        if(ctx->filedescriptor)
        {
            ctx->slot = 0;
            get_info(ctx->dev, ABS_MT_POSITION_X, &ctx->minx, &ctx->maxx);
            get_info(ctx->dev, ABS_MT_POSITION_Y, &ctx->miny, &ctx->maxy);

            for(int i=0; i<NUM_TOUCHES; ++i)
            {
                ctx->touches.id[i] = -1;
            }
        }
        else
        {
            fprintf(stderr, "No compatible multi touchscreen found\n");
            free(ctx);
            ctx = NULL;
        }
    }
    else
    {
        fprintf(stderr, "Failed to allocate TouchscreenContext\n");
    }

    return ctx;
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

    struct TouchscreenContext *ctx =  (struct TouchscreenContext *)ctxPtr;

    while(libevdev_has_event_pending(ctx->dev))
    {
        handle_packet(ctx->dev,
                      &ctx->slot,
                      ctx->minx, ctx->maxx, ctx->miny, ctx->maxy,
                      &ctx->touches);
    }

    return ctx->touches;
}

void
magicts_finalize(void *ctxPtr)
{
    if(ctxPtr)
    {
        struct TouchscreenContext *ctx =  (struct TouchscreenContext *)ctxPtr;
        libevdev_free(ctx->dev);
        close(ctx->filedescriptor);
        free(ctxPtr);
    }
}

