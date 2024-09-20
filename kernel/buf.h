struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock; // M: every buffer block has a lock, which is used to protect the buffer block 
  // and helps us visit different buffer blocks concurrently
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
  uint timestamps; // M: record the time steps of the buffer block
};

