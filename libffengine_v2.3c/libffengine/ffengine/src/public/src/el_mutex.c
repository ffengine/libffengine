#include <pthread.h>
#include <stdlib.h>

#include "el_std.h"

EL_STATIC void *el_mutex_init(void)
{
	pthread_mutex_t *hmutex = malloc(sizeof(pthread_mutex_t));
	if(pthread_mutex_init(hmutex, NULL))
	{
		free(hmutex);
		return NULL;
	}
	return (void *)hmutex;
}

// On sucess, returns 0.
EL_STATIC int el_mutex_lock(void *hmutex)
{
	return pthread_mutex_lock((pthread_mutex_t *)hmutex);
}

// On sucess, returns 0.
EL_STATIC int el_mutex_unlock(void *hmutex)
{
	return pthread_mutex_unlock((pthread_mutex_t *)hmutex);
}

EL_STATIC void el_mutex_destroy(void *hmutex)
{
	pthread_mutex_destroy((pthread_mutex_t *)hmutex);
	free(hmutex);
}

