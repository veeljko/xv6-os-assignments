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
        printf("Pokazuje broj referenci na putanji.\n\n");
        printf("Opcije:\n");
        printf("  -h, --help    Prikazuje ovaj help meni\n");
        exit();
    }
    char *path = argv[1];
    int ref_count = get_inode_ref(path);
    if (ref_count < 0){
        handle_error("Greska pri pozivu funkcije get_inode_ref");
    }
    printf("Broj referenci u memoriji: %d\n", ref_count);
    exit();
}
