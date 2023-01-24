#if 0
make
clang "$0" -I src -o magicts_test libmagicts.so -Wl,-rpath -Wl,. && ./magicts_test
rm -f magicts_test
exit
#endif

#include "magicts.h"

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

    void *magicts_context = magicts_initialize();

    while(running)
    {
        TouchData touches = magicts_update(magicts_context);
        for(int i=0; i<NUM_TOUCHES; ++i)
        {
            if(touches.id[i] >= 0)
            {
                printf("[%d]: X: %1.2f, Y: %1.2f (%d)\n",
                        i,
                        touches.x[i], touches.y[i],
                        touches.id[i]);
            }
        }

        usleep(166);
    }

    magicts_finalize(magicts_context);
    return 0;
}

