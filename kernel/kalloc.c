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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    char name[10] = {0};
    snprintf(name, 9, "kmem-%d", i);
    initlock(&kmem[i].lock, name);
  }
  
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int hart_id = cpuid();
  

  acquire(&kmem[hart_id].lock);
  r->next = kmem[hart_id].freelist;
  kmem[hart_id].freelist = r;
  release(&kmem[hart_id].lock);

  pop_off();
}

void *
ksteal(int hart_id) {
  struct run *r;

  for (int i = 1; i < NCPU; i++) {
    int next_hart_id = (hart_id + i) % NCPU;
    acquire(&kmem[next_hart_id].lock);
    r = kmem[next_hart_id].freelist;

    if (r) {
      kmem[next_hart_id].freelist = r->next;
      
    }
    release(&kmem[next_hart_id].lock);

    if (r) break;
  }
  
  return r;
  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int hart_id = cpuid();
  
  acquire(&kmem[hart_id].lock);
  r = kmem[hart_id].freelist;
  if(r)
    kmem[hart_id].freelist = r->next;
  release(&kmem[hart_id].lock);

  if (r == 0) r = ksteal(hart_id); 

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  pop_off();
  return (void*)r;
}

