#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // System call
    if(killed(p))
      kexit(-1);

    p->trapframe->epc += 4;
    intr_on();
    syscall();

  } else if((which_dev = devintr()) != 0){
    // Device interrupt — handled

  } else if(r_scause() == 15){
    // Store page fault: try COW first, then lazy allocation
    uint64 va = r_stval();
    if(cowfault(p->pagetable, va) != 0){
      // Not a COW page — try lazy allocation
      if(vmfault(p->pagetable, va, 0) == 0){
        // Neither COW nor lazy — kill the process
        setkilled(p);
      }
    }

  } else if(r_scause() == 13){
    // Load page fault: handle lazy allocation
    if(vmfault(p->pagetable, r_stval(), 1) == 0){
      setkilled(p);
    }

  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    kexit(-1);

  if(which_dev == 2)
    yield();

  prepare_return();

  uint64 satp = MAKE_SATP(p->pagetable);
  return satp;
}

void
prepare_return(void)
{
  struct proc *p = myproc();

  intr_off();

  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();

  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  w_sepc(p->trapframe->epc);
}

void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  if(which_dev == 2 && myproc() != 0)
    yield();

  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }
  w_stimecmp(r_time() + 1000000);
}

int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    clockintr();
    return 2;
  } else {
    return 0;
  }
}