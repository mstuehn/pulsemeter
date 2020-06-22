#define _GNU_SOURCE /* for asprintf */

#include "evdev.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <linux/input.h>

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

static int
is_event_device(const struct dirent *dir) {
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

static bool
does_device_match( int fd, int vendor, int product )
{
    int version;
    unsigned short id[4];

    if (ioctl(fd, EVIOCGVERSION, &version)) {
        perror("can't get version");
        return 1;
    }
    printf("Input driver version is %d.%d.%d\n",
            version >> 16, (version >> 8) & 0xff, version & 0xff);

    ioctl(fd, EVIOCGID, id);
    printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
            id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

    return (id[ID_VENDOR] == vendor) && (id[ID_PRODUCT] == product);
}

char*
scan_devices( uint16_t vendor, uint16_t product )
{
    printf("Scanning for compatible Vendor/Product:  0x%04X/0x%04X\n", vendor, product );

    struct dirent **namelist;
    int ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort);
    if( ndev <= 0 ) {
        fprintf(stderr, "No devices found\n" );
        return NULL;
    }

    printf("Available devices:\n");

    char fname[64];
    bool found = false;
    for( int i = 0; i < ndev && !found; i++) {
        char name[256] = "???";

        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
        int fd = open(fname, O_RDONLY);
        if (fd >= 0) {
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            if( does_device_match( fd, vendor, product )) {
                printf("Device found: %s \n", fname );
                found = true;
            }
            close(fd);
        }
        printf("%s:  %s\n", fname, name);
        free(namelist[i]);
    }

    if( !found ) {
        printf("No devices found, wait 5s\n");
        sleep(5);
        return NULL;
    }

    char *filename;
    asprintf(&filename, "%s", fname );
    return filename;
}

bool
get_events(int fd, uint16_t type, uint16_t* code, uint16_t* value)
{
    struct input_event ev;
    ssize_t size = read(fd, &ev, sizeof(struct input_event));

    if( size < sizeof(struct input_event )) {
        printf("expected %lu bytes, got %li\n", sizeof(struct input_event), size);
        perror("\nerror reading");
        return false;
    }

    //printf("Event: time %ld.%06ld, ", ev.time.tv_sec, ev.time.tv_usec);
    //printf("type: %i, code: %i, value: %i\n", ev.type, ev.code, ev.value);

    if( type != ev.type ) return false;

    *code = ev.code;
    *value = ev.value;

    return true;
}

