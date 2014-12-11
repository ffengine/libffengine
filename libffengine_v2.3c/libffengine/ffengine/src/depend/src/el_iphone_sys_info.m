#include <sys/time.h>
#import <Foundation/Foundation.h>

#include "el_log.h"
#include "el_platform_sys_info.h"

EL_STATIC int64_t el_get_sys_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (1000 * tv.tv_sec + tv.tv_usec/1000);
}

//~ 另一种算法
// return CFAbsoluteTimeGetCurrent() * 1000;
