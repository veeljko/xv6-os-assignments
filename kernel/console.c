// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "traps.h"
#include "spinlock.h"
#include "file.h"
#include "memlayout.h"
#include "proc.h"
#include "x86.h"
#include "printf.h"

#include <stdarg.h>

static void consputc(int);

static int panicked = 0;
static int markColor = 0x5000;
static int hideInput = 0;


static struct {
	struct spinlock lock;
	int locking;
} cons;

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int locking;
	va_list args;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	va_start(args, fmt);

	fnvprintf(consputc, fmt, args);

	va_end(args);

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.

	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if(c == '\n')
		pos += 80 - pos%80;
	else if(c == BACKSPACE){
		if(pos > 0) --pos;
	} else
		crt[pos++] = (c&0xff) | 0x0700;  // black on white

	if(pos < 0 || pos > 25*80)
		panic("pos under/overflow");

	if((pos/80) >= 24){  // Scroll up.
		memmove(crt, crt+80, sizeof(crt[0])*23*80);
		pos -= 80;
		memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
	}

	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x
#define A(x)  ((x)+'@')

static void
setcursor(int pos) {
	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
}

static int
getcursor(){
	int pos;
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);
	return pos;
}

//FIND MODE
#define SEARCH_MAX 70
struct {
	char buf[SEARCH_MAX];
	int len;
	int marked;
	int totalfound;
} searchinfo;

static int
cmp_non_case_sens(char a, char b){
	return (a == b ? 1 : a + 32 == b ? 1 : a - 32 == b ? 1 : 0);
}

static void
mark_range_color(int l, int r, int color){
	for(int i = l; i <= r; i++){
		int ch = crt[i] & 0x00ff;
		crt[i] = ch | color;
	}
}

static void
mark_range(int l, int r){
	mark_range_color(l, r, markColor);
}

static void
markalloccur(){
	int mx = 24 * 80;
	for(int i = 0; i < mx; i++){
		int rightPart = crt[i] & 0x00ff;
		crt[i] = rightPart | 0x7000; // pozadina
	}
	if(searchinfo.len == 0) return;
	int p1 = 0, found = 0;
	for(int i = 0; i < mx; i++){
		char ch = crt[i] & 0x00ff;
		if(cmp_non_case_sens(searchinfo.buf[p1], ch)){
			p1++;
			if(p1 == searchinfo.len){
				mark_range(i - searchinfo.len + 1, i);
				p1 = 0;
				found++;
			}
		}
		else{
			p1 = 0;
		}
	}
	searchinfo.totalfound = found;
}

static int
marknthoccur(int n){
	if(searchinfo.len == 0) return 0;
	markalloccur();
	int mx = 24 * 80;
	int p1 = 0, found = 0;
	for(int i = 0; i < mx; i++){
		char ch = crt[i] & 0x00ff;
		if(cmp_non_case_sens(searchinfo.buf[p1], ch)){
			p1++;
			if(p1 == searchinfo.len){
				p1 = 0;
				found++;
				if (found == n){
					mark_range_color(i - searchinfo.len + 1, i, 0x2000); //target
					searchinfo.marked = n;
					return 1;
				}
			}
		}
		else{
			p1 = 0;
		}
	}
	return 0;
}

static void
mark_prev(){
	int ans = marknthoccur(searchinfo.marked - 1);
	if (ans == 0) marknthoccur(searchinfo.totalfound);
}

static void
mark_next(){
	int ans = marknthoccur(searchinfo.marked + 1);
	if (ans == 0) marknthoccur(1);
}


static void
consolefindmode(int c){
	int pos = getcursor();

	if(c == '\n') return;
	else if(c == BACKSPACE){
		if(pos > 24 * 80 + 8){
			searchinfo.len--;
			pos--;
		}
	}
	else{
		if(searchinfo.len < SEARCH_MAX-1){
			searchinfo.buf[searchinfo.len++] = c;
			if (!hideInput) crt[pos++] = (c & 0xff) | 0x0700;
			else crt[pos++] = (' ' & 0xff) | 0x0700;
		}
	}
	markalloccur();
	marknthoccur(1);
	setcursor(pos);
	crt[pos] = ' ' | 0x0700;
}



static void
enterfindmode(){
	int mx = 25 * 80;
	for (int i=0;i<mx;i++){
		int rightPart = crt[i] & 0x00ff;
		crt[i] = rightPart | 0x7000;
	}

	int pos = 24 * 80;
	for (int i=pos;i<mx;i++){
		crt[i] = ' ' | 0x0200;
	}

	static char input[] = "Search: ";
	for (int i=0;i<8;i++){
		crt[pos++] = input[i] | 0x0200;
	}
	for(int i=0;i<searchinfo.len;i++){
		if (!hideInput) crt[pos++] = searchinfo.buf[i] | 0x0700;
		else crt[pos++] = ' ' | 0x0700;
	}
	markalloccur();
	marknthoccur(1);
	setcursor(pos);
	crt[pos] = ' ' | 0x0700;
}


static void
leavefindmode(){
	int mx = 25 * 80;
	for (int i=0;i<mx;i++){
		int rightPart = crt[i] & 0x00ff;
		crt[i] = rightPart | 0x0700;
	}
	int pos = 24 * 80;
	for (int i=pos;i<mx;i++){
		crt[i] = ' ' | 0x0700;
	}
}

static int
togglefindmode(){
	static int flag = 0;
	static int inputmodepos = 0;
	if (!flag){
		inputmodepos = getcursor();
		enterfindmode();
		flag = 1;
	}
	else{
		setcursor(inputmodepos);
		leavefindmode();
		flag = 0;
	}
	return flag;
}

static void
change_selected_color(){
	if (markColor == 0x5000) markColor = 0x4000;
	else markColor = 0x5000;
	markalloccur();
	marknthoccur(searchinfo.marked);
}

static void
to_upper_case(int l, int r){
	int diff = 'A' - 'a';
	for (int i=l;i<=r;i++){
		int c = crt[i] & 0x00ff;
		if (c >= 'a' && c <= 'z') c += diff;
		crt[i] = (c & 0x00ff) | 0x0700;
	}
}

static void
targetToUpperCaseScreen(){
	int mx = 24 * 80;
	for(int i = 0; i < mx; i++){
		int rightPart = crt[i] & 0x00ff;
		crt[i] = rightPart | 0x7000; // pozadina
	}
	if(searchinfo.len == 0) return;
	int p1 = 0, found = 0;
	for(int i = 0; i < mx; i++){
		char ch = crt[i] & 0x00ff;
		if(cmp_non_case_sens(searchinfo.buf[p1], ch)){
			p1++;
			if(p1 == searchinfo.len){
				to_upper_case(i - searchinfo.len + 1, i);
				p1 = 0;
				found++;
			}
		}
		else{
			p1 = 0;
		}
	}
	markalloccur();
	marknthoccur(searchinfo.marked);
	searchinfo.totalfound = found;
}

void
consoleintr(int (*getc)(void))
{
	static int toggle_find_mode = 0;
	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(toggle_find_mode) consolefindmode(BACKSPACE);
			else{
				if(input.e != input.w){
					input.e--;
					consputc(BACKSPACE);
				}
			}
			break;
		case A('F'):
			if (hideInput == 0) toggle_find_mode = togglefindmode();
			break;
		case A('N'):
			if (!toggle_find_mode) break;
			mark_next();
			break;
		case A('P'):
			if (!toggle_find_mode) break;
			mark_prev();
			break;
		case A('B'):
			// e9printf("test\n");
			change_selected_color();
			break;
		case A('U'):
			targetToUpperCaseScreen();
			break;
		case A('S'):
			if (hideInput == 0 && !toggle_find_mode){
				hideInput = 1;
				toggle_find_mode = togglefindmode();
			}
			else if (hideInput == 1 && toggle_find_mode){
				hideInput = 0;
				toggle_find_mode = togglefindmode();
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;
				input.buf[input.e++ % INPUT_BUF] = c;
				if (!toggle_find_mode){
					consputc(c);
					if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
						input.w = input.e;
						wakeup(&input.r);
					}
				}
				else consolefindmode(c);
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

