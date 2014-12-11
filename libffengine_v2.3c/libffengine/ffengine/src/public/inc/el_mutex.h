
#ifndef __EL_PLAYER_LOCK_H__
#define __EL_PLAYER_LOCK_H__

#include "el_std.h"

// On sucess, returns a mutex lock.
EL_STATIC void *el_mutex_init(void);

// On sucess, returns 0.
EL_STATIC int el_mutex_lock(void *hmutex);

// On sucess, returns 0.
EL_STATIC int el_mutex_unlock(void *hmutex);

EL_STATIC void el_mutex_destroy(void *hmutex);


#endif // __EL_PLAYER_LOCK_H__

