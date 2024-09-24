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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// M: create kmems for each CPU
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// M: initialize the allocator
void
kinit()
{
  char buf[10];
  for (int i = 0; i < NCPU; ++i) {
    snprintf(buf, 10, "kmem_CPU%d", i);
    // M: initlock accepts a string name
    initlock(&kmem[i].lock, buf);
  }
  // M: from the end of kernel address to PHYSTOP.
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  // M: pa should be aligned by 4096 bytes
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu = cpuid();
  pop_off();
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();
  pop_off();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r) {
    kmem[cpu].freelist = r->next;
  } else {
    // M: the current CPU does not have any free pages
    // M: so we should steal a free page from the others
    struct run* tmp;
    for (int i = 0; i < NCPU; ++i) {
      if (i == cpu)
        continue;
      acquire(&kmem[i].lock);
      tmp = kmem[i].freelist;
      if (tmp == 0) {
        release(&kmem[i].lock);
        continue;
      } else {
        // M: struct run* steal_head = kmem[i].freelist;
        // M: allocate 1024 pages from the other CPU at most
        int steal_count = 1024;
        while (--steal_count > 0 && tmp->next) {
          tmp = tmp->next;
        }
        kmem[cpu].freelist = kmem[i].freelist;
        kmem[i].freelist = tmp->next;
        // M: split the cpu's freelist and the stealed freelist
        tmp->next = 0;
        release(&kmem[i].lock);
        break;
      }
    }
    r = kmem[cpu].freelist;
    if (r) {
      kmem[cpu].freelist = r->next;
    }
  }
  release(&kmem[cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
