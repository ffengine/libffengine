
#include <stdarg.h>

#include "el_std.h"
#include "el_log.h"
#include "el_platform_sys_info.h"
#include "el_iOS_log.h"

#ifdef EL_LOG

EL_STATIC void el_log(EL_CONST void *a_pBuf)
{
    if (!a_pBuf)
        return;
    
	ios_Log( "%s\n",  a_pBuf);
}

EL_STATIC void el_printf(char *fmt,...)
{
    static EL_CHAR buf[EL_LOG_SIZE_OF_EASY_WAY];
    va_list ap; 
    if(strlen(fmt) >= (EL_LOG_SIZE_OF_EASY_WAY >> 1))
    {
        // Make the best way to avoid bound overflow
        return;
    }    

    va_start(ap,fmt); 
    vsprintf(buf, (EL_CONST EL_CHAR*)fmt , ap);
    va_end(ap); 
    buf[EL_LOG_SIZE_OF_EASY_WAY - 1] = 0;
    el_log(buf);    
}

#endif

