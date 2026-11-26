#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"
#define PAGE_SIZE 4096

void
test(void)
{
    int r;
    int fd;
    char *buf;
    struct snap_info info;

    unlink("snapflag");

    buf = malloc(9000);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    buf[8192] = 'C';
    printf("%d\n", snap_num());
    r = snap_take(0);


    fd = open("snapflag", O_RDONLY);
    if(fd < 0){
        printf("First pass after snap_take\n");

        char *buf2;
        buf2 = malloc(PAGE_SIZE);
        r = snap_take(1);
        printf("%d\n", snap_num());

        fd = open("snapflag", O_CREATE | O_WRONLY);
        if(fd < 0){
            printf("[FAIL] could not create snapflag\n");
            exit();
        }
        close(fd);

        r = snap_restore(0);
        printf("%d\n", snap_num());

        printf("snap_restore(0) returned %d\n", r);
        exit();
    } else {
        close(fd);

        printf("Second pass after snap_restore\n");
        char *buf3;
        buf3 = malloc(5000);
        snap_take(3);
        printf("%d\n", snap_num());
        r = snap_release(0);
        r = snap_release(1);
        r = snap_release(2);
        r = snap_release(3);
        printf("%d\n", snap_num());
        unlink("snapflag");
    }
}

int
main(void)
{
    test();
    exit();
}
