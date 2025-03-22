/* src/win/event_batch_win.c */
#include <windows.h>
#include "uv.h"
#include "event_batch.h"
#include "event_batch.c"
#include "internal.h"
#include "stdlib.h"

/* Initialize Windows-specific batch resources */
int uv__batch_windows_init(uv_loop_t *loop, uv_batch_t *batch)
{
  HANDLE hEvent;

  /* Create event handle for signaling */
  hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (hEvent == NULL)
  {
    return UV_ENOMEM;
  }

  /* Register with IOCP */
  if (CreateIoCompletionPort((HANDLE)hEvent,
                             loop->iocp,
                             (ULONG_PTR)batch,
                             0) == NULL)
  {
    CloseHandle(hEvent);
    return UV_EINVAL;
  }

  batch->event_handle = hEvent;
  return 0;
}

/* Clean up Windows-specific resources */
void uv__batch_windows_cleanup(uv_batch_t *batch)
{
  if (batch && batch->event_handle)
  {
    CloseHandle(batch->event_handle);
    batch->event_handle = NULL;
  }
}

// Make Windows-specific functions safer
int uv__batch_add_iocp_event(uv_loop_t *loop, DWORD bytes, ULONG_PTR key, OVERLAPPED *overlapped)
{
  if (loop == NULL || loop->batch_system == NULL || !loop->batch_enabled)
    return -1; // Signal to fall back to normal processing

  uv_batch_t *batch_system = loop->batch_system;

  // Use mutex for thread safety
  uv_mutex_lock(&batch_system->mutex);

  // Check if batch is full
  if (loop->batch_pending >= batch_system->capacity)
  {
    if (batch_system->flags & UV_BATCH_AUTO_PROCESS)
    {
      // Process current batch if auto-processing enabled
      uv__batch_process_pending(loop);
    }
    else
    {
      // Batch is full, don't add
      uv_mutex_unlock(&batch_system->mutex);
      return -1;
    }
  }

  // Get existing IOCP events array or allocate if needed
  if (batch_system->iocp_events == NULL)
  {
    batch_system->iocp_events = (uv_batch_iocp_event_t *)
        uv__malloc(sizeof(uv_batch_iocp_event_t) * batch_system->capacity);

    if (batch_system->iocp_events == NULL)
    {
      uv_mutex_unlock(&batch_system->mutex);
      return -1;
    }
  }

  // Add event to batch
  unsigned int index = loop->batch_pending;
  batch_system->iocp_events[index].bytes = bytes;
  batch_system->iocp_events[index].key = key;
  batch_system->iocp_events[index].overlapped = overlapped;

  // Increment pending count
  loop->batch_pending++;

  // Start timer if this is the first event
  if (loop->batch_pending == 1)
  {
    uv_timer_start(&batch_system->timeout_timer,
                   uv__batch_timer_cb,
                   batch_system->timeout_ms,
                   0);
  }

  uv_mutex_unlock(&batch_system->mutex);
  return 0;
}

/* Platform-specific process pending check */
int uv__batch_windows_process_pending(uv_loop_t *loop)
{
  DWORD result;
  ULONG_PTR key;
  OVERLAPPED *overlapped;
  uv_batch_t *batch;

  if (!loop || !loop->batch_system)
  {
    return 0;
  }

  batch = loop->batch_system;

  /* Check if batch event is signaled */
  result = WaitForSingleObject(batch->event_handle, 0);
  if (result == WAIT_OBJECT_0)
  {
    /* Reset event */
    ResetEvent(batch->event_handle);

    /* Process batch */
    uv__batch_process(batch);
    return 1;
  }

  return 0;
}