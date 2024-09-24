#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

// M: BUCKET means "桶" in Chinese.
#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;

pthread_mutex_t lock[NBUCKET]; // M: declear a lock

double
now()
{
 struct timeval tv;
 // M: gettimeofday() is a function in <sys/time.h>
 // M: it gets the current time.
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// M: insert a key-value pair into the bucket.
static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

static 
void put(int key, int value)
{
  // M: i means the bucket number.
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  // M: if e is true, means the key is already present.
  // M: so we need to update the value.
  if(e){
    // update the existing key.
    e->value = value;
  } else { // M: if e is false, means the key is not present.
    // the new is new.

    pthread_mutex_lock(&lock[i]); // M: lock the bucket

    // M: the double pointer stores the address of a pointer.
    insert(key, value, &table[i], table[i]);

    pthread_mutex_unlock(&lock[i]); // M: unlock the bucket
  }

}

// M: get the corresponding value of the key.
static struct entry*
get(int key)
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;


  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();
  }

  for (int i = 0; i < NBUCKET; i++) {
    // table[i] = 0;
    pthread_mutex_init(&lock[i], NULL); // M: init the lock
  }

  //
  // first the puts
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
