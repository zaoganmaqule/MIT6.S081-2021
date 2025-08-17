// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKETSIZE 13
#define BUFFERSIZE 5
// #define hash(blockno) ((blockno) % BUCKETSIZE)

extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[BUFFERSIZE];
}bcachebucket[BUCKETSIZE]; 

static int 
hash(uint blockno) {
  return blockno % BUCKETSIZE;
} 
// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

void
binit(void)
{
  for (int i = 0; i < BUCKETSIZE; i++) {
    initlock(&bcachebucket[i].lock, "bcachebucket");
    for (int j = 0; j < BUFFERSIZE; j++) {
      initsleeplock(&bcachebucket[i].buf[j].lock, "buffer");
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket_index = hash(blockno);

  acquire(&bcachebucket[bucket_index].lock);
  
  //缓存中有无该块
  for (int i = 0; i < BUFFERSIZE; i++) {
    b = &bcachebucket[bucket_index].buf[i];
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      b->lastuse = ticks;
      release(&bcachebucket[bucket_index].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //缓存中有无空闲
  uint least_use = 0xffffffff;
  int index = -1;
  for (int i = 0; i < BUFFERSIZE; i++) {
      b = &bcachebucket[bucket_index].buf[i];
      if (b->refcnt == 0 && b->lastuse < least_use) {
        index = i;
        least_use = b->lastuse;
      }
  }

  //该哈希表满了
  if (index == -1) {
    release(&bcachebucket[bucket_index].lock);

    for (int i = 0; i < BUCKETSIZE; i++) {
      if (i == bucket_index) continue;
      acquire(&bcachebucket[i].lock);

      for (int j = 0; j < BUFFERSIZE; j++) {
        b = &bcachebucket[i].buf[j];
        if (b->refcnt == 0 && b->lastuse < least_use) {
          index = j;
          bucket_index = i;
          least_use = b->lastuse;
        }
      }
      release(&bcachebucket[i].lock);
    }
    if (index == -1) panic("bget: no buffers");

    acquire(&bcachebucket[bucket_index].lock);
  }

  b = &bcachebucket[bucket_index].buf[index];
  b->dev = dev;
  b->refcnt = 1;
  b->blockno = blockno;
  b->lastuse = ticks;
  b->valid = 0;

  release(&bcachebucket[bucket_index].lock);
  acquiresleep(&b->lock);
  return b;
  
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  acquire(&bcachebucket[hash(b->blockno)].lock);
  b->refcnt--;
  
  release(&bcachebucket[hash(b->blockno)].lock);
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {

  acquire(&bcachebucket[hash(b->blockno)].lock);
  b->refcnt++;
  release(&bcachebucket[hash(b->blockno)].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcachebucket[hash(b->blockno)].lock);
  b->refcnt--;
  release(&bcachebucket[hash(b->blockno)].lock);
}


