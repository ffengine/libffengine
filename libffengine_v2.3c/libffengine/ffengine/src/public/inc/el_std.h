#ifndef EL_STD_H_
#define EL_STD_H_

#undef NULL
#define NULL   (void *)0

#define EL_CONST const
#define EL_NULL NULL

#define EL_SUCCESS	0
#define EL_FAILED	-1

typedef int EL_BOOL;
#define EL_TRUE     1
#define EL_FALSE    0

#define EL_STATIC static

#define EL_MIN(x,y)			(((x)<(y))?(x):(y))

typedef unsigned int    EL_SIZE;
typedef char            EL_CHAR;
typedef int             EL_INT;
typedef unsigned int    EL_UINT;
typedef long            EL_LONG;
typedef short           EL_SHORT;
typedef unsigned long   EL_DWORD;
typedef unsigned short  EL_WORD;
typedef unsigned char   EL_BYTE;
typedef unsigned char   EL_UTF8;
typedef unsigned short  EL_WCHAR;
//typedef float           EL_FLOAT;
//typedef double          EL_DOUBLE;
typedef void            EL_VOID;
//typedef long long       uint64_t;

#define EL_MAKEDWORD(a, b)  ((EL_DWORD)(((EL_WORD)(a)) | ((EL_DWORD)((EL_WORD)(b))) << 16))
#define EL_MAKEWORD(a, b)   ((EL_WORD)(((EL_BYTE)(a)) | ((EL_WORD)((EL_BYTE)(b))) << 8))
#define EL_LOWORD(l)        ((EL_WORD)(l))
#define EL_HIWORD(l)        ((EL_WORD)(((EL_DWORD)(l) >> 16) & 0xFFFF))
#define EL_LOBYTE(w)         ((EL_BYTE)(w))
#define EL_HIBYTE(w)         ((EL_BYTE)(((EL_WORD)(w) >> 8) & 0xFF))

typedef unsigned int EL_WPARAM;
typedef long EL_LPARAM;

#endif
