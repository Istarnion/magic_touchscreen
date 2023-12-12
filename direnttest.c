#if 0
cc $0 -o direnttest
./direnttest
rm -f direnttest
exit 0
#endif

#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int
main(int num_args, char *args[])
{
    char full_name[128];
    strcpy(full_name, "/dev/input/");
    int full_name_offset = strlen(full_name);

    DIR *eventdir = opendir("/dev/input");
    if(eventdir)
    {
        errno = 0;
        struct dirent *entry = NULL;
        while((entry = readdir(eventdir)))
        {
            strncpy(full_name+full_name_offset, entry->d_name, 128 - full_name_offset); 
            printf("Type: %d, %s\n", entry->d_type, full_name);
        }

        if(errno)
        {
            perror("Error reading dir entry");
        }
    }
    else
    {
        perror("Failed to open /dev/input/");
    }

    return 0;
}

