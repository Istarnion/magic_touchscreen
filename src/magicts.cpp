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
#include <dirent.h>

/*
 * Context for a single touch screen
 */
struct TouchscreenContext
{
    int filedescriptor;
    struct libevdev *dev;
    float minx, miny;
    float maxx, maxy;

    int slot;
    struct TouchData touches;
    char screen_id[SCREEN_ID_LENGTH];
};

/*
 * The library context. Just holds several touch screen contexts
 */
struct MagicTouchContext
{
    int screencount;
    TouchscreenContext screen_contexts[MAX_TOUCHSCREENS];
};

/*
 * This struct is means to be a list of all touch screens available.
 * See get_devices() for where this list is created.
 */
struct TouchscreenCandidates
{
    int count;
    char filepath[MAX_TOUCHSCREENS][64];
    int fd[MAX_TOUCHSCREENS];
    libevdev *dev[MAX_TOUCHSCREENS];
    char id[MAX_TOUCHSCREENS][SCREEN_ID_LENGTH];
};

/*
 * Check if string starts with prefix
 */
static inline bool
prefix_match(const char *prefix, const char *string)
{
    bool result = true;
    while(*prefix && *string)
    {
        if(*prefix++ != *string++)
        {
            result = false;
            break;
        }
    }

    return result;
}

/*
 * Check to see if the libevdev device supports the required multitouch
 * event types.
 */
static bool
supports_mt_events(struct libevdev *dev)
{
    bool result = libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)        &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID) &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X)  &&
                  libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y);
    return result;
}

/*
 * Get a list of all multitouch devices on the system.
 * The devices will be opened and initialized. See close_device_list()
 * for a way to close them all, or call libevdev_free() and close() on
 * each device you want to close (in that order).
 */
static TouchscreenCandidates
get_devices(void)
{
    TouchscreenCandidates candidates = { 0 };

    char candidate_name[64];
    strcpy(candidate_name, "/dev/input/");
    int candidate_name_offset = strlen(candidate_name);

    DIR *eventdir = opendir("/dev/input");
    if(eventdir)
    {
        errno = 0;
        struct dirent *entry = NULL;
        while((entry = readdir(eventdir)))
        {
            if(prefix_match("event", entry->d_name))
            {
                strncpy(candidate_name+candidate_name_offset, entry->d_name, 64 - candidate_name_offset);
                struct stat fileinfo;
                int error = stat(candidate_name, &fileinfo);

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
                    int fd = open(candidate_name, O_RDONLY);
                    if (fd >= 0)
                    {
                        libevdev *dev = NULL;
                        int rc = libevdev_new_from_fd(fd, &dev);
                        if (rc < 0)
                        {
                            fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
                        }

                        if(supports_mt_events(dev))
                        {
                            candidates.fd[candidates.count] = fd;
                            candidates.dev[candidates.count] = dev;
                            strncpy(candidates.id[candidates.count], libevdev_get_uniq(dev), 31);
                            strcpy(candidates.filepath[candidates.count], candidate_name);
                            ++candidates.count;

                            if(candidates.count < MAX_TOUCHSCREENS)
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
                            libevdev_free(dev);
                            close(fd);
                        }
                    }
                }
            }
        }

        closedir(eventdir);
    }
    else
    {
        perror("Failed to open /dev/input/");
    }

    return candidates;
}

/*
 * Close all initialized devices in the TouchscreenCandidates list
 */
static void
close_device_list(TouchscreenCandidates *candidates)
{
    for(int i=0; i<candidates->count; ++i)
    {
        libevdev_free(candidates->dev[i]);
        close(candidates->fd[i]);
    }

    memset(candidates, 0, sizeof(TouchscreenCandidates));
}

/*
 * Get info about the axis min and max values for a touchscreen
 */
static void
get_info(struct libevdev *dev, int axis, float *min, float *max)
{
    const struct input_absinfo *abs;
    abs = libevdev_get_abs_info(dev, axis);
    *min = (float)abs->minimum;
    *max = (float)abs->maximum;
}

/*
 * Read the next event for a libevdev device.
 */
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

/*
 * Fetch and handle a packet of events.
 * libevdev packages multitouch events as SLOT, ID, X, Y, and REPORT
 * so a single touch is at least 5 events.
 */
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
magicts_initialize(const char *id)
{
    struct MagicTouchContext *ctx =
        (struct MagicTouchContext *)malloc(sizeof(struct MagicTouchContext));
    memset(ctx, 0, sizeof(struct MagicTouchContext));

    if(ctx)
    {
        TouchscreenCandidates candidates = get_devices();

        for(int i=0; i<candidates.count; ++i)
        {
            if(id == NULL || strcmp(id, candidates.id[i]) == 0)
            {
                TouchscreenContext *screen = &ctx->screen_contexts[ctx->screencount];
                screen->filedescriptor = candidates.fd[i];
                screen->dev = candidates.dev[i];

                get_info(screen->dev, ABS_MT_POSITION_X, &screen->minx, &screen->maxx);
                get_info(screen->dev, ABS_MT_POSITION_Y, &screen->miny, &screen->maxy);

                strncpy(screen->screen_id, candidates.id[i], SCREEN_ID_LENGTH);

                for(int j=0; j<NUM_TOUCHES; ++j)
                {
                    screen->touches.id[j] = -1;
                }

                printf("Initialized touchscreen %s\n", screen->screen_id);
                ++ctx->screencount;
            }
            else
            {
                libevdev_free(candidates.dev[i]);
                close(candidates.fd[i]);
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
magicts_get_screencount()
{
    TouchscreenCandidates screens = get_devices();
    int result = screens.count;
    close_device_list(&screens);
    return result;
}

struct MagicTouchScreenScreenIDList
magicts_get_screenids()
{
    MagicTouchScreenScreenIDList result = { 0 };
    TouchscreenCandidates screens = get_devices();
    result.count = screens.count;

    for(int i=0; i<screens.count; ++i)
    {
        strncpy(result.ids + i * SCREEN_ID_LENGTH, screens.id[i], 31);
    }

    close_device_list(&screens);
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
            handle_packet(screen->dev,
                          &screen->slot,
                          screen->minx, screen->maxx, screen->miny, screen->maxy,
                          &screen->touches);
        }

        /*
         * Though we support NUM_TOUCHES touches on each screen,
         * we also only expose NUM_TOUCHES externally, no matter
         * how many screens we have. To support this, we divide the
         * available external touches amongst the screens.
         * This works as long as the assumption that the slots used
         * for each screen is < NUM_TOUCHES / ctx->screencount holds,
         * which it seems to do (as long as the hardware limit of touches
         * times the number of screens is < NUM_TOUCHES).
         * For NUM_TOUCHES == 100 and a hardware limit of 10 touches per
         * screens, we're fine with up to 10 screens.
         */
        int offset = i * (NUM_TOUCHES / ctx->screencount);
        for(int j=0; j<NUM_TOUCHES; ++j)
        {
            int slot = offset + j;
            if(slot >= NUM_TOUCHES) break;

            strncpy(touches.screen[slot], screen->screen_id, SCREEN_ID_LENGTH);
            if(screen->touches.id[j] >= 0)
            {
                touches.id[slot] = screen->touches.id[j];
                touches.x[slot] = screen->touches.x[j];
                touches.y[slot] = screen->touches.y[j];
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
            libevdev_free(ctx->screen_contexts[i].dev);
            close(ctx->screen_contexts[i].filedescriptor);
        }

        free(ctxPtr);
    }
}

