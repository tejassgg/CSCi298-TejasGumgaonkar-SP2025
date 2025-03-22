/* src/event-batch.c */
#define UV_HANDLE_INTERVAL 0x0001
#include <stdlib.h>
#include <string.h>

#include "uv.h"
#include "event_batch.h"
#include "uv-common.h"

/* Timer callback for batch processing */
static void uv__batch_timer_cb(uv_timer_t *handle)
{
  uv_batch_t *batch = (uv_batch_t *)handle->data;

  uv_mutex_lock(&batch->mutex);

  if (batch->current_size > 0 && !batch->is_processing)
  {
    uv__batch_process(batch);
  }

  uv_mutex_unlock(&batch->mutex);
}

/* Callback for the batch timeout timer */
static void uv__batch_timeout_cb(uv_timer_t *handle)
{
  uv_loop_t *loop = handle->loop;

  /* Process pending batched events if there are any */
  if (loop->batch_pending > 0)
  {
    uv__batch_process_pending(loop);
  }
}


int uv_batch_init_ex(uv_loop_t *loop, const uv_batch_config_t *config)
{
  int err;
  uv_batch_t *batch_system;

  if (loop == NULL || config == NULL)
    return UV_EINVAL;

  /* Check if batch system is already initialized */
  if (loop->batch_system != NULL)
    return UV_EALREADY;

  /* Allocate batch system structure */
  batch_system->iocp_events = (uv_batch_iocp_event_t *)
      uv__malloc(sizeof(uv_batch_iocp_event_t) * config->batch_size);

  if (batch_system->iocp_events == NULL)
  {
    uv__free(batch_system);
    return UV_ENOMEM;
  }

  /* Initialize batch system fields */
  batch_system->size = 0;
  batch_system->capacity = config->batch_size;
  batch_system->timeout_ms = config->timeout_ms;
  batch_system->flags = config->flags;

  /* Allocate event array */
  batch_system->iocp_events = (uv_batch_iocp_event_t *)
      uv__malloc(sizeof(uv_batch_iocp_event_t) * config->batch_size);

  if (batch_system->iocp_events == NULL)
  {
    uv__free(batch_system);
    return UV_ENOMEM;
  }

  /* Initialize timeout timer */
  err = uv_timer_init(loop, &batch_system->timeout_timer);
  if (err)
  {
    uv__free(batch_system->iocp_events);
    uv__free(batch_system);
    return err;
  }

  /* Set timer as internal handle so it doesn't prevent loop from exiting */
  batch_system->timeout_timer.flags |= UV_HANDLE_INTERVAL;

  /* Store batch system in the loop */
  loop->batch_system = batch_system;
  loop->batch_enabled = 0;
  loop->batch_pending = 0;

  return 0;
}

int uv_batch_init(uv_loop_t *loop)
{
  uv_batch_config_t default_config = {
      .batch_size = UV_BATCH_DEFAULT_SIZE,
      .timeout_ms = UV_BATCH_DEFAULT_TIMEOUT,
      .flags = UV_BATCH_DEFAULT_FLAGS};

  return uv_batch_init_ex(loop, &default_config);
}

void uv_batch_enable(uv_loop_t *loop)
{
  if (loop == NULL || loop->batch_system == NULL)
    return;

  loop->batch_enabled = 1;
}

void uv_batch_disable(uv_loop_t *loop)
{
  if (loop == NULL || loop->batch_system == NULL)
    return;

  /* Process any pending batched events */
  if (loop->batch_pending > 0)
  {
    uv__batch_process_pending(loop);
  }

  /* Stop the timeout timer */
  uv_timer_stop(&loop->batch_system->timeout_timer);

  loop->batch_enabled = 0;
}

void uv_batch_cleanup(uv_loop_t *loop)
{
  uv_batch_t *batch_system;

  if (loop == NULL || loop->batch_system == NULL)
    return;

  batch_system = (uv_batch_t *)loop->batch_system;

  /* Process any pending batched events */
  if (loop->batch_pending > 0)
  {
    uv__batch_process_pending(loop);
  }

  /* Stop and close the timeout timer */
  uv_timer_stop(&batch_system->timeout_timer);
  uv_close((uv_handle_t *)&batch_system->timeout_timer, NULL);

  /* Free resources */
  uv__free(batch_system->iocp_events);
  uv__free(batch_system);

  loop->batch_system = NULL;
  loop->batch_enabled = 0;
  loop->batch_pending = 0;
}

int uv__batch_add_iocp_event(uv_loop_t *loop, DWORD bytes, ULONG_PTR key, OVERLAPPED *overlapped)
{
  uv_batch_t *batch_system;
  unsigned int index;

  if (loop == NULL || loop->batch_system == NULL || !loop->batch_enabled)
    return -1; /* Pass through to normal processing */

  batch_system = (uv_batch_t *)loop->batch_system;
  batch_system->iocp_events[index].bytes = bytes;
  batch_system->iocp_events[index].key = key;
  batch_system->iocp_events[index].overlapped = overlapped;

  /* Check if batch is full */
  if (loop->batch_pending >= batch_system->capacity)
  {
    /* Process the batch if auto-processing is enabled */
    if (batch_system->flags & UV_BATCH_AUTO_PROCESS)
    {
      uv__batch_process_pending(loop);
    }
    else
    {
      return -1; /* Batch is full, pass through to normal processing */
    }
  }

  /* Add event to batch */
  index = loop->batch_pending;
  batch_system->iocp_events[index].bytes = bytes;
  batch_system->iocp_events[index].key = key;
  batch_system->iocp_events[index].overlapped = overlapped;

  /* Increment pending event count */
  loop->batch_pending++;

  /* Start timeout timer if this is the first event in batch */
  if (loop->batch_pending == 1)
  {
    uv_timer_start(&batch_system->timeout_timer,
                   uv__batch_timeout_cb,
                   batch_system->timeout_ms,
                   0);
  }

  return 0; /* Successfully batched */
}

// unsigned int uv__batch_adjust_timeout(uv_loop_t *loop, unsigned int timeout)
// {
//   uv_batch_t *batch_system;

//   if (loop == NULL || loop->batch_system == NULL || !loop->batch_enabled || loop->batch_pending == 0)
//     return timeout;

//   batch_system = (uv_batch_t *)loop->batch_system;

//   /* If we have pending events, don't wait longer than the batch timeout */
//   if (timeout > batch_system->timeout_ms || timeout == INFINITE)
//     return batch_system->timeout_ms;

//   return timeout;
// }

uint64_t uv__batch_adjust_timeout(uv_loop_t *loop, uint64_t timeout) {
  uv_batch_t* batch_system;
  
  /* If batching isn't enabled or there are no pending events, don't modify timeout */
  if (loop == NULL || loop->batch_system == NULL || !loop->batch_enabled || loop->batch_pending == 0)
      return timeout;
  
  batch_system = (uv_batch_t*)loop->batch_system;
  
  /* 
   * If the current timeout is longer than our batch timeout, reduce it
   * to ensure batched events get processed reasonably soon.
   * 
   * This is important because we don't want events sitting in the batch
   * for too long just because the event loop is waiting for a longer timeout.
   */
  if (timeout > batch_system->timeout_ms || timeout == INFINITE)
      return batch_system->timeout_ms;
  
  /* 
   * If the current timeout is already shorter than our batch timeout,
   * we keep the shorter value to maintain responsiveness.
   */
  return timeout;
}

/* Process a batch of events */
void uv__batch_process(uv_batch_t *batch)
{
  uv_batch_event_t *current, *next;
  size_t processed = 0;
  uint64_t start_time;

  if (!batch || batch->is_processing || batch->current_size == 0)
  {
    return;
  }

  batch->is_processing = 1;
  // start_time = uv__batch_hrtime();

  /* Sort events by priority */
  uv__batch_sort_by_priority(&batch->queue_head);

  /* Process all events */
  if (batch->process_batch_cb)
  {
    batch->process_batch_cb(batch->queue_head, batch->current_size);
  }

  /* Update event status and call callbacks */
  current = batch->queue_head;
  while (current != NULL)
  {
    next = current->next;

    current->status = UV_BATCH_STATUS_COMPLETED;
    if (current->callback)
    {
      current->callback(current->data);
    }

    processed++;
    current = next;
  }

  /* Update statistics */
  // uv__batch_update_stats(batch, processed);
  // batch->stats.total_processing_time += uv__batch_hrtime() - start_time;

  /* Clean up processed events */
  uv__batch_free_events(batch->queue_head);
  batch->queue_head = NULL;
  batch->queue_tail = NULL;
  batch->current_size = 0;
  batch->loop->batch_pending -= processed;
  batch->is_processing = 0;
}

/* Signal batch processing on Windows */
void uv__batch_windows_signal(uv_batch_t* batch) {
  if (batch && batch->event_handle) {
    SetEvent(batch->event_handle);
  }
}

void uv__batch_schedule_processing(uv_loop_t *loop, int immediate) {
  uv_batch_t *batch_system;

  if (loop == NULL || loop->batch_system == NULL)
      return;

  batch_system = (uv_batch_t *)loop->batch_system;

  if (immediate) {
      uv__batch_process_pending(loop);
  } else {
      // Schedule processing for the next iteration of the event loop
      uv_timer_start(&batch_system->timeout_timer, uv__batch_timeout_cb, batch_system->timeout_ms, 0);
  }
}

/* 
 * Adds an event to the batch system's internal storage.
 * This is the core internal function that handles the actual event storage
 * and is called by platform-specific event addition functions.
 *
 * Parameters:
 *   loop       - The event loop that owns the batch system
 *   event      - Pointer to the event data to be batched
 *   event_size - Size of the event data in bytes
 *
 * Returns:
 *   0 on success (event added to batch)
 *   -1 on failure (batch full, batching disabled, etc.)
 */
int uv__batch_add_event_internal(uv_loop_t* loop, void* event, size_t event_size) {
  uv_batch_t* batch_system;
  char* storage_ptr;
  uv_batch_event_t* batch_event;
  
  /* Validate parameters and check if batching is enabled */
  if (loop == NULL || loop->batch_system == NULL || 
      !loop->batch_enabled || event == NULL || event_size == 0) {
    return -1;
  }
  
  batch_system = (uv_batch_t*)loop->batch_system;
  
  /* Check if batch is at capacity */
  if (loop->batch_pending >= batch_system->capacity) {
    /* If auto-process is enabled, process the current batch first */
    if (batch_system->flags & UV_BATCH_AUTO_PROCESS) {
      uv__batch_process_pending(loop);
    } else {
      /* Batch is full and auto-process is disabled */
      return -1;
    }
  }
  
  /* Get pointer to the next available event storage slot */
  batch_event = &batch_system->events[loop->batch_pending];
  
  /* Check if the event will fit in our storage */
  if (event_size > batch_system->event_size) {
    /* Event is too large for pre-allocated storage */
    return -1;
  }
  
  /* Copy the event data into our storage */
  memcpy(&batch_event->data, event, event_size);
  batch_event->size = event_size;
  
  /* Mark the event as valid */
  batch_event->flags = UV_BATCH_EVENT_VALID;
  
  /* Increment pending event count */
  loop->batch_pending++;
  
  /* If this is the first event in the batch, start the timeout timer */
  if (loop->batch_pending == 1 && batch_system->timeout_ms > 0) {
    uv_timer_start(&batch_system->timeout_timer, 
                   uv__batch_timeout_cb, 
                   batch_system->timeout_ms, 
                   0);
  }
  
  /* If we've reached capacity or threshold, schedule processing */
  if (loop->batch_pending >= batch_system->capacity ||
      (batch_system->flags & UV_BATCH_THRESHOLD_PROCESS && 
       loop->batch_pending >= batch_system->process_threshold)) {
    
    /* Signal the event loop that processing is needed */
    if (loop->batch_pending >= batch_system->capacity) {
      /* For full batches, we might want immediate processing */
      uv__batch_schedule_processing(loop, 1); /* 1 = immediate */
    } else {
      /* For threshold-based triggers, regular scheduling is fine */
      uv__batch_schedule_processing(loop, 0); /* 0 = regular */
    }
  }
  
  return 0;
}

/* Process any pending batches - called during event loop */
int uv__batch_process_pending(uv_loop_t *loop)
{
  uv_batch_t *batch;
  int processed = 0;

  if (!loop || !loop->batch_enabled || !loop->batch_system ||
      loop->batch_pending == 0)
  {
    return 0;
  }

  batch = loop->batch_system;

  uv_mutex_lock(&batch->mutex);
  if (batch->current_size > 0 && !batch->is_processing)
  {
    uv__batch_process(batch);
    processed = 1;
  }
  uv_mutex_unlock(&batch->mutex);

  return processed;
}

/* Utilities and other required functions... */