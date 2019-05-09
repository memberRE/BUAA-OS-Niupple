// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>


/* ----------------- help functions ---------------- */

/* Overview:
 * 	Copy `len` bytes from `src` to `dst`.
 *
 * Pre-Condition:
 * 	`src` and `dst` can't be NULL. Also, the `src` area 
 * 	 shouldn't overlap the `dest`, otherwise the behavior of this 
 * 	 function is undefined.
 */
void user_bcopy(const void *src, void *dst, size_t len)
{
	void *max;

	//	writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
	max = dst + len;

	// copy machine words while possible
	if (((int)src % 4 == 0) && ((int)dst % 4 == 0)) {
		while (dst + 3 < max) {
			*(int *)dst = *(int *)src;
			dst += 4;
			src += 4;
		}
	}

	// finish remaining 0-3 bytes
	while (dst < max) {
		*(char *)dst = *(char *)src;
		dst += 1;
		src += 1;
	}

	//for(;;);
}

/* Overview:
 * 	Sets the first n bytes of the block of memory 
 * pointed by `v` to zero.
 * 
 * Pre-Condition:
 * 	`v` must be valid.
 *
 * Post-Condition:
 * 	the content of the space(from `v` to `v`+ n) 
 * will be set to zero.
 */
void user_bzero(void *v, u_int n)
{
	char *p;
	int m;

	p = v;
	m = n;

	while (--m >= 0) {
		*p++ = 0;
	}
}
/*--------------------------------------------------------------*/

/* Overview:
 * 	Custom page fault handler - if faulting page is copy-on-write,
 * map in our own private writable copy.
 * 
 * Pre-Condition:
 * 	`va` is the address which leads to a TLBS exception.
 *
 * Post-Condition:
 *  Launch a user_panic if `va` is not a copy-on-write page.
 * Otherwise, this handler should map a private writable copy of 
 * the faulting page at correct address.
 */
static void
pgfault(u_int va, u_int *epc)
{
	//extern struct Env * env;
	writef("in pagefault envid = %d\n", syscall_getenvid());
	//writef("%d: in pgfault %x\n", syscall_getenvid(), va);
	u_int *tmp;
	//	writef("fork.c:pgfault():\t va:%x\n",va);
	u_int instr = *epc;
	//struct Env *env;
	//++env->env_nop;
	/*
	writef("Env: 0x%x, Instr: 0x%x, opcode: %b, reg_rs: %d, reg_rt: %d, OUT_count: %d, COW_count: %d\n",
			env->env_id,
			instr,
			instr >> 26,
			(instr >> 20) & (0x1f),
			(instr >> 15) & (0x1f),
			env->env_runs,
			env->env_nop);
			*/
    
    //map the new page at a temporary place
	tmp = (*vpt)+(va>>12);
	//writef("before: va %x -> pa %x\n", va, *tmp);
	if(!(*tmp & PTE_COW)) {
		user_panic("no pte_cow\n");
	}

	tmp = USTACKTOP;
	if(syscall_mem_alloc(0, tmp, PTE_V | PTE_R) < 0) {
		return;
	}

	//copy the content
	user_bcopy(ROUNDDOWN(va, BY2PG), tmp, BY2PG);
	
    //map the page on the appropriate place
	if(syscall_mem_map(0, tmp, 0, ROUNDDOWN(va, BY2PG), PTE_V | PTE_R) < 0) {
		return;
	}

	//writef("after: va %x -> pa %x\n", va, *((*vpt)+(va>>12)));
	
    //unmap the temporary place
	syscall_mem_unmap(0, tmp);
	
}

/* Overview:
 * 	Map our virtual page `pn` (address pn*BY2PG) into the target `envid`
 * at the same virtual address. 
 *
 * Post-Condition:
 *  if the page is writable or copy-on-write, the new mapping must be 
 * created copy on write and then our mapping must be marked 
 * copy on write as well. In another word, both of the new mapping and
 * our mapping should be copy-on-write if the page is writable or 
 * copy-on-write.
 * 
 * Hint:
 * 	PTE_LIBRARY indicates that the page is shared between processes.
 * A page with PTE_LIBRARY may have PTE_R at the same time. You
 * should process it correctly.
 */
static void
duppage(u_int envid, u_int pn)
{
	//writef("duppage from %d to %d at %x\n", syscall_getenvid(), envid, pn);
	u_int addr;
	u_int perm;

	addr = *(*vpt+pn);
	perm = addr&(BY2PG-1);
	addr = pn*BY2PG;
	if(!(perm & PTE_V)) {
		return;
	} else {
		if((perm & PTE_R) && (perm & PTE_LIBRARY)) {
			//as it is
		} else if((perm & PTE_R)) {
			perm |= PTE_COW;
		}
	}
	if(syscall_mem_map(0, addr, envid, addr, perm) < 0) {
		user_panic("unsuccessful duplicate\n");
		return;
	}
	if(syscall_mem_map(0, addr, 0, addr, perm) < 0) {
		user_panic("unsuccessful duplicate\n");
		return;
	}
	if(pn == (0x7f3fdf70 >> 12)) {
		//writef("%d: perm = %x\n", syscall_getenvid(), perm);
		//syscall_panic("panic in duppage\n");
	}

	//	user_panic("duppage not implemented");
}

/* Overview:
 * 	User-level fork. Create a child and then copy our address space
 * and page fault handler setup to the child.
 *
 * Hint: use vpd, vpt, and duppage.
 * Hint: remember to fix "env" in the child process!
 * Note: `set_pgfault_handler`(user/pgfault.c) is different from 
 *       `syscall_set_pgfault_handler`. 
 */
extern void __asm_pgfault_handler(void);
int extid;
int
fork(void)
{
	//writef("now start to fork\n");
	// Your code here.
	u_int newenvid;
	extern struct Env *envs;
	extern struct Env *env;
	u_int i;


	//The parent installs pgfault using set_pgfault_handler
	set_pgfault_handler(pgfault);
	//writef("set\n");

	//alloc a new alloc

	newenvid = syscall_env_alloc();
	//writef("successfully return from syscall_env_alloc with id %d\n", newenvid);
	//writef("my USTACKTOP is %x\n", USTACKTOP);
	//syscall_panic("panic before dup\n");
	if(newenvid != 0) {
		for(i = 0; i < USTACKTOP; i += BY2PG) {
			if((*vpd)[i >> 22] & PTE_V) {
				//writef("loop %x\n", i);
				duppage(newenvid, i >> 12);
			}
			if(i >= 0x7f3fd000) {
				//syscall_panic("father panic\n");
			}
		}
		if(syscall_mem_alloc(newenvid, UXSTACKTOP - BY2PG, PTE_V | PTE_R) < 0) {
			user_panic("cannot alloc mem for child as xstack\n");
		}
		//writef("loop ended\n");
		if(syscall_set_pgfault_handler(newenvid, __asm_pgfault_handler, UXSTACKTOP) < 0) {
			user_panic("set handler failed\n");
		}
		if(syscall_set_env_status(newenvid, ENV_RUNNABLE) < 0) {
			user_panic("set status failed\n");
		}
	} else {
		//writef("I am son\n");
		env = envs+ENVX(syscall_getenvid());
		//syscall_panic("son needs to panic\n");
	}
	//writef("");
	//writef("%d returning %d\n", syscall_getenvid(), newenvid);
	//writef("let's see if you can print this well?\n");
	//writef("i = %d\n", i);

	return newenvid;
}

// Challenge!
int
sfork(void)
{
	user_panic("sfork not implemented");
	return -E_INVAL;
}
