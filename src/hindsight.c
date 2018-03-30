/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** @brief Hindsight main executable @file */

#include <errno.h>
#include <luasandbox/lauxlib.h>
#ifdef _WIN32
#include "win_pthread.h"
#include "win_sem.h"
#else
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/inotify.h>
#endif
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "hs_analysis_plugins.h"
#include "hs_checkpoint_writer.h"
#include "hs_config.h"
#include "hs_input.h"
#include "hs_input_plugins.h"
#include "hs_logger.h"
#include "hs_output_plugins.h"


static const char g_module[] = "hindsight";

#ifdef _WIN32
static HANDLE g_iocp;

#define IOCP_WEVENT_LOAD_PATH_INPUT    1
#define IOCP_WEVENT_LOAD_PATH_ANALYSIS 2
#define IOCP_WEVENT_LOAD_PATH_OUTPUT   3
#define IOCP_WEVENT_SHUTDOWN           4

typedef struct _watcher {
  OVERLAPPED             overlapped;
  int                    eventId;
} watcher_t;

#define MAX_BUFF_SIZE  100
typedef struct _fsevent {
  watcher_t               w;
  FILE_NOTIFY_INFORMATION data[MAX_BUFF_SIZE];
  DWORD                   notifyFilters;
  DWORD                   bytesReturned;
  HANDLE                  handle;
} fsevent_t;

typedef struct _shutdownEvent {
	watcher_t  w;
} shutdownEvent_t;

bool createWatchFile(HANDLE iocp, const char* file, fsevent_t *fileHandle) {
  memset(fileHandle, 0, sizeof(fsevent_t));

  fileHandle->handle = CreateFileA(file, GENERIC_READ | FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
    NULL);
  if (fileHandle->handle == INVALID_HANDLE_VALUE || fileHandle->handle == NULL) {
    return false;
  }
  if (CreateIoCompletionPort(fileHandle->handle, iocp, (ULONG_PTR)fileHandle->handle, 1) == NULL) {
    DWORD errCode = GetLastError();
    CloseHandle(fileHandle->handle);
    SetLastError(errCode);
    return false;
  }

  fileHandle->notifyFilters = FILE_NOTIFY_CHANGE_FILE_NAME |
    FILE_NOTIFY_CHANGE_DIR_NAME |
    FILE_NOTIFY_CHANGE_SIZE |
    FILE_NOTIFY_CHANGE_LAST_WRITE |
	FILE_NOTIFY_CHANGE_CREATION;
  return true;
}

void closeWatch(fsevent_t *fileHandle) {
	CloseHandle(fileHandle->handle);
}

bool readFs(fsevent_t *evt) {
  evt->bytesReturned = 0;
  memset(&evt->w.overlapped, 0, sizeof(OVERLAPPED));

  return ReadDirectoryChangesW(evt->handle,
    evt->data,
    MAX_BUFF_SIZE * sizeof(FILE_NOTIFY_INFORMATION),
    TRUE,
    evt->notifyFilters,
    &evt->bytesReturned,
    &evt->w.overlapped,
    NULL);
}

BOOL CtrlHandler(DWORD fdwCtrlType) {
	shutdownEvent_t *evt;
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
		evt = (shutdownEvent_t*)calloc(1, sizeof(shutdownEvent_t));
		evt->w.eventId = IOCP_WEVENT_SHUTDOWN;
		PostQueuedCompletionStatus(g_iocp, 1, 0, (LPOVERLAPPED)evt);
		return TRUE;
	case CTRL_CLOSE_EVENT:
		evt = (shutdownEvent_t*)calloc(1, sizeof(shutdownEvent_t));
		evt->w.eventId = IOCP_WEVENT_SHUTDOWN;
		PostQueuedCompletionStatus(g_iocp, 1, 0, (LPOVERLAPPED)evt);
		return TRUE;
	case CTRL_BREAK_EVENT:
		evt = (shutdownEvent_t*)calloc(1, sizeof(shutdownEvent_t));
		evt->w.eventId = IOCP_WEVENT_SHUTDOWN;
		PostQueuedCompletionStatus(g_iocp, 1, 0, (LPOVERLAPPED)evt);
		return TRUE;
	case CTRL_SHUTDOWN_EVENT:
		evt = (shutdownEvent_t*)calloc(1, sizeof(shutdownEvent_t));
		evt->w.eventId = IOCP_WEVENT_SHUTDOWN;
		PostQueuedCompletionStatus(g_iocp, 1, 0, (LPOVERLAPPED)evt);
		return TRUE;
	default:
		return FALSE;
	}
}
#else
static sem_t g_shutdown;

void* sig_handler(void *arg)
{
  (void)arg;
  int sig;
  sigset_t signal_set;
  sigemptyset(&signal_set);
  sigaddset(&signal_set, SIGINT);
  sigaddset(&signal_set, SIGTERM);

  for (;;) {
    sigwait(&signal_set, &sig);
    if (sigismember(&signal_set, SIGHUP)) {
      if (sig == SIGHUP) {
        break;
      }
      hs_log(NULL, g_module, 6, "forced stop signal received");
      exit(EXIT_FAILURE);
    }
    if (sig == SIGINT || sig == SIGTERM) {
      hs_log(NULL, g_module, 6, "stop signal received");
      sem_post(&g_shutdown);
#ifdef HINDSIGHT_CLI
      sigaddset(&signal_set, SIGINT);
      sigaddset(&signal_set, SIGTERM);
      sigaddset(&signal_set, SIGHUP);
      continue;
#endif
      break;
    } else {
      hs_log(NULL, g_module, 6, "unexpected signal received %d", sig);
    }
  }
  return (void *)0;
}

#endif


int main(int argc, char *argv[])
{
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s <cfg> [loglevel]\n", argv[0]);
    return EXIT_FAILURE;
  }

#ifdef _WIN32
  if (!(g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1))) {
    printf("ERROR: create completionPort failed, %s.\n", w32_error(GetLastError()));
    return EXIT_FAILURE;
  }
#else
  if (sem_init(&g_shutdown, 0, 1)) {
    perror("g_shutdown sem_init failed");
    return EXIT_FAILURE;
  }

  if (sem_wait(&g_shutdown)) {
    perror("g_shutdown sem_wait failed");
    return EXIT_FAILURE;
  }
#endif

  int loglevel = 6;
  if (argc == 3) {
    loglevel = atoi(argv[2]);
    if (loglevel < 0 || loglevel > 7) {
      loglevel = 6;
    }
  }
  hs_init_log(loglevel);

  hs_config cfg;
  if (hs_load_config(argv[1], &cfg)) {
    return EXIT_FAILURE;
  }
#ifdef _WIN32
  fsevent_t dirHandles[3];
  if (cfg.load_path[0] != 0) {
    if (!createWatchFile(g_iocp, cfg.load_path_input, &dirHandles[0])) {
      printf("ERROR: watch '%s' failed, %s.\n", cfg.load_path_input, w32_error(GetLastError()));
      return EXIT_FAILURE;
    }
    dirHandles[0].w.eventId = IOCP_WEVENT_LOAD_PATH_INPUT;

    if (!createWatchFile(g_iocp, cfg.load_path_analysis, &dirHandles[1])) {
      printf("ERROR: watch '%s' failed, %s.\n", cfg.load_path_analysis, w32_error(GetLastError()));
      return EXIT_FAILURE;
    }
    dirHandles[1].w.eventId = IOCP_WEVENT_LOAD_PATH_ANALYSIS;

    if (!createWatchFile(g_iocp, cfg.load_path_output, &dirHandles[2])) {
      printf("ERROR: watch '%s' failed, %s.\n", cfg.load_path_output, w32_error(GetLastError()));
      return EXIT_FAILURE;
    }
    dirHandles[2].w.eventId = IOCP_WEVENT_LOAD_PATH_OUTPUT;
  }
#else
  int watch[3] = {0};
  int load = 0;
  if (cfg.load_path[0] != 0) {
    bool err = false;
    load = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (load < 0) {
      hs_free_config(&cfg);
      perror("inotify_init failed");
      return EXIT_FAILURE;
    }
    watch[0] = inotify_add_watch(load, cfg.load_path_input,
                                 IN_CLOSE_WRITE | IN_MOVED_TO);
    if (watch[0] < 0) {
      hs_log(NULL, g_module, 0, "%s: directory does not exist",
             cfg.load_path_input);
      err = true;
    }

    watch[1] = inotify_add_watch(load, cfg.load_path_analysis,
                                 IN_CLOSE_WRITE | IN_MOVED_TO);
    if (watch[1] < 0) {
      hs_log(NULL, g_module, 0, "%s: directory does not exist",
             cfg.load_path_analysis);
      err = true;
    }

    watch[2] = inotify_add_watch(load, cfg.load_path_output,
                                 IN_CLOSE_WRITE | IN_MOVED_TO);
    if (watch[2] < 0) {
      hs_log(NULL, g_module, 0, "%s: directory does not exist",
             cfg.load_path_output);
      err = true;
    }

    if (err) {
      hs_free_config(&cfg);
      return EXIT_FAILURE;
    }
  }
#endif
  hs_checkpoint_reader cpr;
  hs_init_checkpoint_reader(&cpr, cfg.output_path);
  hs_cleanup_checkpoints(&cpr, cfg.run_path, cfg.analysis_threads);

  hs_log(NULL, g_module, 6, "starting");

#ifdef _WIN32
  if(!SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, TRUE)) {
    printf("ERROR: watch signal failed, %s.\n", w32_error(GetLastError()));
    return EXIT_FAILURE;
  } 
#else
  sigset_t signal_set;
  sigfillset(&signal_set);
  if (pthread_sigmask(SIG_SETMASK, &signal_set, NULL)) {
    perror("pthread_sigmask failed");
    return EXIT_FAILURE;
  }
  pthread_t sig_thread;
  if (pthread_create(&sig_thread, NULL, sig_handler, NULL)) {
    hs_log(NULL, g_module, 1, "signal handler could not be setup");
    return EXIT_FAILURE;
  }
#endif

  hs_input_plugins ips;
  hs_init_input_plugins(&ips, &cfg, &cpr);
  hs_load_input_startup(&ips);

  hs_analysis_plugins aps;
  hs_init_analysis_plugins(&aps, &cfg, &cpr);
  hs_load_analysis_startup(&aps);
  hs_start_analysis_threads(&aps);

  hs_output_plugins ops;
  hs_init_output_plugins(&ops, &cfg, &cpr);
  hs_load_output_startup(&ops);

  hs_checkpoint_writer cpw;
  hs_init_checkpoint_writer(&cpw, &ips, &aps, &ops, cfg.output_path);
  
#ifdef _WIN32
  for (int i = 0; i < 3; i++) {
    if (!readFs(&dirHandles[i])) {
      printf("ERROR: read fs event failed, %s.\n", w32_error(GetLastError()));
      return EXIT_FAILURE;
    }
  }

  bool                     isRunning = true;
  OVERLAPPED               *overlapped;
  DWORD                    bytesReturned;
  ULONG_PTR	               completionKey = 0;
  FILE_NOTIFY_INFORMATION *info;
  watcher_t               *w;
  fsevent_t               *e;
  shutdownEvent_t         *shutdownEvt;
  char                     fileName[MAX_PATH+1];

  while (isRunning) {
    bytesReturned = 0;
    memset(&overlapped, 0, sizeof(OVERLAPPED*));

    if (!GetQueuedCompletionStatus(g_iocp, &bytesReturned, &completionKey, &overlapped, 1000)) {
      if (GetLastError() != WAIT_TIMEOUT) {
        printf("ERROR: read fs event failed, %s.\n", w32_error(GetLastError()));
        isRunning = false;
      } else {
         hs_write_checkpoints(&cpw, &cpr);
      }
      continue;
    }
	w = (watcher_t *)overlapped;


#define hs_load_file_dynamic(call, op)                                                                        \
  for(info = (FILE_NOTIFY_INFORMATION *)(e->data);;                                                           \
	info = (FILE_NOTIFY_INFORMATION *)( ((char*) e->data) + info->NextEntryOffset)) {                                    \
      int size = WideCharToMultiByte(CP_ACP, 0, info->FileName,                                               \
        info->FileNameLength / 2, fileName, MAX_PATH, NULL, NULL);                                            \
      if (size <= 0) {                                                                                         \
        printf("ERROR: WideCharToMultiByte failed, %s.\n",  w32_error(GetLastError()));                       \
        isRunning = false;                                                                                    \
        break;                                                                                                \
      }                                                                                                       \
      fileName[size] = '\0';                                                                                  \
                                                                                                              \
      char * slash = fileName;                                                                                \
      while ((slash = strchr(slash, '\\')) != NULL) {                                                         \
        *slash = '/';                                                                                         \
      }                                                                                                       \
                                                                                                              \
      call(op, fileName);                                                                                     \
      if(info->NextEntryOffset == 0) {                                                                        \
         break;                                                                                               \
      }                                                                                                       \
    }

    switch (w->eventId)  {
    case IOCP_WEVENT_LOAD_PATH_INPUT:
      e = (fsevent_t*)w;
      hs_load_file_dynamic(hs_load_input_dynamic, &ips);

      if (!readFs(&dirHandles[0])) {
        printf("ERROR: read fs '%s' failed, %s.\n", cfg.load_path_input, w32_error(GetLastError()));
        isRunning = false;
      }
      break;
    case IOCP_WEVENT_LOAD_PATH_ANALYSIS:
      e = (fsevent_t*)w;
      hs_load_file_dynamic(hs_load_analysis_dynamic, &aps);

      if (!readFs(&dirHandles[1])) {
        printf("ERROR: read fs '%s' failed, %s.\n", cfg.load_path_analysis, w32_error(GetLastError()));
        isRunning = false;
      }
      break;
    case IOCP_WEVENT_LOAD_PATH_OUTPUT:
      e = (fsevent_t*)w;
      hs_load_file_dynamic(hs_load_output_dynamic, &ops);

      if (!readFs(&dirHandles[2])) {
        printf("ERROR: read fs '%s' failed, %s.\n", cfg.load_path_output, w32_error(GetLastError()));
        isRunning = false;
      }
      break;
    case IOCP_WEVENT_SHUTDOWN:
	  shutdownEvt = (shutdownEvent_t*)w;
	  free(shutdownEvt);
      isRunning = false;
      break;
    default:
      printf("ERROR: Unhandled eventId.\n");
      isRunning = false;
      break;
    }
  }
#else
  struct timespec ts;
  const struct inotify_event *event;
  char inotify_buf[sizeof(struct inotify_event) + FILENAME_MAX + 1]
      __attribute__((aligned(__alignof__(struct inotify_event))));

  while (true) {
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
      hs_log(NULL, g_module, 3, "clock_gettime failed");
      ts.tv_sec = time(NULL);
      ts.tv_nsec = 0;
    }
    ts.tv_sec += 1;
    if (!sem_timedwait(&g_shutdown, &ts)) {
      sem_post(&g_shutdown);
      break; // shutting down
    }
    hs_write_checkpoints(&cpw, &cpr);

    if (load) {
      for (;;){
        ssize_t len = read(load, inotify_buf, sizeof(inotify_buf));
        if (len == -1 && errno != EAGAIN) {
          hs_log(NULL, g_module, 1, "inotify read failure");
          sem_post(&g_shutdown);
        }
        if (len <= 0) break;

        for (char *ptr = inotify_buf; ptr < inotify_buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
          event = (const struct inotify_event *)ptr;
          if (event->len) {
            if (watch[1] == event->wd) {
              hs_load_analysis_dynamic(&aps, event->name);
            } else if (watch[0] == event->wd) {
              hs_load_input_dynamic(&ips, event->name);
            } else if (watch[2] == event->wd) {
              hs_load_output_dynamic(&ops, event->name);
            }
          }
        }
      }
    }

#ifdef HINDSIGHT_CLI
    if (ips.list_cnt == 0) {
      hs_log(NULL, g_module, 6, "input plugins have exited; "
             "cascading shutdown initiated");
      pthread_kill(sig_thread, SIGINT); // when all the inputs are done, exit
    }
#endif
  }

  if (load) {
    for (int i = 0; i < 3; ++i) {
      inotify_rm_watch(load, watch[i]);
    }
    close(load);
  }
#endif
  int rv = EXIT_SUCCESS;

#ifdef HINDSIGHT_CLI
  hs_stop_input_plugins(&ips);
  hs_wait_input_plugins(&ips);
  hs_write_checkpoints(&cpw, &cpr);
  if (ips.terminated) {
    rv = 2;
  }
  hs_free_input_plugins(&ips);

  hs_stop_analysis_plugins(&aps);
  hs_wait_analysis_plugins(&aps);
  if (aps.terminated) {
    rv |= 4;
  }
  hs_write_checkpoints(&cpw, &cpr);
  hs_free_analysis_plugins(&aps);

  hs_stop_output_plugins(&ops);
  hs_wait_output_plugins(&ops);
  hs_write_checkpoints(&cpw, &cpr);
  if (ops.terminated) {
    rv |= 8;
  }
  hs_free_output_plugins(&ops);
#ifndef _WIN32
  pthread_kill(sig_thread, SIGHUP);
#endif
#else
  // non CLI mode should shut everything down immediately
  hs_stop_input_plugins(&ips);
  hs_stop_analysis_plugins(&aps);
  hs_stop_output_plugins(&ops);

  hs_wait_input_plugins(&ips);
  hs_wait_analysis_plugins(&aps);
  hs_wait_output_plugins(&ops);

  hs_write_checkpoints(&cpw, &cpr);

  hs_free_input_plugins(&ips);
  hs_free_analysis_plugins(&aps);
  hs_free_output_plugins(&ops);
#endif

  hs_free_checkpoint_writer(&cpw);
  hs_free_checkpoint_reader(&cpr);
  hs_free_config(&cfg);

#ifndef _WIN32
  pthread_join(sig_thread, NULL);
#endif
  hs_log(NULL, g_module, 6, "exiting");
  hs_free_log();

#ifdef _WIN32
  CloseHandle(g_iocp);
  closeWatch(&dirHandles[0]);
  closeWatch(&dirHandles[1]);
  closeWatch(&dirHandles[2]);
#else
  sem_destroy(&g_shutdown);
#endif

  return rv;
}
