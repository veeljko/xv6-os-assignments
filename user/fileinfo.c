#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"
#include "kernel/fileblks.h"

void handle_error(char *err_msg){
    printf("Doslo je do greske: %s", err_msg);
    exit();
}

int
main(int argc, char *argv[])
{
    if(argc == 1 || (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))){
        printf("Upotreba: fileinfo <putanja_do_fajla> [OPCIJA]\n");
        printf("Prikazuje detaljnu mapu fizickih blokova datoteke na disku.\n\n");
        printf("Opcija:\n");
        printf("  -h, --help    Prikazuje ovaj help meni\n");
        exit();
    }

    struct fileblks fileInfo;

    int fd = open(argv[1], 0);
    if(fd < 0){
        handle_error("Greska pri otvaranju fajla");
    }

    if(get_file_blocks(fd, &fileInfo) == -1){
        close(fd);
        handle_error("Greska pri pozivu funckije get_file_blocks");
    }

    int size = fileInfo.num_blocks * 512 - fileInfo.last_block_free;

    printf("\n--- INSPEKCIJA DATOTEKE: %s ---\n", argv[1]);
    printf("Ukupna velicina: %d bajtova\n", size);
    printf("Zauzetih blokova: %d\n", fileInfo.num_blocks);
    printf("Slobodno u zadnjem bloku: %d bajtova\n", fileInfo.last_block_free);
    printf("-------------------------------------------------------\n");
    printf("Direktni blokovi: (0-11)\n");

    for(int i = 0; i < fileInfo.num_blocks && i < 12; i++){
        printf("[%d] ", fileInfo.blocks[i]);
    }
    printf("\n");
    if (fileInfo.blocks[12] != 0){
        printf("\nIndirektni blokovi: (12-139)\n");

        for(int i = 12; i < fileInfo.num_blocks; i++){
            printf("%d ", fileInfo.blocks[i]);
        }
        printf("\n");
    }
    printf("-------------------------------------------------------\n");

    close(fd);
    exit();
}
