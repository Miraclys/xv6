#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

// M: we should optimize the gitpid() function
uint64
sys_getpid(void)
{
  return myproc()->pid;
  // return myproc()->usyscall_page->pid;
  // struct usyscall *u = (struct usyscall *)USYSCALL;
  // return u->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
// M: implementation of syscall sys_pagccess
// M: the function is used to check the accessed bits of the pages
// M: so we should clear the accessed bits of the pages after checking
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.

  // M: the start address
  // M: the number of pages
  // M: the address of access bits
  uint64 va;
  int pagenum;
  uint64 abitsaddr;

  // M: get the three arguments

  // M: get the start of virtual address
  argaddr(0, &va);
  // M: get the number of pages to check
  argint(1, &pagenum);
  // M: get the address of access bits
  argaddr(2, &abitsaddr);

  // M: maskbits is used to store the accessed bits
  uint64 maskbits = 0;
  struct proc *proc = myproc();

  // M: we should check pagenum pages
  for (int i = 0; i < pagenum; i++) {

    // M: obtain the corresponding pte
    pte_t *pte = walk(proc->pagetable, va+i*PGSIZE, 0);

    if (pte == 0)
      panic("[ERROR] sys_pagccess page not exist.");

    // M: PTE_FLAGS is used to get the flags of the pte
    // M: if the page is accessed
    if (PTE_FLAGS(*pte) & PTE_A) {
      maskbits = maskbits | (1L << i);
    }

    // clear PTE_A, set PTE_A bits zero
    // M: so that we could check the accessed bits of the pages next time
    *pte = ((*pte&PTE_A) ^ *pte) ^ 0 ;
  }

  // M: copy the maskbits to the valuable whose address is abitsaddr in the user space
  if (copyout(proc->pagetable, abitsaddr, (char *)&maskbits, sizeof(maskbits)) < 0)
    panic("[ERROR] sys_pgacess copyout error");

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
