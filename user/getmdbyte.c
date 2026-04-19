#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

void handle_error(char *err_msg){
    printf("Doslo je do greske: %s", err_msg);
    exit();
}

int
main(int argc, char *argv[])
{
    if((argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) || argc == 1){
        printf("Upotreba: get_inode_red [PATH]\n");
        printf("Prikazuje srednji bit zadatog fajla.\n\n");
        printf("Opcije:\n");
        printf("  -h, --help    Prikazuje ovaj help meni\n");
        exit();
    }

    char *path = argv[1];
    printf("%s\n", path);
    int fd = open(path, 0);
    int vrednostBajta = getmdbyte(fd);
    printf("Bajt koji se nalazi na sredini fajla: %d\n", vrednostBajta);
    exit();
}
