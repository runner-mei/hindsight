
#ifndef PLATFORM_H
#define PLATFORM_H 1

#ifdef _WIN32
// 定义此宏表示将在 win2003 上运行
#ifndef _WIN32_WINNT
#define _WIN32_WINNT  0x0501
#endif
#if _MSC_VER >= 1600
#include <SDKDDKVer.h>
#include <stdbool.h>
#endif
#include <winsock2.h>
#include <windows.h>
#include <Mswsock.h>
#include <Ws2tcpip.h>


#ifndef __MINGW32__
#include <SDKDDKVer.h>
#if (NTDDI_VERSION >= NTDDI_VISTA)
# define HAS_INET_NTOP 1
#endif
#endif

#else
# define HAS_INET_NTOP 1
#endif

#if (_WIN32_WINNT >= 0x0600)
#define HAS_GETQUEUEDCOMPLETIONSTATUSEX 1
#endif

#ifdef _WIN32
#define socket_type SOCKET
#define iovec WSABUF
#else
 typedef int socket_type;
 #define closesocket  close
#endif

 #if __GNUC__
typedef long long int int64;
typedef unsigned long long int uint64;
#else
typedef __int64 int64;
typedef unsigned __int64 uint64;
#endif


#ifdef _STDBOOL
#ifndef __RPCNDR_H__
typedef int boolean;
#endif

#ifndef false
# define false ((boolean)0)
#endif
#ifndef true
# define true ((boolean)1)
#endif
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef EMPTY
#define EMPTY
#endif
#ifndef EMPTY_FUNC
#define EMPTY_FUNC()
#endif
#ifndef EMPTY_FUNC1
#define EMPTY_FUNC1(x)
#endif


#ifdef _WIN32

#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#define vsnprintf _vsnprintf
#define vscprintf _vscprintf
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strtoll _strtoi64
#define strtoull _strtoui64
#define alloca  _alloca

//#define filelength _filelength
//#define fileno _fileno
//#define getcwd _getcwd
//#define stat _stat
//#define access _access

#define CLOCK_REALTIME   0
int clock_gettime(int x, struct timespec *tv);
const char * w32_error(DWORD errCode);
#else
#define vscprintf(fmt, argList) vsnprintf(0, 0, fmt, argList)
#endif

#define my_malloc   malloc
#define my_free(x)    if(NULL != x) free(x)
#define my_realloc  realloc
#define my_calloc   calloc
#define my_strdup   strdup



/* On Windows, variables that may be in a DLL must be marked specially.  */
#ifdef _MSC_VER 
#ifdef _USRDLL
#if  HS_EXPORTS
# define DLL_VARIABLE __declspec (dllexport)
# define DLL_VARIABLE_C extern "C" __declspec (dllexport)
#else
# define DLL_VARIABLE __declspec (dllimport)
# define DLL_VARIABLE_C extern "C" __declspec (dllimport)
#endif
#else
# define DLL_VARIABLE extern
# define DLL_VARIABLE_C extern
#endif
#else
# define DLL_VARIABLE extern
# define DLL_VARIABLE_C extern
#endif

#define mu_max(x,y) (((x) > (y))?(x):(y))
#define mu_min(x,y) (((x) < (y))?(x):(y))

#endif