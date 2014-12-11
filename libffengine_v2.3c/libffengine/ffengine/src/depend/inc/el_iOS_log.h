
#include <stdio.h>

#ifdef EL_LOG

#define ios_Log(fmt,args...)  printf(fmt,##args)

#else

#define ios_Log(fmt,args...) ;

#endif
