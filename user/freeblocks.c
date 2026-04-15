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
    if(argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))){
        printf("Upotreba: freeblocks [OPCIJA]\n");
        printf("Pokazuje broj slobodnih blokova na xv6 fajl sistemu.\n\n");
        printf("Opcije:\n");
        printf("  -h, --help    Prikazuje ovaj help meni\n");
        printf("\nPrimer: freeblocks\n");
        exit();
    }
    else if (argc == 1){
        printf("Skeniram bitmap-u diska... \n\n");

        int blocks = get_free_blocks();
        if (blocks == -1){
            handle_error("Doslo je do greske pri racunanju slobodnih blokova.");
        }
        else{
            int kb = (blocks * 512) / 1024;
            printf("------------------------------\n");
            printf("STATUS DISKA\n");
            printf("------------------------------\n");
            printf("Slobodnih blokova: %d\n", blocks);
            printf("Slobodan prostor %d KB\n", kb);
            printf("------------------------------\n");
        }
    }

    exit();
}
