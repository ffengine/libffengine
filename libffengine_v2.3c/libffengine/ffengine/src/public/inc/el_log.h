
#ifndef _EL_LOG_H_
#define _EL_LOG_H_

#include "el_std.h"

// 通过定义EL_LOG来决定是否输出log

#undef EL_LOG
//#define EL_LOG

#ifdef EL_LOG   //EL_LOG

#define EL_LOG_SIZE_OF_EASY_WAY  2048

EL_STATIC void el_log(EL_CONST void *a_pBuf);
EL_STATIC void el_printf(char *fmt,...);

#define FUNC_DEBUG_START int64_t FUNC_START_BeginTime = el_get_sys_time_ms();
#define FUNC_DEBUG_END( var1 ) el_printf("------ %s    Msg:%d  use time: %lld        \n", __FUNCTION__, var1, el_get_sys_time_ms() - FUNC_START_BeginTime);
    
#else       //!EL_LOG
    
#define el_printf(fmt,...)
    
#define FUNC_DEBUG_START ;
#define FUNC_DEBUG_END( var1 ) ;
    
#endif  //EL_LOG
    
// 定义EL_DBG_LOG,  log输出建议使用EL_DBG_LOG
#define EL_DBG_LOG(fmt,args...)  \
do{ \
el_printf(fmt,##args); \
}while(0)

#endif
