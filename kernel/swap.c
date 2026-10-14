// Swap space: a raw area of the disk just past the file system,
// holding pages evicted from physical memory. Not persistent across boots.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "fs.h"
#include "buf.h"
#include "defs.h"

#define BLKS_PER_PAGE (PGSIZE / BSIZE)

static struct {
  struct spinlock lock;
  char used[SWAPPAGES];
} swap;

void
swapinit(void)
{
  initlock(&swap.lock, "swap");
}

// copy a page to or from the 4 disk blocks of a swap slot.
static void
swap_io(int slot, void *pa, int write)
{
  for (int i = 0; i < BLKS_PER_PAGE; i++) {
    struct buf *b = bread(ROOTDEV, SWAPSTART + slot * BLKS_PER_PAGE + i);
    if (write) {
      memmove(b->data, (char *)pa + i * BSIZE, BSIZE);
      bwrite(b);
    } else {
      memmove((char *)pa + i * BSIZE, b->data, BSIZE);
    }
    brelse(b);
  }
}

// write the page at pa to a free swap slot.
// returns the slot, or -1 if swap is full.
int
swapout(void *pa)
{
  int slot;

  acquire(&swap.lock);
  for (slot = 0; slot < SWAPPAGES; slot++)
    if (!swap.used[slot])
      break;
  if (slot == SWAPPAGES) {
    release(&swap.lock);
    return -1;
  }
  swap.used[slot] = 1;
  release(&swap.lock);

  swap_io(slot, pa, 1);
  return slot;
}

// read swap slot into the page at pa and release the slot.
void
swapin(int slot, void *pa)
{
  swap_io(slot, pa, 0);
  swapfree(slot);
}

// release a swap slot without reading it (page was discarded).
void
swapfree(int slot)
{
  if (slot < 0 || slot >= SWAPPAGES)
    panic("swapfree: bad slot");
  acquire(&swap.lock);
  if (!swap.used[slot])
    panic("swapfree: not in use");
  swap.used[slot] = 0;
  release(&swap.lock);
}
