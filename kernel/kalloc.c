// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// neww ds for COW ref count 25 to 29
struct {
  struct spinlock lock;
  int count[PHYSTOP / PGSIZE];
} refcnt;

// neww ref increment function 32 to 38
void
refinc(uint64 pa)
{
  acquire(&refcnt.lock);
  refcnt.count[pa / PGSIZE]++;
  release(&refcnt.lock);
}

// neww fun to decremtnt reference 
void
refdec(uint64 pa)
{
  acquire(&refcnt.lock);
  refcnt.count[pa / PGSIZE]--;
  release(&refcnt.lock);
}

// neww func to get reference count 
int
refget(uint64 pa)
{
  int n;
  acquire(&refcnt.lock);
  n = refcnt.count[pa / PGSIZE];
  release(&refcnt.lock);
  return n;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt"); // COW: initialize ref count spinlock
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// neww kfree updation 87 to 93
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // COW: Decrement ref count. Only free if it reaches 0.
  acquire(&refcnt.lock);
  refcnt.count[(uint64)pa / PGSIZE]--;
  if(refcnt.count[(uint64)pa / PGSIZE] > 0){
    release(&refcnt.lock);
    return; // Page is still shared — do not free
  }
  release(&refcnt.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// neww line added so pg ref starts from 1. 120 to 122
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); 
    acquire(&refcnt.lock);
    refcnt.count[(uint64)r / PGSIZE] = 1;
    release(&refcnt.lock);
  }
  return (void*)r;
}