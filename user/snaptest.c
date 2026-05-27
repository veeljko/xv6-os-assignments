#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"
#define PAGE_SIZE 4096

void
ok(char *name)
{
    printf("[OK] %s\n", name);
}

void
fail(char *name, int got, int expected)
{
    printf("[FAIL] %s -> got %d, expected %d\n", name, got, expected);
}

void
expect(char *name, int got, int expected)
{
    if(got == expected)
        ok(name);
    else
        fail(name, got, expected);
}

void
expect_nonneg(char *name, int got)
{
    if(got >= 0)
        printf("[OK] %s -> %d\n", name, got);
    else
        printf("[FAIL] %s -> got %d, expected >= 0\n", name, got);
}

void
expect_positive(char *name, int got)
{
    if(got > 0)
        printf("[OK] %s -> %d\n", name, got);
    else
        printf("[FAIL] %s -> got %d, expected > 0\n", name, got);
}

void
fill_range(char *p, int n, char c)
{
    int i;
    for(i = 0; i < n; i++)
        p[i] = c;
}

void
print_status(int slot)
{
    struct snap_info info;
    int r;

    r = snap_status(slot, &info);

    if(r == 0){
        printf("slot %d status: size=%d num_pages=%d\n",
               slot, info.size, info.num_pages);
    } else {
        printf("slot %d status failed: %d\n", slot, r);
    }
}

void
test_empty_and_invalid(void)
{
    int r;
    struct snap_info info;

    printf("\n=== test_empty_and_invalid ===\n");

    r = snap_empty();
    expect("snap_empty() initially returns 0", r, 0);

    r = snap_take(-1);
    expect("snap_take(-1) invalid slot", r, -2);

    r = snap_take(4);
    expect("snap_take(4) invalid slot", r, -2);

    r = snap_restore(-1);
    expect("snap_restore(-1) invalid slot", r, -2);

    r = snap_restore(4);
    expect("snap_restore(4) invalid slot", r, -2);

    r = snap_release(-1);
    expect("snap_release(-1) invalid slot", r, -2);

    r = snap_release(4);
    expect("snap_release(4) invalid slot", r, -2);

    r = snap_diff(-1, 0);
    expect("snap_diff(-1, 0) invalid slot", r, -2);

    r = snap_diff(0, 4);
    expect("snap_diff(0, 4) invalid slot", r, -2);

    /*
     * Po specifikaciji snap_status ima samo:
     * 0 uspeh, -1 prazan slot, -2 ostale greske.
     * Zato je i nevalidan slot -2.
     */
    r = snap_status(4, &info);
    expect("snap_status(4) invalid slot", r, -2);

    r = snap_restore(0);
    expect("snap_restore(0) empty slot", r, -1);

    r = snap_release(0);
    expect("snap_release(0) empty slot", r, -1);

    r = snap_status(0, &info);
    expect("snap_status(0) empty slot", r, -1);

    r = snap_diff(0, 1);
    expect("snap_diff(0, 1) both empty", r, -1);
}

void
test_take_status_diff_release(void)
{
    int r;
    char *buf;
    struct snap_info info;

    printf("\n=== test_take_status_diff_release ===\n");

    buf = malloc(3 * PAGE_SIZE);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    fill_range(buf, 3 * PAGE_SIZE, 'A');

    r = snap_take(0);
    expect("snap_take(0)", r, 0);

    r = snap_empty();
    expect("snap_empty() after slot 0 taken", r, 1);

    r = snap_take(0);
    expect("snap_take(0) occupied", r, -3);

    r = snap_status(0, &info);
    expect("snap_status(0)", r, 0);

    if(r == 0){
        printf("slot 0: size=%d num_pages=%d\n", info.size, info.num_pages);

        if(info.size > 0)
            ok("snap_status size > 0");
        else
            printf("[FAIL] snap_status size should be > 0\n");

        if(info.num_pages > 0)
            ok("snap_status num_pages > 0");
        else
            printf("[FAIL] snap_status num_pages should be > 0\n");
    }

    /*
     * Snapshot sa samim sobom mora imati diff 0.
     */
    r = snap_diff(0, 0);
    expect("snap_diff(0, 0) same snapshot", r, 0);

    /*
     * Promenimo nekoliko stranica i napravimo snapshot 1.
     */
    buf[0] = 'X';
    buf[PAGE_SIZE] = 'Y';
    buf[2 * PAGE_SIZE] = 'Z';

    r = snap_take(1);
    expect("snap_take(1)", r, 0);

    r = snap_diff(0, 1);
    expect_positive("snap_diff(0, 1) after memory changes", r);

    r = snap_diff(1, 0);
    expect_positive("snap_diff(1, 0) symmetric direction", r);

    /*
     * Oslobadjanje.
     */
    r = snap_release(0);
    expect("snap_release(0)", r, 0);

    r = snap_status(0, &info);
    expect("snap_status(0) after release", r, -1);

    r = snap_release(0);
    expect("snap_release(0) again", r, -1);

    /*
     * Slot 1 jos postoji.
     */
    r = snap_status(1, &info);
    expect("snap_status(1) still exists", r, 0);

    r = snap_release(1);
    expect("snap_release(1)", r, 0);

    r = snap_empty();
    expect("snap_empty() after releasing all", r, 0);
}

void
test_all_slots_full(void)
{
    int r;

    printf("\n=== test_all_slots_full ===\n");

    r = snap_empty();
    expect("snap_empty before filling all slots", r, 0);

    r = snap_take(0);
    expect("snap_take(0)", r, 0);

    r = snap_empty();
    expect("snap_empty after slot 0", r, 1);

    r = snap_take(1);
    expect("snap_take(1)", r, 0);

    r = snap_empty();
    expect("snap_empty after slot 1", r, 2);

    r = snap_take(2);
    expect("snap_take(2)", r, 0);

    r = snap_empty();
    expect("snap_empty after slot 2", r, 3);

    r = snap_take(3);
    expect("snap_take(3)", r, 0);

    r = snap_empty();
    expect("snap_empty when all slots full", r, -1);

    r = snap_take(3);
    expect("snap_take(3) occupied", r, -3);

    r = snap_release(2);
    expect("snap_release(2)", r, 0);

    r = snap_empty();
    expect("snap_empty after releasing slot 2", r, 2);

    r = snap_release(0);
    expect("snap_release(0)", r, 0);

    r = snap_release(1);
    expect("snap_release(1)", r, 0);

    r = snap_release(3);
    expect("snap_release(3)", r, 0);

    r = snap_empty();
    expect("snap_empty after cleanup", r, 0);
}

void
test_diff_different_sizes(void)
{
    int r;
    char *extra;

    printf("\n=== test_diff_different_sizes ===\n");

    /*
     * Snapshot 0 pre rasta memorije.
     */
    r = snap_take(0);
    expect("snap_take(0) before sbrk", r, 0);

    /*
     * Prosirujemo memoriju procesa za dve stranice.
     * Ovo bi trebalo da poveca curproc->sz.
     */
    extra = sbrk(2 * PAGE_SIZE);

    if(extra == (char*)-1){
        printf("[FAIL] sbrk failed\n");
        snap_release(0);
        return;
    }

    fill_range(extra, 2 * PAGE_SIZE, 'S');

    /*
     * Snapshot 1 ima veci adresni prostor.
     */
    r = snap_take(1);
    expect("snap_take(1) after sbrk", r, 0);

    r = snap_diff(0, 1);
    expect_positive("snap_diff(0, 1) different sizes", r);

    print_status(0);
    print_status(1);

    r = snap_release(0);
    expect("snap_release(0)", r, 0);

    r = snap_release(1);
    expect("snap_release(1)", r, 0);
}

/*
 * Ovaj test koristi fajl kao flag.
 * Restore vraca memoriju procesa i trapframe, ali ne vraca fajl sistem.
 */
void
test_restore_detailed(void)
{
    int r;
    int fd;
    char *buf;
    struct snap_info info;

    printf("\n=== test_restore_detailed ===\n");

    unlink("restoreflag");

    buf = malloc(3 * PAGE_SIZE);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    buf[0] = 'A';
    buf[PAGE_SIZE] = 'B';
    buf[2 * PAGE_SIZE] = 'C';

    r = snap_take(0);
    expect("snap_take(0) restore base", r, 0);

    fd = open("restoreflag", O_RDONLY);

    if(fd < 0){
        printf("restore first pass\n");

        r = snap_status(0, &info);
        expect("snap_status(0) before restore", r, 0);

        buf[0] = 'X';
        buf[PAGE_SIZE] = 'Y';
        buf[2 * PAGE_SIZE] = 'Z';

        r = snap_take(1);
        expect("snap_take(1) changed memory", r, 0);

        r = snap_diff(0, 1);
        expect_positive("snap_diff(0, 1) before restore", r);

        fd = open("restoreflag", O_CREATE | O_WRONLY);
        if(fd < 0){
            printf("[FAIL] could not create restoreflag\n");
            exit();
        }
        close(fd);

        printf("calling snap_restore(0)\n");

        r = snap_restore(0);


        printf("snap_restore returned %d\n", r);
        exit();
    }

    close(fd);

    printf("restore second pass\n");

    if(buf[0] == 'A' && buf[PAGE_SIZE] == 'B' && buf[2 * PAGE_SIZE] == 'C')
        ok("memory content restored to A/B/C");
    else
        printf("[FAIL] restored values: %d %d %d\n",
               buf[0], buf[PAGE_SIZE], buf[2 * PAGE_SIZE]);

        /*
         * Bitan deo: restore ne sme da unisti snapshot slot.
         * Zato slot 0 i dalje mora da postoji.
         */
        r = snap_status(0, &info);
    expect("snap_status(0) after restore, slot still exists", r, 0);

    /*
     * Slot 1 je postojao pre restore-a, ali restore vraca stanje
     * procesa na trenutak kada slot 1 jos nije postojao u memorijskom
     * toku. Medjutim, slotovi su u kernel strukturi procesa.
     *
     * Ako ti cuvas slotove u struct proc i restore ne vraca struct proc,
     * slot 1 ce najverovatnije i dalje postojati.
     */
    r = snap_status(1, &info);
    printf("snap_status(1) after restore returned %d\n", r);

    r = snap_release(0);
    expect("snap_release(0) after restore", r, 0);

    if(r == 0){
        /*
         * ok
         */
    }

    if(snap_status(1, &info) == 0){
        r = snap_release(1);
        expect("snap_release(1) if still present", r, 0);
    }

    unlink("restoreflag");
}

void
test_fork_inheritance_detailed(void)
{
    int r;
    int pid;
    char *buf;
    struct snap_info info;

    printf("\n=== test_fork_inheritance_detailed ===\n");

    buf = malloc(2 * PAGE_SIZE);
    if(buf == 0){
        printf("[FAIL] malloc failed\n");
        exit();
    }

    buf[0] = 'P';
    buf[PAGE_SIZE] = 'Q';

    r = snap_take(2);
    expect("parent snap_take(2)", r, 0);

    pid = fork();

    if(pid < 0){
        printf("[FAIL] fork failed\n");
        snap_release(2);
        return;
    }

    if(pid == 0){
        printf("child started\n");

        /*
         * Child mora da vidi snapshot u slotu 2.
         */
        r = snap_status(2, &info);
        expect("child snap_status(2) inherited", r, 0);

        /*
         * Child menja svoju memoriju i pravi jos jedan snapshot.
         */
        buf[0] = 'C';

        r = snap_take(3);
        expect("child snap_take(3)", r, 0);

        r = snap_diff(2, 3);
        expect_positive("child snap_diff(2, 3)", r);

        /*
         * Child oslobadja svoj slot 2.
         * Ovo ne sme uticati na parent-ov slot 2.
         */
        r = snap_release(2);
        expect("child snap_release(2)", r, 0);

        r = snap_status(2, &info);
        expect("child snap_status(2) after release", r, -1);

        r = snap_release(3);
        expect("child snap_release(3)", r, 0);

        printf("child exiting\n");
        exit();
    }

    wait();

    /*
     * Parent posle child release-a i dalje mora imati svoj slot 2.
     */
    r = snap_status(2, &info);
    expect("parent snap_status(2) after child exit", r, 0);

    /*
     * Parent ne sme imati child-ov slot 3.
     */
    r = snap_status(3, &info);
    expect("parent snap_status(3), child snapshot must not leak", r, -1);

    r = snap_release(2);
    expect("parent snap_release(2)", r, 0);
}

int
main(void)
{
    printf("\n\n=== SNAP DEEP TEST START ===\n");

    test_empty_and_invalid();

    test_take_status_diff_release();

    test_all_slots_full();

    test_diff_different_sizes();

    test_restore_detailed();

    test_fork_inheritance_detailed();

    printf("\n=== SNAP DEEP TEST END ===\n");

    exit();
}
