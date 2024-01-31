#if 0
make
clang "$0" -I src -o magicts_test libmagicts.so -Wl,-rpath -Wl,.
exit
#endif

#include "src/magicts.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

volatile bool running = true;

void
handle_sigint(int sig)
{
    running = false;
}

int
main(int num_args, char *args[])
{
    signal(SIGINT, handle_sigint);

    int screencount = magicts_get_screencount();
    printf("%d screen(s) connected\n", screencount);

    printf("Screens:\n");
    MagicTouchScreenScreenIDList screen_ids = magicts_get_screenids();
    for(int i=0; i<screencount; ++i)
    {
        printf("  %s\n", screen_ids.ids + i*32);
    }

    void *magicts_context = magicts_initialize(NULL);
    if(!magicts_context)
    {
        printf("Failed to initialize any touch screens!\n");
        return 1;
    }

    while(running)
    {
        TouchData touches = magicts_update(magicts_context);
        /*
        for(int i=0; i<NUM_TOUCHES; ++i)
        {
            if(touches.id[i] >= 0)
            {
                printf("[%d]: X: %1.2f, Y: %1.2f (%d, %s)\n",
                        i,
                        touches.x[i], touches.y[i],
                        touches.id[i],
                        touches.screen[i]);
            }
        }
        */

        usleep(83);
    }

    magicts_finalize(magicts_context);
    return 0;
}

