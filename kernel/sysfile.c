//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"

int
mappages2(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
	char *a, *last;
	pte_t *pte;

	a = (char*)PGROUNDDOWN((uint)va);
	last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
	for(;;){
		if((pte = walkpgdir(pgdir, a, 1)) == 0) return -1;
		*pte = pa | perm | PTE_P;
		if(a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

int
dell(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
	char *a, *last;
	pte_t *pte;

	a = (char*)PGROUNDDOWN((uint)va);
	last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
	for(;;){
		if((pte = walkpgdir(pgdir, a, 1)) == 0) return -1;
		*pte = pa | perm;
		if(a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

int
sys_snap_swap(){
	int slot1, slot2, va;
	if(argint(0, &slot1) < 0) return -1;
	if(argint(1, &slot2) < 0) return -1;
	if(argint(2, &va) < 0) return -1;
	if (slot1 < 0 || slot1 >= NSNAP) return -1;
	if (slot2 < 0 || slot2 >= NSNAP) return -1;

	struct proc *curproc = myproc();

	if (curproc->copy_of_pgdir[slot1] == 0) return -1;
	if (curproc->copy_of_pgdir[slot2] == 0) return -1;

	pde_t *pgdir1 = curproc->copy_of_pgdir[slot1];
	pde_t *pgdir2 = curproc->copy_of_pgdir[slot2];
	uint sz1 = curproc->copy_of_sz[slot1];
	uint sz2 = curproc->copy_of_sz[slot2];

	pte_t *entry1 = walkpgdir(pgdir1, (void*)va, 0);
	pte_t *entry2 = walkpgdir(pgdir2, (void*)va, 0);


	// if(entry1 == 0 || entry2 == 0) return -1;
	if(!(*entry1 & PTE_P) || !(*entry2 & PTE_P)) return -1;
	// e9printf("fdsfdsfdasfdasdfs\n %d \n", va);
	uint pa1 = PTE_ADDR(*entry1);
	uint pa2 = PTE_ADDR(*entry2);
	uint flags1 = PTE_FLAGS(*entry1);
	uint flags2 = PTE_FLAGS(*entry2);

	char *page1 = (char*)P2V(pa1);
	char *page2 = (char*)P2V(pa2);


	dell(pgdir1, (void*) va, sizeof(page1), V2P(page1), (flags1 ^ PTE_P));
	dell(pgdir2, (void*) va, sizeof(page2), V2P(page2), (flags2 ^ PTE_P));

	if(mappages(pgdir1, (void*)va, sizeof(page2), V2P(page2), flags2) < 0) {
		kfree(page1);
		kfree(page2);
		return -1;
	}
	if(mappages(pgdir2, (void*)va, sizeof(page1), V2P(page1), flags1) < 0) {
		kfree(page1);
		kfree(page2);
		return -1;
	}
	kfree(page1);
	kfree(page2);
	switchuvm(curproc);
	return 0;
}

int
sys_snap_num(){
	int ukupno = 0;
	struct proc *curproc = myproc();
	pte_t *pte;
	uint pa, i, flags;
	pde_t *pgdir = curproc->pgdir;
	for (int i=0;i<NSNAP;i++){
		if (curproc->copy_of_pgdir[i] == 0) continue;

		for (int j=0;j<curproc->copy_of_sz[i];j+=PGSIZE){
			if((pte = walkpgdir(pgdir, (void *) j, 0)) == 0) continue;
			if(!(*pte & PTE_P)) continue;
			ukupno++;
		}
	}
	return ukupno;
}

int
sys_snap_take()
{
	int slot;

	if(argint(0, &slot) < 0) return -4;

	if(slot < 0 || slot >= NSNAP) return -2;

	struct proc *curproc = myproc();

	if(curproc->copy_of_pgdir[slot] != 0) return -3;

	pde_t *pgdir = curproc->pgdir;
	uint sz = curproc->sz;

	pde_t *d;
	pte_t *pte;
	uint pa, i, flags;
	char *mem;
	struct trapframe *tfcopy = 0;
	int err = -4;

	if((d = setupkvm()) == 0)
		return -1;

	if((tfcopy = (struct trapframe*)kalloc()) == 0){
		freevm(d);
		return -1;
	}

	/*
	 * Cuvamo CPU stanje iz trenutka snapshot-a.
	 * Kasnije ce snap_restore vratiti eip/esp/registre na ovo stanje.
	 */
	*tfcopy = *curproc->tf;

	/*
	 * Kada se restore vrati na mesto posle snap_take,
	 * hocemo da izgleda kao da je snap_take vratio 0.
	 */
	tfcopy->eax = 0;

	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
			goto bad;

		if(!(*pte & PTE_P))
			goto bad;

		pa = PTE_ADDR(*pte);
		flags = PTE_FLAGS(*pte);

		if((mem = kalloc()) == 0){
			err = -1;
			goto bad;
		}

		memmove(mem, (char*)P2V(pa), PGSIZE);

		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
			kfree(mem);
			err = -1;
			goto bad;
		}
	}

	curproc->copy_of_pgdir[slot] = d;
	curproc->copy_of_sz[slot] = sz;
	curproc->copy_of_tf[slot] = tfcopy;

	return 0;

	bad:
	if(tfcopy != 0)
		kfree((char*)tfcopy);

	freevm(d);
	return err;
}
int
sys_snap_restore(void)
{
	int slot;

	if(argint(0, &slot) < 0) return -3;

	if(slot < 0 || slot >= NSNAP) return -2;

	struct proc *curproc = myproc();

	if(curproc->copy_of_pgdir[slot] == 0) return -1;
	if(curproc->copy_of_tf[slot] == 0) return -1;

	pde_t *newpgdir;
	pde_t *oldpgdir;

	newpgdir = copyuvm(curproc->copy_of_pgdir[slot],
					   curproc->copy_of_sz[slot]);

	if(newpgdir == 0)
		return -3;

	oldpgdir = curproc->pgdir;

	curproc->pgdir = newpgdir;
	curproc->sz = curproc->copy_of_sz[slot];

	/*
	 * Ovo je kljucno:
	 * vracamo user registre, eip i esp iz trenutka snap_take.
	 */
	*curproc->tf = *curproc->copy_of_tf[slot];

	/*
	 * Kada se user program vrati iz syscall-a,
	 * snap_take treba da izgleda kao da je vratio 0.
	 */
	curproc->tf->eax = 0;

	switchuvm(curproc);

	freevm(oldpgdir);

	return 0;
}

int min(int a, int b){
	return a > b ? b : a;
}

int max(int a, int b){
	return a > b ? a : b;
}

int
sys_snap_diff(void)
{
	int slot1, slot2;

	if(argint(0, &slot1) < 0) return -3;
	if(argint(1, &slot2) < 0) return -3;

	if(slot1 < 0 || slot1 >= NSNAP) return -2;
	if(slot2 < 0 || slot2 >= NSNAP) return -2;
	struct proc *curproc = myproc();

	if(curproc->copy_of_pgdir[slot1] == 0) return -1;
	if(curproc->copy_of_pgdir[slot2] == 0) return -1;


	int diff = 0;
	for(uint i = 0; i < max(curproc->copy_of_sz[slot1], curproc->copy_of_sz[slot2]); i += PGSIZE){
		if (i < min(curproc->copy_of_sz[slot1], curproc->copy_of_sz[slot2])){
			pte_t *entry1 = walkpgdir(curproc->copy_of_pgdir[slot1], (void*)i, 0);
			pte_t *entry2 = walkpgdir(curproc->copy_of_pgdir[slot2], (void*)i, 0);

			if(entry1 == 0 && entry2 == 0) continue;
			if(entry1 == 0 || entry2 == 0) {
				diff++;
				continue;
			}
			if(!(*entry1 & PTE_P) && !(*entry2 & PTE_P)){
				continue;
			}
			if(!(*entry1 & PTE_P) || !(*entry2 & PTE_P)){
				diff++;
				continue;
			}

			uint pa1 = PTE_ADDR(*entry1);
			uint pa2 = PTE_ADDR(*entry2);
			char *page1 = (char*)P2V(pa1);
			char *page2 = (char*)P2V(pa2);

			for (int j=0;j<PGSIZE;j++){
				if (page1[j] != page2[j]){
					diff++;
					break;
				}
			}
		}
		else diff++;
	}

	return diff;
}

int
sys_snap_release(void)
{
	int slot;

	if(argint(0, &slot) < 0) return -3;

	if(slot < 0 || slot >= NSNAP) return -2;

	struct proc *curproc = myproc();

	if(curproc->copy_of_pgdir[slot] == 0) return -1;

	freevm(curproc->copy_of_pgdir[slot]);
	curproc->copy_of_pgdir[slot] = 0;
	curproc->copy_of_sz[slot] = 0;

	if(curproc->copy_of_tf[slot] != 0){
		kfree((char*)curproc->copy_of_tf[slot]);
		curproc->copy_of_tf[slot] = 0;
	}

	return 0;
}

int
sys_snap_status(){
	int slot;
	struct snap_info *info;

	if(argint(0, &slot) < 0) return -2;
	if(argptr(1, (char**)&info, sizeof(struct snap_info)) < 0) return -2;
	if(slot < 0 || slot >= NSNAP) return -2;
	struct proc *curproc = myproc();
	if (curproc->copy_of_pgdir[slot] == 0) return -1;

	info->size = curproc->copy_of_sz[slot];
	info->num_pages = (curproc->copy_of_sz[slot] + PGSIZE - 1) / PGSIZE; //ceil div
	return 0;
}

int
sys_snap_empty(void){
	struct proc *curproc = myproc();
	for (int i=0;i<NSNAP;i++){
		if (curproc->copy_of_pgdir[i] == 0) return i;
	}
	return -1;
}



// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
	int fd;
	struct file *f;

	if(argint(n, &fd) < 0)
		return -1;
	if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
		return -1;
	if(pfd)
		*pfd = fd;
	if(pf)
		*pf = f;
	return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
	int fd;
	struct proc *curproc = myproc();

	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd] == 0){
			curproc->ofile[fd] = f;
			return fd;
		}
	}
	return -1;
}

int
sys_dup(void)
{
	struct file *f;
	int fd;

	if(argfd(0, 0, &f) < 0)
		return -1;
	if((fd=fdalloc(f)) < 0)
		return -1;
	filedup(f);
	return fd;
}

int
sys_read(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return fileread(f, p, n);
}

int
sys_write(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -1;
	return filewrite(f, p, n);
}

int
sys_close(void)
{
	int fd;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -1;
	myproc()->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

int
sys_fstat(void)
{
	struct file *f;
	struct stat *st;

	if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
		return -1;
	return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
		return -1;

	begin_op();
	if((ip = namei(old)) == 0){
		end_op();
		return -1;
	}

	ilock(ip);
	if(ip->type == T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
	}

	ip->nlink++;
	iupdate(ip);
	iunlock(ip);

	if((dp = nameiparent(new, name)) == 0)
		goto bad;
	ilock(dp);
	if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
		iunlockput(dp);
		goto bad;
	}
	iunlockput(dp);
	iput(ip);

	end_op();

	return 0;

bad:
	ilock(ip);
	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);
	end_op();
	return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
	int off;
	struct dirent de;

	for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if(de.inum != 0)
			return 0;
	}
	return 1;
}

int
sys_unlink(void)
{
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	uint off;

	if(argstr(0, &path) < 0)
		return -1;

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		return -1;
	}

	ilock(dp);

	// Cannot unlink "." or "..".
	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto bad;

	if((ip = dirlookup(dp, name, &off)) == 0)
		goto bad;
	ilock(ip);

	if(ip->nlink < 1)
		panic("unlink: nlink < 1");
	if(ip->type == T_DIR && !isdirempty(ip)){
		iunlockput(ip);
		goto bad;
	}

	memset(&de, 0, sizeof(de));
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("unlink: writei");
	if(ip->type == T_DIR){
		dp->nlink--;
		iupdate(dp);
	}
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	end_op();

	return 0;

bad:
	iunlockput(dp);
	end_op();
	return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
	struct inode *ip, *dp;
	char name[DIRSIZ];

	if((dp = nameiparent(path, name)) == 0)
		return 0;
	ilock(dp);

	if((ip = dirlookup(dp, name, 0)) != 0){
		iunlockput(dp);
		ilock(ip);
		if((type == T_FILE && ip->type == T_FILE) || ip->type == T_DEV)
			return ip;
		iunlockput(ip);
		return 0;
	}

	if((ip = ialloc(dp->dev, type)) == 0)
		panic("create: ialloc");

	ilock(ip);
	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;
	iupdate(ip);

	if(type == T_DIR){  // Create . and .. entries.
		dp->nlink++;  // for ".."
		iupdate(dp);
		// No ip->nlink++ for ".": avoid cyclic ref count.
		if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
			panic("create dots");
	}

	if(dirlink(dp, name, ip->inum) < 0)
		panic("create: dirlink");

	iunlockput(dp);

	return ip;
}

int
sys_open(void)
{
	char *path;
	int fd, omode;
	struct file *f;
	struct inode *ip;

	if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
		return -1;

	begin_op();

	if(omode & O_CREATE){
		ip = create(path, T_FILE, 0, 0);
		if(ip == 0){
			end_op();
			return -1;
		}
	} else {
		if((ip = namei(path)) == 0){
			end_op();
			return -1;
		}
		ilock(ip);
		if(ip->type == T_DIR && omode != O_RDONLY){
			iunlockput(ip);
			end_op();
			return -1;
		}
	}

	if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
		if(f)
			fileclose(f);
		iunlockput(ip);
		end_op();
		return -1;
	}
	iunlock(ip);
	end_op();

	f->type = FD_INODE;
	f->ip = ip;
	f->off = 0;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return fd;
}

int
sys_mkdir(void)
{
	char *path;
	struct inode *ip;

	begin_op();
	if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_mknod(void)
{
	struct inode *ip;
	char *path;
	int major, minor;

	begin_op();
	if((argstr(0, &path)) < 0 ||
			argint(1, &major) < 0 ||
			argint(2, &minor) < 0 ||
			(ip = create(path, T_DEV, major, minor)) == 0){
		end_op();
		return -1;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int
sys_chdir(void)
{
	char *path;
	struct inode *ip;
	struct proc *curproc = myproc();

	begin_op();
	if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
		end_op();
		return -1;
	}
	ilock(ip);
	if(ip->type != T_DIR){
		iunlockput(ip);
		end_op();
		return -1;
	}
	iunlock(ip);
	iput(curproc->cwd);
	end_op();
	curproc->cwd = ip;
	return 0;
}

int
sys_exec(void)
{
	char *path, *argv[MAXARG];
	int i;
	uint uargv, uarg;

	if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
		return -1;
	}
	memset(argv, 0, sizeof(argv));
	for(i=0;; i++){
		if(i >= NELEM(argv))
			return -1;
		if(fetchint(uargv+4*i, (int*)&uarg) < 0)
			return -1;
		if(uarg == 0){
			argv[i] = 0;
			break;
		}
		if(fetchstr(uarg, &argv[i]) < 0)
			return -1;
	}
	return exec(path, argv);
}

int
sys_pipe(void)
{
	int *fd;
	struct file *rf, *wf;
	int fd0, fd1;

	if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
		return -1;
	if(pipealloc(&rf, &wf) < 0)
		return -1;
	fd0 = -1;
	if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
		if(fd0 >= 0)
			myproc()->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -1;
	}
	fd[0] = fd0;
	fd[1] = fd1;
	return 0;
}
