#ifndef REALMCP_WIN_BRIDGE_H
#define REALMCP_WIN_BRIDGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct win_proc win_proc_t;

win_proc_t* win_proc_spawn(const char* exe_path);
int win_proc_write_line(win_proc_t* proc, const char* line, size_t len);
int win_proc_read_line(win_proc_t* proc, char** out_line, size_t* out_len);
void win_proc_close(win_proc_t* proc);

int win_http_get(
    const char* url,
    int timeout_ms,
    char** out_body,
    size_t* out_len,
    int* out_status
);

void win_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
