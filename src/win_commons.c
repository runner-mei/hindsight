#include <stdio.h>
#include <time.h>
#include "win_commons.h"


#ifdef _WIN32

#ifdef __cplusplus
extern "C"
{
#endif


LARGE_INTEGER
getFILETIMEoffset()
{
  SYSTEMTIME s;
  FILETIME f;
  LARGE_INTEGER t;

  s.wYear = 1970;
  s.wMonth = 1;
  s.wDay = 1;
  s.wHour = 0;
  s.wMinute = 0;
  s.wSecond = 0;
  s.wMilliseconds = 0;
  SystemTimeToFileTime(&s, &f);
  t.QuadPart = f.dwHighDateTime;
  t.QuadPart <<= 32;
  t.QuadPart |= f.dwLowDateTime;
  return (t);
}

int clock_gettime(int x, struct timespec *tv) {
  LARGE_INTEGER           t;
  FILETIME            f;
  double                  microseconds;
  static LARGE_INTEGER    offset;
  static double           frequencyToMicroseconds;
  static int              initialized = 0;
  static BOOL             usePerformanceCounter = 0;

  if (x != CLOCK_REALTIME) {
    return -1;
  }

  if (!initialized) {
    LARGE_INTEGER performanceFrequency;
    initialized = 1;
    usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
    if (usePerformanceCounter) {
      QueryPerformanceCounter(&offset);
      frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
    }
    else {
      offset = getFILETIMEoffset();
      frequencyToMicroseconds = 10.;
    }
  }
  if (usePerformanceCounter) QueryPerformanceCounter(&t);
  else {
    GetSystemTimeAsFileTime(&f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
  }

  t.QuadPart -= offset.QuadPart;
  microseconds = (double)t.QuadPart / frequencyToMicroseconds;
  t.QuadPart = (LONGLONG) microseconds;
  tv->tv_sec = t.QuadPart / 1000000;
  tv->tv_nsec = t.QuadPart % 1000000;
  return (0);
}



__declspec (thread) static char szMessageBuffer[1024 + 1];

const char * w32_error(DWORD errCode) {
	if (errCode > WSABASEERR) {
		HMODULE hModule = GetModuleHandle("wsock32");
		if (hModule != NULL) {
			if (FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				errCode,
				LANG_NEUTRAL,
				szMessageBuffer,
				sizeof(szMessageBuffer),
				NULL) <= 0) {
				snprintf(szMessageBuffer, 1024, "Error %ld", errCode);
			}

			FreeLibrary(hModule);
		}
	}
	else {
		if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			errCode,
			LANG_NEUTRAL,
			szMessageBuffer,
			sizeof(szMessageBuffer),
			NULL) <= 0) {
			snprintf(szMessageBuffer, 1024, "Error %ld", errCode);
		}
	}
	return szMessageBuffer;
}

#ifdef __cplusplus
};
#endif

#endif