// test user-level fault handler -- alloc pages to fix faults

#include <inc/lib.h>

void
handler(struct UTrapframe *utf)
{
	int r;
	void *addr = (void*)utf->utf_fault_va;

	cprintf("fault %x\n", addr);
	cprintf("ip %x\n",utf->utf_eip);
	cprintf("ROUNDDOWN: %x\n",ROUNDDOWN(addr, PGSIZE));
	if ((r = sys_page_alloc(0, ROUNDDOWN(addr, PGSIZE),
				PTE_P|PTE_U|PTE_W)) < 0)
		panic("allocating at %x in page fault handler: %e", addr, r);
	cprintf("addr: %x\n",addr);
	snprintf((char*) addr, 100, "this string was faulted in at %x", addr);
}

void
umain(int argc, char **argv)
{
	set_pgfault_handler(handler);
	//*(int*)0xDeadBeef = 0;
	cprintf("%s\n", (char*)0xDeadBeef);//异常后又回到vprintfmt里面，此时sys_page_alloc和snprintf生效了
	cprintf("%s\n", (char*)0xCafeBffe);//snprintf的时候字符串会指向cafec000（0xCafeBffe+2）嵌套page fault，从异常回来后，继续之前的copy，第二个handler里面的copy会被覆盖
}
/*
fault deadbeef
ip 8005f1
ROUNDDOWN: deadb000
this string was faulted in at deadbeef
fault cafebffe
ip 8005f1   //vprintfmt
ROUNDDOWN: cafeb000
fault cafec000
ip 8003af   //sprintputch
ROUNDDOWN: cafec000
this string was faulted in at cafebffe
 */
