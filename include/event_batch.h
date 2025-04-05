/* include/event_batch.h */
#ifndef EVENT_BATCH_H
#define EVENT_BATCH_H

#include "uv.h"
#include "../src/win/internal.h"

#ifdef __cplusplus 
extern "C" {
#endif

/* Configuration constants */
#define UV_BATCH_MAX_SIZE       1000
#define UV_BATCH_TIMEOUT_MS     100
#define UV_BATCH_MAX_EVENT_TYPES 16
#define UV_BATCH_EVENT_VALID  0x01  /* Event contains valid data */
#define UV_BATCH_AUTO_PROCESS      0x01  /* Automatically process when full */
#define UV_BATCH_THRESHOLD_PROCESS 0x02  /* Process when threshold is reached */


/* Configuration for the event batching system */
typedef struct uv_batch_config_s {
  unsigned int batch_size;  /* Maximum number of events to batch together */
  unsigned int timeout_ms;  /* Maximum time to wait before processing a batch */
  unsigned int flags;       /* Configuration flags */
} uv_batch_config_t;

/* Default configuration values */
#define UV_BATCH_DEFAULT_SIZE    64
#define UV_BATCH_DEFAULT_TIMEOUT 10
#define UV_BATCH_DEFAULT_FLAGS   0
#define UV_BATCH_MAX_EVENT_SIZE  100

/* Flag definitions */
#define UV_BATCH_AUTO_PROCESS  0x01  /* Automatically process batches when full */

/* IOCP event structure for batching */
typedef struct uv_batch_iocp_event_s {
  DWORD bytes;             /* Transferred bytes */
  ULONG_PTR key;           /* Completion key */
  OVERLAPPED* overlapped;  /* Overlapped structure */
} uv_batch_iocp_event_t;


/* Event structure */
struct uv_batch_event_s {
  uv_batch_event_type_t type;
  uv_batch_priority_t priority;
  uv_batch_status_t status;
  // void* data;
  size_t data_size;
  uint64_t timestamp;
  void (*callback)(void* result);
  struct uv_batch_event_s* next;
  void* data;
  // unsigned char data[UV_BATCH_MAX_EVENT_SIZE];  /* Storage for event data */
  size_t size;                                  /* Actual size of event data */
  unsigned int flags;                           /* Event flags */
};

/* Batch structure - Windows specific */
struct uv_batch_s {
  uv_loop_t* loop;
  uv_batch_event_t* queue_head;
  uv_batch_event_t* queue_tail;
  size_t current_size;
  uv_mutex_t mutex;
  uv_batch_stats_t stats;
  int initialized;
  int is_processing;
  void (*process_batch_cb)(uv_batch_event_t*, size_t);
  void (*error_cb)(uv_batch_event_t*, int);
  HANDLE event_handle;  /* Windows-specific field */
  unsigned int size;                  /* Current number of events in batch */
  uv_batch_iocp_event_t* iocp_events; /* Array of batched IOCP events */
  uv_timer_t timeout_timer;           /* Timer for batch processing */
  unsigned int timeout_ms;            /* Timeout in milliseconds */
  unsigned int capacity;               /* Maximum number of events in batch */
  unsigned int event_size;             /* Maximum size of each event */
  unsigned int process_threshold;      /* Threshold for early processing */
  uv_batch_event_t* events;            /* Array of batched events */
  unsigned int flags;                  /* Batch flags */
};

typedef void (*uv_batch_callback_t)(void *result);

/* Internal functions */
void uv__batch_process(uv_batch_t* batch);
int uv__batch_windows_init(uv_loop_t* loop, uv_batch_t* batch);
void uv__batch_windows_cleanup(uv_batch_t* batch);
uint64_t uv__batch_hrtime(void);
void uv__batch_update_stats(uv_batch_t* batch, size_t processed_count);
int uv__batch_validate_config(uv_batch_t* batch);
void uv__batch_free_events(uv_batch_event_t* head);
void uv__batch_sort_by_priority(uv_batch_event_t** head);
int uv__batch_process_pending(uv_loop_t* loop);
int uv__batch_add_iocp_event(uv_loop_t* loop, DWORD bytes, ULONG_PTR key, OVERLAPPED* overlapped);
uint64_t uv__batch_adjust_timeout(uv_loop_t *loop, uint64_t timeout);
int uv__batch_add_event_internal(uv_loop_t *loop, uv_batch_event_type_t type, uv_batch_priority_t priority, void *event, size_t event_size, uv_batch_callback_t callback);
// int uv__batch_add_event_internal(uv_loop_t* loop, void* event, size_t event_size);
// void uv__batch_process_internal(uv_loop_t* loop);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BATCH_H */