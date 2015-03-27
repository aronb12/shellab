#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "help.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in below.
 *
 * === User information ===
 * Team: The_Foot_Clan
 * User 1: arona12
 * SSN: 2106844339
 * User 2: sveinnt12
 * SSN: 2803872909
 * === End User Information ===
 ********************************************************/

static void *barber_work(void *arg);

/*********************************************************
	Wrapper functions definitions
 ********************************************************/

void unix_error(char *msg);

// Thread control
void posix_error(int code, char *msg);

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, void * (*routine)(void *), void *argp);

void Pthread_detach(pthread_t tid);

// Semaphores

void Sem_init(sem_t *sem, int pshared, unsigned int value);

void P(sem_t *sem);

void V(sem_t *sem);

/*********************************************************
	End of wrapper functions definitions
 ********************************************************/

struct chairs
{
	struct customer **customer;	/* Array of customers */
	int max;					/* Chairs in waiting room */
	sem_t mutex;				/* Semaphore to protect access to chairs in waiting room */
	sem_t empty;				/* Available chairs */
	sem_t barber;
};

struct barber
{
	int room;
	struct simulator *simulator;
};

struct simulator
{
	struct chairs chairs;
	
	pthread_t *barberThread;
	struct barber **barber;
};

/**
 * Initialize data structures and create waiting barber threads.
 */
static void setup(struct simulator *simulator)
{
	struct chairs *chairs = &simulator->chairs;

	/* Setup chairs struct */
	chairs->max = thrlab_get_num_chairs();

	/* Setup semaphores*/
	Sem_init(&chairs->mutex, 0, 1);		// 1 == there is an available chair to start
	Sem_init(&chairs->empty, 0, 1);		// 0 == a barber can't start without a customer
	Sem_init(&chairs->barber, 0, 0);	// sem_init(&chairs->barber, 0, 0);

	/* Create chairs*/
	chairs->customer = malloc(sizeof(struct customer *) * thrlab_get_num_chairs());
	
	/* Create barber thread data */
	simulator->barberThread = malloc(sizeof(pthread_t) * thrlab_get_num_barbers());
	simulator->barber = malloc(sizeof(struct barber*) * thrlab_get_num_barbers());

	/* Start barber threads */
	struct barber *barber;
	for (unsigned int i = 0; i < thrlab_get_num_barbers(); i++) 
	{
		barber = calloc(sizeof(struct barber), 1);
		barber->room = i;
		barber->simulator = simulator;
		simulator->barber[i] = barber;

		Pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
		Pthread_detach(simulator->barberThread[i]);

		// pthread_create(&simulator->barberThread[i], 0, barber_work, barber);
		// pthread_detach(simulator->barberThread[i]);
	}
}

/**
 * Free all used resources and end the barber threads.
 */
static void cleanup(struct simulator *simulator)
{
	/* Free chairs */
	free(simulator->chairs.customer);

	/* Free semaphores */
	sem_destroy(&simulator->chairs.mutex);
	sem_destroy(&simulator->chairs.empty);
	sem_destroy(&simulator->chairs.barber);

	/* Free barber thread data */
	free(simulator->barber);
	free(simulator->barberThread);
}

/**
 * Called in a new thread each time a customer has arrived.
 */
static void customer_arrived(struct customer *customer, void *arg)
{
	struct simulator *simulator = arg;
	struct chairs *chairs = &simulator->chairs;

	/* Customer mutex starts as 0 */
	Sem_init(&customer->mutex, 0, 0);

	/* Wait for a chair to be available */
	if(sem_trywait(&chairs->empty) < 0)
	{
		/* No chair available. Reject customer */
		thrlab_reject_customer(customer);
	}
	else
	{
		/* Lock chairs */
		V(&chairs->mutex);

		/* Chair available. Accept customer */
		thrlab_accept_customer(customer);

		/* TODO: */
		/* Choose seat for customer */
		chairs->customer[0] = customer;

		// sem_post(&chairs->mutex);
		/* Unlock chairs */
		V(&chairs->mutex);

		// barber can start cutting hair
		V(&chairs->barber);
		P(&customer->mutex);
		// sem_post(&chairs->barber);
		// sem_wait(&customer->mutex);
	}
}

static void *barber_work(void *arg)
{
	struct barber *barber = arg;
	struct chairs *chairs = &barber->simulator->chairs;
	struct customer *customer = 0; /* TODO: Fetch a customer from a chair */

	/* Main barber loop */
	while (true)
	{
		/* TODO: Here you must add you semaphores and locking logic */
		// barber waits for his semaphore to increment
		P(&chairs->barber);
		// sem_wait(&chairs->barber);

		P(&chairs->mutex);
		// sem_wait(&chairs->mutex);
		
		/* TODO: You must choose the customer */
		customer = chairs->customer[0];
		// take customer from waiting room and put in chair 
		thrlab_prepare_customer(customer, barber->room);
		V(&chairs->mutex);
		// sem_post(&chairs->mutex);
		
		// make chair in waiting room available
		V(&chairs->empty);
		// sem_post(&chairs->chair);
		// Cut hair
		thrlab_sleep(5 * (customer->hair_length - customer->hair_goal));
		// take customer out of shop
		thrlab_dismiss_customer(customer, barber->room);
		// customer can leave barber shop
		V(&customer->mutex);
		// sem_post(&customer->mutex);
	}
	return NULL;
}

int main (int argc, char **argv)
{
	struct simulator simulator;

	thrlab_setup(&argc, &argv);
	setup(&simulator);

	thrlab_wait_for_customers(customer_arrived, &simulator);

	thrlab_cleanup();
	cleanup(&simulator);

	return EXIT_SUCCESS;
}

/*********************************************************
	Wrapper functions implementations
 ********************************************************/

void unix_error(char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
	exit(0);
}

// Thread control
void posix_error(int code, char *msg)
{
	fprintf(stderr, "%s: %s\n", msg, strerror(code));
	exit(0);
}

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, void * (*routine)(void *), void *argp)
{
	int rc;

	if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
		posix_error(rc, "Pthread_create error");
}

void Pthread_detach(pthread_t tid)
{
	int rc;

	if ((rc = pthread_detach(tid)) != 0)
		posix_error(rc, "Pthread_detach error");
}

// Semaphores
void Sem_init(sem_t *sem, int pshared, unsigned int value)
{
	if (sem_init(sem, pshared, value) < 0)
		unix_error("Sem_init error");
}

void P(sem_t *sem)
{
	if (sem_wait(sem) < 0)
		unix_error("P error");
}

void V(sem_t *sem)
{
	if (sem_post(sem) < 0)
		unix_error("V error");
}

/*********************************************************
	End of wrapper functions implementations
 ********************************************************/
