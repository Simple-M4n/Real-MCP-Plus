#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "win_bridge.h"

struct win_proc {
    HANDLE h_process;
    HANDLE h_thread;
    HANDLE h_stdin_write;
    HANDLE h_stdout_read;
    char*  buf;
    size_t buf_len;
    size_t buf_cap;
};

win_proc_t* win_proc_spawn(const char* exe_path) {
    if (!exe_path) return NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE child_stdin_r = NULL, child_stdin_w = NULL;
    HANDLE child_stdout_r = NULL, child_stdout_w = NULL;

    if (!CreatePipe(&child_stdin_r, &child_stdin_w, &sa, 0)) return NULL;
    SetHandleInformation(child_stdin_w, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&child_stdout_r, &child_stdout_w, &sa, 0)) {
        CloseHandle(child_stdin_r);
        CloseHandle(child_stdin_w);
        return NULL;
    }
    SetHandleInformation(child_stdout_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = child_stdin_r;
    si.hStdOutput = child_stdout_w;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    size_t cmd_len = strlen(exe_path) + 4;
    char* cmd = (char*)malloc(cmd_len);
    if (!cmd) {
        CloseHandle(child_stdin_r);
        CloseHandle(child_stdin_w);
        CloseHandle(child_stdout_r);
        CloseHandle(child_stdout_w);
        return NULL;
    }
    snprintf(cmd, cmd_len, "\"%s\"", exe_path);

    BOOL ok = CreateProcessA(
        NULL,
        cmd,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    free(cmd);

    CloseHandle(child_stdin_r);
    CloseHandle(child_stdout_w);

    if (!ok) {
        CloseHandle(child_stdin_w);
        CloseHandle(child_stdout_r);
        return NULL;
    }

    win_proc_t* proc = (win_proc_t*)calloc(1, sizeof(win_proc_t));
    if (!proc) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(child_stdin_w);
        CloseHandle(child_stdout_r);
        return NULL;
    }

    proc->h_process = pi.hProcess;
    proc->h_thread = pi.hThread;
    proc->h_stdin_write = child_stdin_w;
    proc->h_stdout_read = child_stdout_r;
    proc->buf_cap = 65536;
    proc->buf = (char*)malloc(proc->buf_cap);
    proc->buf_len = 0;

    return proc;
}

int win_proc_write_line(win_proc_t* proc, const char* line, size_t len) {
    if (!proc || !proc->h_stdin_write || !line) return -1;

    size_t total = 0;
    while (total < len) {
        DWORD written = 0;
        if (!WriteFile(proc->h_stdin_write, line + total, (DWORD)(len - total), &written, NULL)) {
            return -1;
        }
        total += written;
    }
    DWORD nl_written = 0;
    if (!WriteFile(proc->h_stdin_write, "\n", 1, &nl_written, NULL) || nl_written != 1) {
        return -1;
    }
    return 0;
}

int win_proc_read_line(win_proc_t* proc, char** out_line, size_t* out_len) {
    if (!proc || !proc->h_stdout_read || !out_line || !out_len) return -1;
    *out_line = NULL;
    *out_len = 0;

    for (;;) {
        char* nl = (char*)memchr(proc->buf, '\n', proc->buf_len);
        if (nl) {
            size_t line_len = (size_t)(nl - proc->buf);
            size_t clean_len = line_len;
            if (clean_len > 0 && proc->buf[clean_len - 1] == '\r') {
                clean_len--;
            }
            char* res = (char*)malloc(clean_len + 1);
            if (!res) return -1;
            memcpy(res, proc->buf, clean_len);
            res[clean_len] = '\0';

            size_t rem = proc->buf_len - (line_len + 1);
            if (rem > 0) {
                memmove(proc->buf, nl + 1, rem);
            }
            proc->buf_len = rem;

            *out_line = res;
            *out_len = clean_len;
            return 1;
        }

        if (proc->buf_cap - proc->buf_len < 16384) {
            size_t new_cap = proc->buf_cap * 2;
            char* nb = (char*)realloc(proc->buf, new_cap);
            if (!nb) return -1;
            proc->buf = nb;
            proc->buf_cap = new_cap;
        }

        DWORD got = 0;
        BOOL ok = ReadFile(
            proc->h_stdout_read,
            proc->buf + proc->buf_len,
            (DWORD)(proc->buf_cap - proc->buf_len),
            &got,
            NULL
        );
        if (!ok || got == 0) {
            if (proc->buf_len > 0) {
                char* res = (char*)malloc(proc->buf_len + 1);
                if (!res) return -1;
                memcpy(res, proc->buf, proc->buf_len);
                res[proc->buf_len] = '\0';
                *out_line = res;
                *out_len = proc->buf_len;
                proc->buf_len = 0;
                return 1;
            }
            return 0;
        }
        proc->buf_len += got;
    }
}

void win_proc_close(win_proc_t* proc) {
    if (!proc) return;
    if (proc->h_stdin_write) CloseHandle(proc->h_stdin_write);
    if (proc->h_stdout_read) CloseHandle(proc->h_stdout_read);
    if (proc->h_process) {
        TerminateProcess(proc->h_process, 0);
        CloseHandle(proc->h_process);
    }
    if (proc->h_thread) CloseHandle(proc->h_thread);
    free(proc->buf);
    free(proc);
}

int win_http_get(
    const char* url,
    int timeout_ms,
    char** out_body,
    size_t* out_len,
    int* out_status
) {
    if (!url || !out_body || !out_len || !out_status) return -1;
    *out_body = NULL;
    *out_len = 0;
    *out_status = 0;

    HINTERNET h_session = WinHttpOpen(
        L"RealMCPPlus/1.0.1 (WinHTTP; x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!h_session) return -1;

    DWORD decomp = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(h_session, WINHTTP_OPTION_DECOMPRESSION, &decomp, sizeof(decomp));

    int wlen = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
    if (wlen <= 0) {
        WinHttpCloseHandle(h_session);
        return -1;
    }
    wchar_t* wurl = (wchar_t*)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wurl) {
        WinHttpCloseHandle(h_session);
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, wlen);

    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0};
    wchar_t path[4096] = {0};
    wchar_t extra[4096] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 4095;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 4095;

    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        free(wurl);
        WinHttpCloseHandle(h_session);
        return -1;
    }
    free(wurl);

    wchar_t full_path[8192] = {0};
    wcsncpy(full_path, path[0] ? path : L"/", 4095);
    if (extra[0]) {
        wcsncat(full_path, extra, 4095);
    }

    WinHttpSetTimeouts(h_session, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    HINTERNET h_connect = WinHttpConnect(h_session, host, uc.nPort, 0);
    if (!h_connect) {
        WinHttpCloseHandle(h_session);
        return -1;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET h_req = WinHttpOpenRequest(
        h_connect,
        L"GET",
        full_path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    if (!h_req) {
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return -1;
    }

    BOOL ok = WinHttpSendRequest(
        h_req,
        L"Accept: application/json\r\n",
        (DWORD)-1L,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );
    if (ok) ok = WinHttpReceiveResponse(h_req, NULL);
    if (!ok) {
        WinHttpCloseHandle(h_req);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return -1;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(
        h_req,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status_code,
        &status_size,
        WINHTTP_NO_HEADER_INDEX
    );
    *out_status = (int)status_code;

    size_t cap = 16384;
    size_t len = 0;
    char* body = (char*)malloc(cap);
    if (!body) {
        WinHttpCloseHandle(h_req);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return -1;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(h_req, &avail) || avail == 0) break;
        if (len + avail + 1 > cap) {
            while (len + avail + 1 > cap) cap *= 2;
            char* nb = (char*)realloc(body, cap);
            if (!nb) {
                free(body);
                WinHttpCloseHandle(h_req);
                WinHttpCloseHandle(h_connect);
                WinHttpCloseHandle(h_session);
                return -1;
            }
            body = nb;
        }
        DWORD read_bytes = 0;
        if (!WinHttpReadData(h_req, body + len, avail, &read_bytes) || read_bytes == 0) break;
        len += read_bytes;
    }

    body[len] = '\0';
    *out_body = body;
    *out_len = len;

    WinHttpCloseHandle(h_req);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    return 0;
}

void win_free(void* ptr) {
    free(ptr);
}
