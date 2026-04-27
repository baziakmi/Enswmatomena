#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define QUEUESIZE 10
#define LOOP 10000
#define ANGLE_AMOUNT 10

#define P 5 // num of producers
#define Q 5 // num of consumers

double total_wait_time = 0; 
int tasks_proc = 0;// tasks processed
pthread_mutex_t stats_mut = PTHREAD_MUTEX_INITIALIZER;

double angles[] = {0, 10, 20, 30, 45, 60, 75, 90, 180, 350};

void *producer (void *args);
void *consumer (void *args);

void calculate_sin(void *arg) {
    double *angle = (double *) arg;
    for(int i=0; i<ANGLE_AMOUNT; i++) {
        double result = sin(*angle);
    }
    free(arg);
    return NULL;
}

typedef struct {
    void * (*work)(void *);
    void * arg;
    struct timeval start_time; // start time
} workFunction;

typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in);
void queueDel (queue *q, workFunction *out);

int main ()
{
  queue *fifo;
  pthread_t pro[P], con[Q]; // pinakas  me threads apo producer kai consumer

  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
  // create threads
  for (int i = 0; i < P; i++) pthread_create(&pro[i], NULL, producer, fifo);
  for (int i = 0; i < Q; i++) pthread_create(&con[i], NULL, consumer, fifo);

  //join producers
  for (int i = 0; i < P; i++) pthread_join(pro[i], NULL);

  for (int i = 0; i < Q; i++) {
    workFunction poison_pill;
    poison_pill.work = NULL; // poison pill gia na mhn einai atermono
    poison_pill.arg = NULL;

    pthread_mutex_lock(fifo->mut);
    while (fifo->full) {
        pthread_cond_wait(fifo->notFull, fifo->mut);
    }
    queueAdd(fifo, poison_pill);
    pthread_mutex_unlock(fifo->mut);
    pthread_cond_signal(fifo->notEmpty);
  }

  //join consumers
  for (int i = 0; i < Q; i++) {
      pthread_join(con[i], NULL);
  }

  queueDelete (fifo);

  printf("Average Wait Time: %f us\n", total_wait_time / tasks_proc);

  return 0;
}

void *producer (void *q)
{
  queue *fifo;
  int i;

  fifo = (queue *)q;

  for (i = 0; i < LOOP / P; i++) {
    workFunction task;
    
    double *arg = (double *)malloc(sizeof(double));
    *arg = angles[i % ANGLE_AMOUNT];

    task.work = calculate_sin;
    task.arg = arg;

    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    gettimeofday(&task.start_time, NULL);
    queueAdd (fifo, task);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
    usleep (100000);
  }
  return (NULL);
}

void *consumer (void *q)
{
  queue *fifo;

  fifo = (queue *)q;
  workFunction d;

  struct timeval end_time;

  while(1) {
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      //printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    }
    queueDel (fifo, &d);

    gettimeofday(&end_time, NULL);

    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);

    if (d.work == NULL) {// poison pill check
      //printf("Consumer: Received poison pill. Exiting...\n");
      pthread_exit(NULL); // exit pthread
    }

    long seconds = end_time.tv_sec - d.start_time.tv_sec;
    long useconds = end_time.tv_usec - d.start_time.tv_usec;
    long waiting_usec = (seconds * 1000000) + useconds;

    pthread_mutex_lock(&stats_mut);
    total_wait_time += waiting_usec;
    tasks_proc++;
    pthread_mutex_unlock(&stats_mut);

    //printf ("consumer: received %d.\n", d);
    d.work(d.arg);
  }
  return (NULL);
}

queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunction in)
{
  q->buf[q->tail] = in;
  q->tail++;

  if (q->tail == QUEUESIZE)
    q->tail = 0;

  if (q->tail == q->head)
    q->full = 1;

  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];

  q->head++;

  if (q->head == QUEUESIZE)
    q->head = 0;

  if (q->head == q->tail)
    q->empty = 1;

  q->full = 0;

  return;
}
