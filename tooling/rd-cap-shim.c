// FIFO-triggered RenderDoc frame capture shim (LD_PRELOAD alongside
// librenderdoc.so). Write anything to /tmp/rd-cap to toggle capture:
// first line starts, second line ends. Capture lands in CWD (*.rdc).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
 #include <pthread.h>
 #include <stdio.h>
 #include <string.h>
 #include <unistd.h>
 #include <stdint.h>
 #include <sys/stat.h>
 #include "renderdoc_app.h"

static void* cap_thread(void* arg) {
  (void)arg;
  // Wait for librenderdoc (preloaded after us or before us - either way
  // RTLD_DEFAULT finds it once loaded).
  pRENDERDOC_GetAPI get_api = NULL;
  for (int i = 0; i < 600 && !get_api; i++) {
    get_api = (pRENDERDOC_GetAPI)dlsym(RTLD_DEFAULT, "RENDERDOC_GetAPI");
    if (!get_api) usleep(100000);
  }
  if (!get_api) {
    fprintf(stderr, "[rdshim] RENDERDOC_GetAPI not found\n");
    return NULL;
  }
  RENDERDOC_API_1_0_0* api = NULL;
  if (!get_api(eRENDERDOC_API_Version_1_0_0, (void**)&api) || !api) {
    fprintf(stderr, "[rdshim] GetAPI(1.0.0) failed\n");
    return NULL;
  }
  fprintf(stderr, "[rdshim] ready, waiting on /tmp/rd-cap\n");
  unlink("/tmp/rd-cap");
  if (mkfifo("/tmp/rd-cap", 0600) != 0) {
    fprintf(stderr, "[rdshim] mkfifo failed\n");
    return NULL;
  }
  int capturing = 0;
  char buf[64];
  for (;;) {
    int fd = open("/tmp/rd-cap", O_RDONLY);
    if (fd < 0) return NULL;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) continue;
    if (!capturing) {
      api->StartFrameCapture(NULL, NULL);
      fprintf(stderr, "[rdshim] capture STARTED\n");
      capturing = 1;
    } else {
      uint32_t ok = api->EndFrameCapture(NULL, NULL);
      fprintf(stderr, "[rdshim] capture ENDED ok=%u\n", ok);
      capturing = 0;
    }
  }
  return NULL;
}

__attribute__((constructor)) static void rdshim_init(void) {
  pthread_t t;
  pthread_create(&t, NULL, cap_thread, NULL);
  pthread_detach(t);
}
