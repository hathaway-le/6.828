/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
.data;                                                              \
	.long name, num;           		             \
.text; 			\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
.data;                                                              \
	.long name, num;           		             \
.text;				\
	.type name, @function;						\
	.align 2;							\
name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.data
	.global entryPointOfTraps
	entryPointOfTraps:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(divide_error, T_DIVIDE);
TRAPHANDLER_NOEC(debug, T_DEBUG);
TRAPHANDLER_NOEC(nmi, T_NMI);
TRAPHANDLER_NOEC(int3, T_BRKPT);
TRAPHANDLER_NOEC(overflow, T_OFLOW);
TRAPHANDLER_NOEC(bounds, T_BOUND);
TRAPHANDLER_NOEC(invalid, T_ILLOP);
TRAPHANDLER_NOEC(device_not_available, T_DEVICE);
TRAPHANDLER_NOEC(coprocessor_segment_overrun, T_COPROC);

TRAPHANDLER(doublefault, T_DBLFLT);
TRAPHANDLER(invalid_tss, T_TSS);
TRAPHANDLER(segment_not_present, T_SEGNP);
TRAPHANDLER(stack_segment, T_STACK);
TRAPHANDLER(general_protection, T_GPFLT);
TRAPHANDLER(page_fault, T_PGFLT);
TRAPHANDLER(reserved, T_RES);

TRAPHANDLER_NOEC(coprocessor_error, T_FPERR);
TRAPHANDLER_NOEC(alignment_check, T_ALIGN);
TRAPHANDLER_NOEC(machine_check, T_MCHK);
TRAPHANDLER_NOEC(simd_coprocessor_error, T_SIMDERR);
TRAPHANDLER_NOEC(syscallHandler, T_SYSCALL);

TRAPHANDLER_NOEC(IRQ_timerHandler, IRQ_OFFSET + IRQ_TIMER);
TRAPHANDLER_NOEC(IRQ_kbdHandler, IRQ_OFFSET + IRQ_KBD);
TRAPHANDLER_NOEC(IRQ_2_Handler, IRQ_OFFSET + 2);
TRAPHANDLER_NOEC(IRQ_3_Handler, IRQ_OFFSET + 3);
TRAPHANDLER_NOEC(IRQ_serialHandler, IRQ_OFFSET + IRQ_SERIAL);
TRAPHANDLER_NOEC(IRQ_5_Handler, IRQ_OFFSET + 5);
TRAPHANDLER_NOEC(IRQ_6_Handler, IRQ_OFFSET + 6);
TRAPHANDLER_NOEC(IRQ_spuriousHandler, IRQ_OFFSET + IRQ_SPURIOUS);
TRAPHANDLER_NOEC(IRQ_8_Handler, IRQ_OFFSET + 8);
TRAPHANDLER_NOEC(IRQ_9_Handler, IRQ_OFFSET + 9);
TRAPHANDLER_NOEC(IRQ_10_Handler, IRQ_OFFSET + 10);
TRAPHANDLER_NOEC(IRQ_11_Handler, IRQ_OFFSET + 11);
TRAPHANDLER_NOEC(IRQ_12_Handler, IRQ_OFFSET + 12);
TRAPHANDLER_NOEC(IRQ_13_Handler, IRQ_OFFSET + 13);
TRAPHANDLER_NOEC(IRQ_ideHandler, IRQ_OFFSET + IRQ_IDE);
TRAPHANDLER_NOEC(IRQ_15_Handler, IRQ_OFFSET + 15);
TRAPHANDLER_NOEC(IRQ_16_Handler, IRQ_OFFSET + 16);
TRAPHANDLER_NOEC(IRQ_17_Handler, IRQ_OFFSET + 17);
TRAPHANDLER_NOEC(IRQ_18_Handler, IRQ_OFFSET + 18);
TRAPHANDLER_NOEC(IRQ_errorHandler, IRQ_OFFSET + IRQ_ERROR);
/*
 * Lab 3: Your code here for _alltraps,可以不需要.globl  ds和es必须是pushl，可能和Trapframe有关,call仅自动装填返回地址和ebp，
 * trap的参数struct Trapframe *tf就是pushl %esp
 */

_alltraps:
	pushl %ds
	pushl %es
	pushal
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap
