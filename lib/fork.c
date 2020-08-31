// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
 
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if((err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW))
	{
		;
	}
	else
	{
		panic("pgfault: access fail\n");
	}
 
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, PFTEMP,PTE_W | PTE_P | PTE_U);
	if(r < 0)
	{
		panic("pgfault: sys_page_alloc fail\n");
	}
 	addr = ROUNDDOWN(addr, PGSIZE);
	memcpy((void *)PFTEMP,(void *)addr,PGSIZE);
	r = sys_page_map(0,(void *)PFTEMP,0,(void *)addr,PTE_W | PTE_P | PTE_U);
	if(r < 0)
	{
		panic("pgfault: sys_page_map fail %e\n",r);
	}
	sys_page_unmap(0,(void *)PFTEMP);
	if(r < 0)
	{
		panic("pgfault: sys_page_unmap fail\n");
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	uintptr_t pn_va = pn * PGSIZE;

	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		if((r = sys_page_map(0, (void *)pn_va, envid, (void *)pn_va, (PTE_COW | PTE_P | PTE_U))) < 0){
			panic("sys_page_map fail\n");
		}
		if((r = sys_page_map(0, (void *)pn_va, 0, (void *)pn_va, (PTE_COW | PTE_P | PTE_U))) < 0){
			panic("sys_page_map fail\n");
		}
	}else{
		if((r = sys_page_map(0, (void *)pn_va, envid, (void *)pn_va, PTE_P | PTE_U)) < 0){
			panic("sys_page_map fail\n");
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int envid,r;
	uintptr_t pageVa;
	unsigned pn;
	set_pgfault_handler(pgfault);
	envid = sys_exofork();

	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];//这个是用户空间定义的变量
		return 0;
	}
	for(pageVa = UTEXT; pageVa < USTACKTOP; pageVa += PGSIZE){
		// check permissions.
		pn = PGNUM(pageVa);
		//cprintf("uvpd[PDX(pageVa)]: %x  uvpt[pn]:  %x\n",uvpd[PDX(pageVa)],uvpt[pn]);
		if((uvpd[PDX(pageVa)] & PTE_P) && (uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_U)){
			duppage(envid, pn);
		}
	}

    // alloc a page and map child exception stack
    if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U | PTE_P | PTE_W)) < 0)
        return r;
    extern void _pgfault_upcall(void);
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)//_pgfault_handler直接继承了，所以不用set_pgfault_handler
        return r;

    // Start the child environment running
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        panic("sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
