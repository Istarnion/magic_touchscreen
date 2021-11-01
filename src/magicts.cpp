#include "magicts.h"

#include <stdlib.h> // NULL, malloc, free
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

struct TouchscreenContext
{
	int filedescriptor;
	libevdev *dev;
	float minx, miny;
	float maxx, maxy;

	int slot;
	TouchData touches;
};

static bool
supports_mt_events(libevdev *dev)
{
	bool result = libevdev_has_event_code(dev, EV_ABS, ABS_MT_SLOT)        &&
		libevdev_has_event_code(dev, EV_ABS, ABS_MT_TRACKING_ID) &&
		libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_X)  &&
		libevdev_has_event_code(dev, EV_ABS, ABS_MT_POSITION_Y);
	return result;
}

/* Return file descriptor for the opened device, or 0 */
static int
get_device(libevdev **dev)
{
	int fd = 0;

    char candidate[32];
    for(int i=0; i<100; ++i)
    {
        snprintf(candidate, 32, "/dev/input/event%d", i);
        struct stat fileinfo;
        stat(candidate, &fileinfo);
        if(S_ISCHR(fileinfo.st_mode))
        {
            printf("%s\n", candidate);
            fd = open(candidate, O_RDONLY);
            if (fd >= 0)
            {
                int rc = libevdev_new_from_fd(fd, dev);
                if (rc < 0)
                {
                    fprintf(stderr, "Failed to init libevdev (%s)\n", strerror(-rc));
                    fd = 0;
                    break;
                }

                if(supports_mt_events(*dev))
                {
                    printf("Found wanted device at %s\n", candidate);
                    break;
                }

                libevdev_free(*dev);
                *dev = NULL;
                fd = 0;
            }
            else
            {
                fd = 0;
            }
		}

	}

	return fd;
}

static void
get_info(libevdev *dev, int axis, float *min, float *max)
{
	const struct input_absinfo *abs;
	abs = libevdev_get_abs_info(dev, axis);
	*min = (float)abs->minimum;
	*max = (float)abs->maximum;
}

static void
read_event(libevdev *dev, struct input_event *ev)
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
		TouchData *touches)
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
	TouchscreenContext *ctx = (TouchscreenContext *)malloc(sizeof(TouchscreenContext));

	if(ctx)
	{
		ctx->dev = NULL;
		ctx->filedescriptor = get_device(&ctx->dev);
		get_info(ctx->dev, ABS_MT_POSITION_X, &ctx->minx, &ctx->maxx);
		get_info(ctx->dev, ABS_MT_POSITION_Y, &ctx->miny, &ctx->maxy);

		for(int i=0; i<NUM_TOUCHES; ++i)
		{
			ctx->touches.id[i] = -1;
		}
	}

	return ctx;
}

TouchData
magicts_update(void *ctxPtr)
{
	TouchscreenContext *ctx =  (TouchscreenContext *)ctxPtr;

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
		TouchscreenContext *ctx =  (TouchscreenContext *)ctxPtr;
		libevdev_free(ctx->dev);
		close(ctx->filedescriptor);
	}

	free(ctxPtr);
}

