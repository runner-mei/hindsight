#include "win_sem.h"


#ifdef _WIN32

#ifdef __cplusplus
extern "C"
{
#endif

DLL_VARIABLE int sem_init(sem_t *sem , int shared , int value)
{
	sem->sem = CreateSemaphore(NULL ,  value , 1024 , NULL) ;
	sem->value = value ;

	if(sem->sem == NULL)
		return -1 ;
	else
		return 0 ;
}

DLL_VARIABLE int sem_destroy(sem_t *sem)
{
	CloseHandle(sem->sem) ;
	return 0 ;
}

DLL_VARIABLE int sem_wait(sem_t *sem)
{
	DWORD rtn ;

	rtn = WaitForSingleObject(sem->sem , INFINITE) ;
	if(rtn == WAIT_OBJECT_0)
		return 0 ;
	else
		return -1 ;
}

DLL_VARIABLE int sem_trywait(sem_t *sem)
{
	DWORD rtn ;

	rtn = WaitForSingleObject(sem->sem , 0) ;
	if(rtn == WAIT_OBJECT_0)
		return 0 ;
	else
		return -1 ;
}

DLL_VARIABLE int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
	DWORD rtn ;

#pragma warning( disable : 4244 )
	rtn = WaitForSingleObject(sem->sem , abs_timeout->tv_sec* 1000 + abs_timeout->tv_nsec / 10^6) ;
#pragma warning( default : 4244 )
	if(rtn == WAIT_OBJECT_0)
		return 0 ;
	else
		return -1 ;
}


DLL_VARIABLE int sem_post(sem_t *sem)
{
	if(ReleaseSemaphore(sem->sem , 1 , &sem->value))
		return 0 ;
	else
		return -1 ;
}

#ifdef __cplusplus
}
#endif

#endif