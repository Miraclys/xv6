#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

// M: nthread means the number of threads that will be synchronized by the barrier
static int nthread = 1;
// M: round is the number of times the barrier has been passed
static int round = 0;

struct barrier {
  pthread_mutex_t barrier_mutex;
  pthread_cond_t barrier_cond;
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

// M: this is the function we need to implement
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //

  // M: mutex is used to protect the critical section
  // M: cond is used to coordinate the running order between threads
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;

  if (bstate.nthread < nthread) {
    // M: pthread_cond_wait will sleep the current thread and unlock the mutex
    // M: when the condition is signaled, the thread will wake up and lock the mutex again
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } 

  if (bstate.nthread == nthread) {
    bstate.round++;
    bstate.nthread = 0;
    // M: pthread_cond_broadcast will wake up all threads that are sleeping on the condition
    // M: instead of waking up only one thread like pthread_cond_signal
    pthread_cond_broadcast(&bstate.barrier_cond);
  }

  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {
    int t = bstate.round;
    assert (i == t);
    barrier();
    usleep(random() % 100);
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
