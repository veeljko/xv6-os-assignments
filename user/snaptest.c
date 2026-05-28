#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"

#define PAGE_SIZE 4096

void
print_result(char *name, int got, int expected)
{
    if(got == expected)
        printf("[OK] %s -> %d\n", name, got);
    else
        printf("[FAIL] %s -> got %d, expected %d\n", name, got, expected);
}

void
print_result_nonneg(char *name, int got)
{
    if(got >= 0)
        printf("[OK] %s -> %d\n", name, got);
    else
        printf("[FAIL] %s -> got %d, expected >= 0\n", name, got);
}

void
fill_page(char *buf, int off, char value)
{
    int i;

    for(i = 0; i < PAGE_SIZE; i++)
        buf[off + i] = value;
}

void
test_basic_errors(void)
{
    int r;
    struct snap_info info;

    printf("\n--- Basic error tests ---\n");

    r = snap_empty();
    print_result("snap_empty initial", r, 0);

    r = snap_take(-1);
    print_result("snap_take(-1)", r, -2);

    r = snap_take(4);
    print_result("snap_take(4)", r, -2);

    r = snap_restore(0);
    print_result("snap_restore(0) on empty slot", r, -1);

    r = snap_release(0);
    print_result("snap_release(0) on empty slot", r, -1);

    r = snap_status(0, &info);
    print_result("snap_status(0) on empty slot", r, -1);

    r = snap_diff(0, 1);
    print_result("snap_diff(0, 1) on empty slots", r, -1);
}

void
test_restore_with_file_flag(void)
{
    int r;
    int fd;
    char *buf;
    struct snap_info info;

    printf("\n--- Restore test with file flag ---\n");

    /*
     * Fajl koristimo kao flag jer snap_restore vraca user memoriju,
     * ali ne vraca stanje fajl sistema.
     */
    unlink("snapflag");

    buf = malloc(9000);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    fill_page(buf, 0, 'A');
    fill_page(buf, 4096, 'B');
    buf[8192] = 'C';

    printf("Before snap_take: buf[0]=%d buf[4096]=%d buf[8192]=%d\n",
           buf[0], buf[4096], buf[8192]);

    r = snap_take(0);
    print_result("snap_take(0)", r, 0);

    /*
     * Ako fajl ne postoji, ovo je prvi prolazak.
     * Ako fajl postoji, to znaci da smo se vratili iz snap_restore.
     */
    fd = open("snapflag", O_RDONLY);

    if(fd < 0){
        printf("First pass after snap_take\n");

        r = snap_take(0);
        print_result("snap_take(0) again, occupied slot", r, -3);

        r = snap_status(0, &info);
        print_result("snap_status(0)", r, 0);

        if(r == 0){
            printf("slot 0: size=%d num_pages=%d\n",
                   info.size, info.num_pages);
        }

        /*
         * Menjamo vise stranica da bi snap_diff imao sta da nadje.
         */
        buf[0] = 'X';
        buf[4096] = 'Y';
        buf[8192] = 'Z';

        r = snap_take(1);
        print_result("snap_take(1)", r, 0);

        r = snap_diff(0, 1);
        print_result_nonneg("snap_diff(0, 1)", r);

        if(r > 0)
            printf("[OK] snapshots are different\n");
        else
            printf("[FAIL] snapshots should be different\n");

        printf("Before restore: buf[0]=%d buf[4096]=%d buf[8192]=%d\n",
               buf[0], buf[4096], buf[8192]);

        fd = open("snapflag", O_CREATE | O_WRONLY);
        if(fd < 0){
            printf("[FAIL] could not create snapflag\n");
            exit();
        }
        close(fd);

        r = snap_restore(0);


        printf("snap_restore(0) returned %d\n", r);
        exit();
    } else {
        close(fd);

        printf("Second pass after snap_restore\n");

        printf("After restore: buf[0]=%d buf[4096]=%d buf[8192]=%d\n",
               buf[0], buf[4096], buf[8192]);

        if(buf[0] == 'A' && buf[4096] == 'B' && buf[8192] == 'C'){
            printf("[OK] memory restored correctly\n");
        } else {
            printf("[FAIL] memory was not restored correctly\n");
        }

        r = snap_release(0);
        print_result("snap_release(0)", r, 0);

        r = snap_release(1);
        print_result("snap_release(1)", r, 0);

        r = snap_release(1);
        print_result("snap_release(1) again", r, -1);

        unlink("snapflag");
    }
}

void
test_fork_inheritance(void)
{
    int r;
    int pid;
    char *buf;
    struct snap_info info;

    printf("\n--- Fork inheritance test ---\n");

    buf = malloc(5000);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    buf[0] = 'P';
    buf[4096] = 'Q';

    r = snap_take(2);
    print_result("parent snap_take(2)", r, 0);

    pid = fork();

    if(pid < 0){
        printf("[FAIL] fork failed\n");
        exit();
    }

    if(pid == 0){
        printf("child: started\n");

        /*
         * Child treba da nasledi snapshot iz slota 2.
         */
        r = snap_status(2, &info);
        print_result("child snap_status(2), inherited snapshot", r, 0);

        if(r == 0){
            printf("child: inherited snapshot size=%d num_pages=%d\n",
                   info.size, info.num_pages);
        }

        /*
         * Child oslobadja svoj snapshot.
         * Ovo ne sme da utice na parent-ov snapshot.
         */
        r = snap_release(2);
        print_result("child snap_release(2)", r, 0);

        r = snap_status(2, &info);
        print_result("child snap_status(2) after release", r, -1);

        printf("child: exiting\n");
        exit();
    }

    wait();

    /*
     * Parent i dalje treba da ima svoj slot 2.
     */
    r = snap_status(2, &info);
    print_result("parent snap_status(2) after child release", r, 0);

    if(r == 0){
        printf("parent: snapshot 2 size=%d num_pages=%d\n",
               info.size, info.num_pages);
    }

    r = snap_release(2);
    print_result("parent snap_release(2)", r, 0);
}

void
test_snap_empty_full_slots(void)
{
    int r;

    printf("\n--- snap_empty full slots test ---\n");

    r = snap_empty();
    print_result("snap_empty before filling", r, 0);

    r = snap_take(0);
    print_result("snap_take(0)", r, 0);

    r = snap_take(1);
    print_result("snap_take(1)", r, 0);

    r = snap_take(2);
    print_result("snap_take(2)", r, 0);

    r = snap_take(3);
    print_result("snap_take(3)", r, 0);

    r = snap_empty();
    print_result("snap_empty when full", r, -1);

    r = snap_release(0);
    print_result("snap_release(0)", r, 0);

    r = snap_release(1);
    print_result("snap_release(1)", r, 0);

    r = snap_release(2);
    print_result("snap_release(2)", r, 0);

    r = snap_release(3);
    print_result("snap_release(3)", r, 0);

    r = snap_empty();
    print_result("snap_empty after cleanup", r, 0);
}

int
main(void)
{
    printf("\n=== SNAPSHOT TEST START ===\n");

    test_basic_errors();

    test_restore_with_file_flag();

    test_fork_inheritance();

    test_snap_empty_full_slots();

    printf("\n=== SNAPSHOT TEST END ===\n");

    exit();
}
