/**
 * src/unix/event_batch_unix.c
 * Unix-specific implementation for event batching
 */

 #ifndef _WIN32

 #include <pthread.h>
 #include "uv.h"
 #include "event_batch.h"
 #include "uv-event-batch-internal.h"
 #include "internal.h"
 
 /* Initialize Unix-specific resources */
 int uv__batch_unix_init(uv_loop_t* loop, uv_batch_t* batch) {
     int ret;
     
     /* Unix implementation primarily relies on the mutex 
        which is already initialized in the common code */
     
     /* Set up eventfd for signaling if available */
 #ifdef __linux__
     batch->eventfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
     if (batch->eventfd >= 0) {
         ret = uv__io_init(&batch->event_watcher, 
                          uv__batch_unix_io_cb,
                          batch->eventfd);
         if (ret) {
             close(batch->eventfd);
             return ret;
         }
         uv__io_start(loop, &batch->event_watcher, POLLIN);
     }
 #endif
 
     return 0;
 }
 
 /* Clean up Unix-specific resources */
 void uv__batch_unix_cleanup(uv_batch_t* batch) {
 #ifdef __linux__
     if (batch->eventfd >= 0) {
         uv__io_stop(batch->loop, &batch->event_watcher, POLLIN);
         uv__io_close(batch->loop, &batch->event_watcher);
         close(batch->eventfd);
         batch->eventfd = -1;
     }
 #endif
 }
 
 #ifdef __linux__
 /* IO callback for eventfd notifications */
 static void uv__batch_unix_io_cb(uv_loop_t* loop, 
                                 uv__io_t* w,
                                 unsigned int events) {
     uv_batch_t* batch;
     uint64_t val;
     
     batch = container_of(w, uv_batch_t, event_watcher);
     
     if (read(batch->eventfd, &val, sizeof(val)) == sizeof(val)) {
         uv__batch_process(batch);
     }
 }
 #endif
 
 /* Signal batch processing on Unix */
 static void uv__batch_unix_signal(uv_batch_t* batch) {
 #ifdef __linux__
     uint64_t val = 1;
     if (batch->eventfd >= 0) {
         write(batch->eventfd, &val, sizeof(val));
     }
 #endif
 }
 
 /* Map platform init function */
 int uv__batch_platform_init(uv_loop_t* loop, uv_batch_t* batch) {
     return uv__batch_unix_init(loop, batch);
 }
 
 /* Map platform cleanup function */
 void uv__batch_platform_cleanup(uv_batch_t* batch) {
     uv__batch_unix_cleanup(batch);
 }
 
 #endif /* !_WIN32 */