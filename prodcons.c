/*
* Operating Systems  [2INCO]  Practical Assignment
* Condition Variables Application
*
* Thomas Gian (0995114)
* Minjin Song (1194206)
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
static pthread_cond_t cond_prod = PTHREAD_COND_INITIALIZER; //to signal different producers
static pthread_cond_t cond_buffer[NROF_ITEMS];
static pthread_cond_t cond_cons = PTHREAD_COND_INITIALIZER; //to signal the consumer

static int produced_count = 0;
static int items_in_buffer = 0;
static int expected_item = 0;

int read_pointer = 0;
int write_pointer = 0;


/* producer thread */
static void *
producer (void * arg)
{
  while (produced_count < NROF_ITEMS)
  {
    // TODO:
    // * get the new item
    ITEM item = get_next_item();
    if (item == NROF_ITEMS) {
      break;
    }
    printf("producer got %d\n", item);
    rsleep (100);	// simulating all kind of activities...

    // TODO:
    // * put the item into buffer[]
    //
    // follow this pseudocode (according to the ConditionSynchronization lecture):
    //      mutex-lock;
    pthread_mutex_lock(&mutex);
    //      while not condition-for-this-producer
    while (item != expected_item) {
      //          wait-cv;
      printf("producer wait with %d\n", item);
      pthread_cond_wait(&cond_buffer[item], &mutex);
    }
    //      critical-section;
    buffer[write_pointer] = item;
    printf("producer wrote %d\n", item);
    write_pointer = (write_pointer + 1) % BUFFER_SIZE;

    //      possible-cv-signals;
    items_in_buffer++;
    printf("items in buffer = %d\n", items_in_buffer);
    if (items_in_buffer == 1) { //signal consumer that the buffer was empty
      pthread_cond_signal(&cond_cons);
    }
    //      mutex-unlock;
    pthread_mutex_unlock(&mutex);
    produced_count++;
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
    //      while not condition-for-this-consumer
    while (items_in_buffer == 0) {
      //          wait-cv;
      printf("consumer wait\n");
      pthread_cond_wait (&cond_cons, &mutex);
      printf("consumer unwait signalled\n");
    }
    //      critical-section;
    item = buffer[read_pointer];
    printf ("%d\n", item);
    read_pointer = (read_pointer + 1) % BUFFER_SIZE;
    items_in_buffer--;
    expected_item++;
    //      possible-cv-signals;
    printf("consumer items in buffer after consumption is %d\n", items_in_buffer);
    if (items_in_buffer < BUFFER_SIZE) {
      pthread_cond_signal (&cond_buffer[expected_item]);
    }
    //      mutex-unlock;
    pthread_mutex_unlock (&mutex);

    rsleep (100);		// simulating all kind of activities...
  }
  return (NULL);
}

int main (void)
{
  pthread_t producer_threads[NROF_PRODUCERS];
  pthread_t consumer_thread;

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
