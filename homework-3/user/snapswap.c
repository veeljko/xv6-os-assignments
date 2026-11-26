#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "user.h"
#include "../kernel/fcntl.h"

int main(){
    char *buf = "PRVI_SNIMAK";
    snap_take(0);
    buf = "DRUGI_SNIMAK";
    snap_take(1);
    int *v = (int *)buf;
    // printf("%d\n", *v);
    int v2 = *v;
    // printf("%d\n", v2);
    snap_swap(0, 1, v2);
    printf("%s\n", buf);
    exit();
}
