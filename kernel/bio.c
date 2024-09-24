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

# define NBUCKET 13

struct hashbuf {
  struct buf head;
  struct spinlock lock;
};

struct {
  // M: buf has the refnt, dev and blockno. 
  // M: the same thought as the page table
  // M: we could use several hash buckets to store the buffer blocks
  struct buf buf[NBUF];
  struct hashbuf bucket[NBUCKET];
} bcache;

int
hash(int block_number) {
  return block_number % NBUCKET;
}

void
binit(void)
{
  struct buf* b;
  char lockname[16];

  // M: initialize NBUCKET buckets
  for (int i = 0; i < NBUCKET; ++i) {
    snprintf(lockname, sizeof(lockname), "bcache%d", i);
    initlock(&bcache.bucket[i].lock, lockname);
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    int bucket_index = hash(b - bcache.buf);
    b->next = bcache.bucket[bucket_index].head.next;
    b->prev = &bcache.bucket[bucket_index].head;
    char name[16];
    snprintf(name, sizeof(name), "buffer%d", b - bcache.buf);
    initsleeplock(&b->lock, name);
    bcache.bucket[bucket_index].head.next->prev = b;
    bcache.bucket[bucket_index].head.next = b;
  }
}

struct buf* 
get_buffer_ptr(uint dev, uint blockno, int bid) {

  // M: if the block is not cached
  struct buf* b = 0;
  struct buf* tmp;

  // M: we use the LRU algorithm to replace the buffer block
  for(int i = bid, time = 0; time != NBUCKET; i = (i + 1) % NBUCKET) {
    ++time;
    if(i != bid) {
      if(!holding(&bcache.bucket[i].lock))
        acquire(&bcache.bucket[i].lock);
      else
        continue;
    }
    // M: start from the head of the list
    // M: which means the least recently used buffer block
    for(tmp = bcache.bucket[i].head.next; tmp != &bcache.bucket[i].head; tmp = tmp->next)
      if(tmp->refcnt == 0 && (b == 0 || tmp->timestamps < b->timestamps))
        b = tmp;
    if(b) {
      if(i != bid) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        // M: we have two buckets locks here
        // M: we should release the lock in the reverse order
        release(&bcache.bucket[i].lock);

        b->next = bcache.bucket[bid].head.next;
        b->prev = &bcache.bucket[bid].head;
        bcache.bucket[bid].head.next->prev = b;
        bcache.bucket[bid].head.next = b;
      }

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      acquire(&tickslock);
      b->timestamps = ticks;
      release(&tickslock);

      release(&bcache.bucket[bid].lock);
      acquiresleep(&b->lock);
      return b;
    } else {
      if(i != bid)
        release(&bcache.bucket[i].lock);
    }
  }
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf* b;

  int bid = hash(blockno);
  acquire(&bcache.bucket[bid].lock);

  for(b = bcache.bucket[bid].head.next; b != &bcache.bucket[bid].head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;

      acquire(&tickslock);
      b->timestamps = ticks;
      release(&tickslock);

      release(&bcache.bucket[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf* buffer_ptr = get_buffer_ptr(dev, blockno, bid);
  
  if (buffer_ptr == 0) {
    panic("bget: no buffers");
  }

  return buffer_ptr;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    // M: read from the disk
    // M: talk to the disk hardware
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  // M: return a locked buffer
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
  if (!holdingsleep(&b->lock))
    panic("brelse");

  int id = hash(b->blockno);
  releasesleep(&b->lock);
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    acquire(&tickslock);
    b->timestamps = ticks;
    release(&tickslock);
  }
  release(&bcache.bucket[id].lock);
}

void
bpin(struct buf* b) {
  int bid = hash(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt++;
  release(&bcache.bucket[bid].lock);
}

void
bunpin(struct buf* b) {
  int bid = hash(b->blockno);
  acquire(&bcache.bucket[bid].lock);
  b->refcnt--;
  release(&bcache.bucket[bid].lock);
}

