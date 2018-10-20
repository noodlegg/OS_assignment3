/*
* Operating Systems  [2INCO]  Practical Assignment
* Condition Variables Application
*
* Thomas Gian (0995114)
* Minjin Song (STUDENT_NR_2)
*
* Grading:
* Students who hand in clean code that fully satisfies the minimum requirements will get an 8.
* "Extra" steps can lead to higher marks because we want students to take the initiative.
* Extra steps can be, for example, in the form of measurements added to your code, a formal
* analysis of deadlock freeness etc.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static void rsleep (int t);			// already implemented (see below)
static ITEM get_next_item (void);	// already implemented (see below)
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int produced_count = 0;
static int items_in_buffer = 0;
static int expected_item = 0;


/* producer thread */
static void *
producer (void * arg)
{
  int id = arg; //identifier of this producers

  while (produced_count < NROF_ITEMS)
  {
    // TODO:
    // * get the new item
    ITEM item = get_next_item();

    rsleep (100);	// simulating all kind of activities...

    // TODO:
    // * put the item into buffer[]
    //
    // follow this pseudocode (according to the ConditionSynchronization lecture):
    //      mutex-lock;
    pthread_mutex_lock(&mutex);
    printf ("Producer: mutex locked!\n");
    //      while not condition-for-this-producer
    while (items_in_buffer >= BUFFER_SIZE) {
      //          wait-cv;
      pthread_cond_wait(&cond, &mutex);
      printf ("Producer: signalled!\n");
    }
    //      critical-section;
    for (int i = 0; i < BUFFER_SIZE; i++) {
      if (buffer[i] < item) {
        buffer[i] = item;
        printf ("Producer: written item %d to buffer index %d\n", item, i);
        break;
      }
    }
    //      possible-cv-signals;
    items_in_buffer++;
    if (items_in_buffer == 1) { //signal consumer that the buffer was empty
      pthread_cond_signal(&cond);
    }
    //      mutex-unlock;
    pthread_mutex_unlock(&mutex);
    printf ("Producer: mutex unlocked!\n");
    produced_count++;
    printf ("Producer: produced_count is %d\n", produced_count);
    //
    // (see condition_test() in condition_basics.c how to use condition variables)
  }
  return (NULL);
}

/* consumer thread */
static void *
consumer (void * arg)
{
  ITEM item;
  while (expected_item < NROF_ITEMS)
  {
    // TODO:
    // * get the next item from buffer[]
    // * print the number to stdout
    //
    // follow this pseudocode (according to the ConditionSynchronization lecture):
    //      mutex-lock;
    pthread_mutex_lock (&mutex);
    printf ("   Consumer: mutex locked!\n");
    //      while not condition-for-this-consumer
    while (items_in_buffer <= 0) {
      //          wait-cv;
      pthread_cond_wait (&cond, &mutex);
      printf ("   Consumer: non-empty buffer signalled!\n");
    }
    //      critical-section;
    for (int i = 0; i < BUFFER_SIZE; i++) {
      if (buffer[i] == expected_item) {
        item = buffer[i];
        buffer[i] = 0;  // clear the index value
        items_in_buffer--;
        printf ("   Consumer: item %d taken out of buffer\n", item);
        break;
      }
    }
    expected_item++;
    //      possible-cv-signals;
    if (items_in_buffer == 0) {
      pthread_cond_signal (&cond);
    }
    //      mutex-unlock;
    pthread_mutex_unlock (&mutex);
    printf ("   Consumer: mutex unlocked!\n");

    rsleep (100);		// simulating all kind of activities...
  }
  return (NULL);
}

int main (void)
{
  pthread_t producer_threads[NROF_PRODUCERS];
  pthread_t consumer_thread;

  //initialize buffer values
  for (int n = 0; n < BUFFER_SIZE; n++) {
    buffer[n] = -1;
  }

  for (int i = 0; i < BUFFER_SIZE; i++) {
    printf ("item %d at buffer index %d\n", buffer[i], i);
  }
  // TODO:
  // * startup the producer threads and the consumer thread
  for (int i = 0; i < NROF_PRODUCERS; i++) {
    pthread_create (&producer_threads[i], NULL, producer, (void*) i);
  }
  pthread_create (&consumer_thread, NULL, consumer, NULL);

  // * wait until all threads are finished
  for (int i = 0; i < NROF_PRODUCERS; i++) {
    pthread_join (producer_threads[i], NULL);
  }
  pthread_join (consumer_thread, NULL);

  return (0);
}

/*
* rsleep(int t)
*
* The calling thread will be suspended for a random amount of time between 0 and t microseconds
* At the first call, the random generator is seeded with the current time
*/
static void
rsleep (int t)
{
  static bool first_call = true;

  if (first_call == true)
  {
    srandom (time(NULL));
    first_call = false;
  }
  usleep (random () % t);
}


/*
* get_next_item()
*
* description:
*		thread-safe function to get a next job to be executed
*		subsequent calls of get_next_item() yields the values 0..NROF_ITEMS-1
*		in arbitrary order
*		return value NROF_ITEMS indicates that all jobs have already been given
*
* parameters:
*		none
*
* return value:
*		0..NROF_ITEMS-1: job number to be executed
*		NROF_ITEMS:		 ready
*/
static ITEM
get_next_item(void)
{
  static pthread_mutex_t	job_mutex	= PTHREAD_MUTEX_INITIALIZER;
  static bool 			jobs[NROF_ITEMS+1] = { false };	// keep track of issued jobs
  static int              counter = 0;    // seq.nr. of job to be handled
  ITEM 					found;          // item to be returned

  /* avoid deadlock: when all producers are busy but none has the next expected item for the consumer
  * so requirement for get_next_item: when giving the (i+n)'th item, make sure that item (i) is going to be handled (with n=nrof-producers)
  */
  pthread_mutex_lock (&job_mutex);

  counter++;
  if (counter > NROF_ITEMS)
  {
    // we're ready
    found = NROF_ITEMS;
  }
  else
  {
    if (counter < NROF_PRODUCERS)
    {
      // for the first n-1 items: any job can be given
      // e.g. "random() % NROF_ITEMS", but here we bias the lower items
      found = (random() % (2*NROF_PRODUCERS)) % NROF_ITEMS;
    }
    else
    {
      // deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
      found = counter - NROF_PRODUCERS;
      if (jobs[found] == true)
      {
        // already handled, find a random one, with a bias for lower items
        found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
      }
    }

    // check if 'found' is really an unhandled item;
    // if not: find another one
    if (jobs[found] == true)
    {
      // already handled, do linear search for the oldest
      found = 0;
      while (jobs[found] == true)
      {
        found++;
      }
    }
  }
  jobs[found] = true;

  pthread_mutex_unlock (&job_mutex);
  return (found);
}
