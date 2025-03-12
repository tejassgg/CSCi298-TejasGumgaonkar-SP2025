/**
 * src/event_batch_common.c
 * Common utilities for event batching system
 */

 #include "uv.h"
 #include "event_batch.h"
 #include "corecrt_malloc.h"
 
 /* Get high-resolution timestamp */
 uint64_t uv__batch_hrtime(void) {
     return uv_hrtime() / 1000000; /* Convert from nanoseconds to milliseconds */
 }
 
 /* Update batch statistics */
 void uv__batch_update_stats(uv_batch_t* batch, size_t processed_count) {
     batch->stats.total_events_processed += processed_count;
     
     if (processed_count > batch->stats.max_batch_size) {
         batch->stats.max_batch_size = processed_count;
     }
     
     /* Update average batch size using moving average */
     if (batch->stats.total_events_processed > 0) {
         batch->stats.avg_batch_size = 
             (batch->stats.avg_batch_size + processed_count) / 2;
     } else {
         batch->stats.avg_batch_size = processed_count;
     }
     
     /* Count events by type */
     uv_batch_event_t* current = batch->queue_head;
     while (current != NULL) {
         if (current->type < UV_BATCH_MAX_EVENT_TYPES) {
             batch->stats.events_by_type[current->type]++;
         }
         if (current->status == UV_BATCH_STATUS_FAILED) {
             batch->stats.failed_events++;
         }
         current = current->next;
     }
 }
 
 /* Utility function to validate batch configuration */
 int uv__batch_validate_config(uv_batch_t* batch) {
     if (!batch) {
         return UV_EINVAL;
     }
     
     if (!batch->initialized) {
         return UV_EINVAL;
     }
     
     if (!batch->process_batch_cb) {
         return UV_EINVAL;
     }
     
     return 0;
 }
 
 /* Helper function to free event chain */
 void uv__batch_free_events(uv_batch_event_t* head) {
     uv_batch_event_t* current = head;
     while (current != NULL) {
         uv_batch_event_t* next = current->next;
         if (current->data) {
             free(current->data);
         }
         free(current);
         current = next;
     }
 }
 
 /* Sort events by priority (bubble sort) */
 void uv__batch_sort_by_priority(uv_batch_event_t** head) {
     int swapped;
     uv_batch_event_t *ptr1;
     uv_batch_event_t *lptr = NULL;
 
     if (*head == NULL)
         return;
 
     do {
         swapped = 0;
         ptr1 = *head;
 
         while (ptr1->next != lptr) {
             if (ptr1->priority > ptr1->next->priority) {
                 /* Swap nodes */
                 void* temp_data = ptr1->data;
                 size_t temp_size = ptr1->data_size;
                 uv_batch_event_type_t temp_type = ptr1->type;
                 uv_batch_priority_t temp_priority = ptr1->priority;
                 uv_batch_status_t temp_status = ptr1->status;
                 uint64_t temp_timestamp = ptr1->timestamp;
                 void (*temp_callback)(void*) = ptr1->callback;
 
                 ptr1->data = ptr1->next->data;
                 ptr1->data_size = ptr1->next->data_size;
                 ptr1->type = ptr1->next->type;
                 ptr1->priority = ptr1->next->priority;
                 ptr1->status = ptr1->next->status;
                 ptr1->timestamp = ptr1->next->timestamp;
                 ptr1->callback = ptr1->next->callback;
 
                 ptr1->next->data = temp_data;
                 ptr1->next->data_size = temp_size;
                 ptr1->next->type = temp_type;
                 ptr1->next->priority = temp_priority;
                 ptr1->next->status = temp_status;
                 ptr1->next->timestamp = temp_timestamp;
                 ptr1->next->callback = temp_callback;
 
                 swapped = 1;
             }
             ptr1 = ptr1->next;
         }
         lptr = ptr1;
     } while (swapped);
 }
 
 /* Debug logging helper */
 void uv__batch_log(const char* format, ...) {
 #ifdef UV_BATCH_DEBUG
     va_list args;
     va_start(args, format);
     vfprintf(stderr, format, args);
     va_end(args);
 #endif
 }