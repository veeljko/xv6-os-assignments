#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fileblks.h"

#define ROWS 25
#define COLS 80

void handle_error(char *err_msg);
void leave_edit_mode();

void itoa_simple(int x, char *out) {
    char tmp[16];
    int i = 0, j = 0, neg = 0;

    if (x == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    if (x < 0) {
        neg = 1;
        x = -x;
    }

    while (x > 0) {
        tmp[i++] = '0' + (x % 10);
        x /= 10;
    }

    if (neg) out[j++] = '-';

    while (i > 0) out[j++] = tmp[--i];

    out[j] = '\0';
}

struct buffer{
    char text[22 * 80];
    char commands[70];
    int n;
    int m;
    int commandMode;
    int resMsg;
    int isDirty;
};

struct buffer buff;
char screen[25 * 80];

void init_screen(){
    for (int i=0;i<25*80;i++) screen[i] = ' ';
}

void init_header(char *path){
    for (int i=0;i<80;i++) screen[i] = '-';
    memmove(screen + 3, " Header: ", 8);
    memmove(screen + 11, path, strlen(path));
    screen[11 + strlen(path)] = ' ';
    char *mod = " [MODIFIED] ";
    if (buff.isDirty) memmove(screen + 11 + strlen(path), mod, strlen(mod));
}

void init_footer_command(){
    for(int i = 24 * 80 + 8; i < 25 * 80; i++)
        screen[i] = ' ';

    memmove(screen + 24 * 80 + 8, buff.commands, buff.m);
}

void init_footer(){
    for(int i = 23 * 80; i < 24 * 80; i++)
        screen[i] = ' ';
    for(int i = 24 * 80; i < 25 * 80; i++)
        screen[i] = ' ';

    char num[16];
    int blocks = get_free_blocks();
    if (blocks == -1){
        handle_error("Doslo je do greske pri racunanju slobodnih blokova.");
    }
    itoa_simple(blocks, num);

    memmove(screen + 23 * 80, "Slobodno: ", 10);
    memmove(screen + 23 * 80 + 10, num, strlen(num));
    memmove(screen + 23 * 80 + 10 + strlen(num),
            " blk : [$] Komande : [Enter] Novi red",
            strlen(" blk : [$] Komande : [Enter] Novi red"));

    memmove(screen + 24 * 80, "Command:", 8);
}

void init_edit_text(){
    for(int i = 80; i < 23 * 80; i++)
        screen[i] = ' ';

    int row = 1;
    int col = 0;

    for(int i = 0; i < buff.n && row < 23; i++){
        char ch = buff.text[i];

        if(ch == '\n'){
            row++;
            col = 0;
        } else {
            if(row < 23){
                screen[row * 80 + col] = ch;
                col++;
                if(col == 80){
                    row++;
                    col = 0;
                }
            }
        }
    }
}

void init_text(char *path){
    buff.n = read_path(path, buff.text);
    if (buff.n == -1){
        handle_error("Citanje nije bilo uspesno");
    }
    if (buff.n == -2){
        handle_error("Datoteka je T_DEV tipa.");
    }
    init_edit_text();
}

int calc_cursor(){
    int r = 0, c = 0;

    for(int i = 0; i < buff.n; i++){
        if(buff.text[i] == '\n'){
            if(r < 21){
                r++;
                c = 0;
            }
        } else {
            c++;
            if(c == 80){
                c = 0;
                if(r < 21) r++;
            }
        }
    }

    return (r + 1) * 80 + c;
}

int init_editor(char *path){
    init_screen();
    init_header(path);
    init_footer();
    init_text(path);

    if (set_cursor(calc_cursor()) == -1){
        handle_error("Greska pri pozivu funkcije set_cursor");
    }
    enter_text_editor();
    print_screen(screen);
}

void update(char *path){
    init_footer();
    init_footer_command();
    init_header(path);
    init_edit_text();
    print_screen(screen);

    if(buff.commandMode){
        if (set_cursor(24 * 80 + 8 + buff.m) == -1){
            handle_error("Greska pri pozivu funkcije set_cursor");
        }
    }
    else{
        if (set_cursor(calc_cursor()) == -1){
            handle_error("Greska pri pozivu funkcije set_cursor");
        }
    }
}

void leave_edit_mode(){
    init_screen();
    if (set_cursor(80) == -1){
        handle_error("Greska pri pozivu funkcije set_cursor");
    }
    print_screen(screen);
    enter_text_editor();
}

int init_stats(char *path){
    int fd = open(path, 0);
    if(fd < 0){
        handle_error("Greska pri otvaranju fajla.");
        return -1;
    }

    struct fileblks fileInfo;
    if(get_file_blocks(fd, &fileInfo) < 0){
        close(fd);
        handle_error("Greska pri pozivu funckije get_file_blocks");
        return -1;
    }

    close(fd);

    buff.m = 0;

    char *prefix = "Blokovi: ";
    memmove(buff.commands, prefix, strlen(prefix));
    buff.m = strlen(prefix);

    for(int i = 0; i < 5; i++){
        char numBlock[16];
        itoa_simple(fileInfo.blocks[i], numBlock);

        memmove(buff.commands + buff.m, numBlock, strlen(numBlock));
        buff.m += strlen(numBlock);

        if (i < 4) buff.commands[buff.m++] = ',';
        buff.commands[buff.m++] = ' ';
    }

    buff.commands[buff.m++] = '(';

    char numBlocksStr[16];
    itoa_simple(fileInfo.num_blocks, numBlocksStr);
    memmove(buff.commands + buff.m, numBlocksStr, strlen(numBlocksStr));
    buff.m += strlen(numBlocksStr);

    char *postfix = " ukupno)";
    memmove(buff.commands + buff.m, postfix, strlen(postfix));
    buff.m += strlen(postfix);
    buff.commands[buff.m] = 0;

    return 0;
}


void print_help(){
    printf("--- XV6 EDITOR HELP ---\n");
    printf("Upotreba: editor <filename>\n\n");

    printf("OPCIJE:\n");
    printf("  -h, --help   Prikazuje ovaj meni\n\n");

    printf("KOMANDE (aktiviraju se tasterom $):\n");
    printf("  save   Trajno cuva sadrzaj na disk\n");
    printf("  stats  Prikazuje fizicke adrese blokova datoteke\n");
    printf("  quit   Izlaz iz editora (upozorava ako nije sacuvano)\n\n");

    printf("DODATNO:\n");
    printf("  [Backspace]  Brise karakter i pomera kursor unazad\n");
    printf("  [Enter]      Novi red (automatsko pozicioniranje)\n");
}

void handle_error(char *err_msg){
    leave_edit_mode();
    printf("Doslo je do greske: %s", err_msg);
    exit();
}

int
main(int argc, char *argv[])
{

    if (argc < 2) print_help();
    else{
        char *path;

        if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")){
            print_help();
            exit();
        }

        path = argv[1];
        buff.commandMode = 0;
        buff.resMsg = 0;
        buff.m = 0;
        buff.isDirty = 0;

        init_editor(path);
        int quitCount = 0;
        char c;
        while(read(0, &c, 1) == 1){
            if (c == '$'){
                buff.commandMode = !buff.commandMode;
                buff.m = 0;
                buff.resMsg = 0;
                quitCount = 0;
                if (buff.commandMode){
                    if (set_cursor(24 * 80 + 8) == -1){
                        handle_error("Greska pri pozivu funkcije set_cursor");
                    }
                }
                else{
                    if (set_cursor(calc_cursor()) == -1){
                        handle_error("Greska pri pozivu funkcije set_cursor");
                    }
                }
            }
            else if (c == '\b' || c == 127){
                if (buff.resMsg){
                    buff.m = 0;
                    buff.resMsg = 0;
                    update(path);
                    continue;
                }
                if (!buff.commandMode){
                    if (buff.n > 0){
                        buff.n--;
                        buff.isDirty = 1;
                    }
                }
                else {
                    if (buff.m > 0) buff.m--;
                }
            }
            else{
                int pos = get_cursor();
                if (!buff.commandMode){
                    if (pos < 22 * 80 - 1){
                        buff.text[buff.n ++] = c;
                        buff.isDirty = 1;
                    }
                }
                else{
                    if (buff.resMsg){
                        buff.m = 0;
                        buff.resMsg = 0;
                        update(path);
                        continue;
                    }
                    if (c == '\n'){
                        buff.commands[buff.m] = 0;
                        if (!strcmp(buff.commands, "quit")){
                            if (buff.isDirty){
                                quitCount++;
                                if (quitCount < 2){
                                    buff.resMsg = 1;
                                    char *res = "Nije sacuvano! Quit ponovo za izlaz.";
                                    memmove(buff.commands, res, strlen(res));
                                    buff.m = strlen(res);
                                }
                                else{
                                    leave_edit_mode();
                                    exit();
                                }
                            }
                            else{
                                leave_edit_mode();
                                exit();
                            }
                        }
                        else if (!strcmp(buff.commands, "save")){
                            quitCount = 0;
                            int res = write_path(path, buff.text, buff.n);
                            if (res == -1){
                                handle_error("Upisivanje nije bilo uspešno.");
                            }
                            if (res == -2){
                                handle_error("Datoteka je T_DEV tipa.");
                            }
                            if (res == -3){
                                handle_error("Nema dovoljno slobodnih blokova na disku.");
                            }
                            if (res == -4){
                                handle_error("Greska pri prosledjivanju parametara funckije.");
                            }
                            if (res == -5){
                                handle_error("Greska pri pravljenju fajla.");
                            }
                            buff.resMsg = 1;
                            char *response = "Sacuvano!";
                            memmove(buff.commands, response, strlen(response));
                            buff.m = strlen(response);
                            buff.isDirty = 0;

                        }
                        else if (!strcmp(buff.commands, "stats")){
                            quitCount = 0;
                            buff.resMsg = 1;
                            if (init_stats(path) == -1){
                                char *res = "INFO: Fajl jos ne postoji na disku. Sacuvajte ga prvo.";
                                memmove(buff.commands, res, strlen(res));
                                buff.m = strlen(res);
                            }
                        }
                        else if (!strcmp(buff.commands, "clear")){
                            buff.commandMode = 0;
                            buff.resMsg = 0;
                            buff.m = 0;
                            buff.isDirty = 0;
                            buff.n = 0;
                            quitCount = 0;
                            for (int i=0;i<25*80;i++) screen[i] = ' ';
                            int res = write_path(path, buff.text, buff.n);
                            if (res == -1){
                                handle_error("Upisivanje nije bilo uspešno.");
                            }
                            if (res == -2){
                                handle_error("Datoteka je T_DEV tipa.");
                            }
                            if (res == -3){
                                handle_error("Nema dovoljno slobodnih blokova na disku.");
                            }
                            if (res == -4){
                                handle_error("Greska pri prosledjivanju parametara funckije.");
                            }
                            if (res == -5){
                                handle_error("Greska pri pravljenju fajla.");
                            }
                        }
                    }
                    else{
                        if (pos < 25 * 80 - 2) buff.commands[buff.m ++] = c;

                    }
                }
            }

            update(path);
        }
    }

    exit();
}
