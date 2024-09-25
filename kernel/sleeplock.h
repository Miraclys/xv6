// #include "spinlock.h"  // 确保包含 spinlock.h 以使用 struct spinlock
// #ifndef SLEEPLOCK_H
// #define SLEEPLOCK_H

// #include "spinlock.h"  // 确保包含 spinlock 头文件

// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

// #endif
