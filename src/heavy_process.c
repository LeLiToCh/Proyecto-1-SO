#include "heavy_process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
static pid_t spawnv_wrap(const char *cmd, char *const argv[])
{
    // _spawnvp returns child exit code if P_WAIT, but we want async. Use _P_NOWAIT.
    int pid = _spawnvp(_P_NOWAIT, cmd, (const char *const *)argv);
    if (pid < 0)
        perror("_spawnvp");
    return pid;
}
int wait_process(pid_t pid, int *out_exit_code)
{
    // Best-effort: on Windows with spawn, there's no direct wait by pid without handle.
    // We fallback to busy sleep scanning process exit via system("tasklist") is ugly; instead just return 0.
    if (out_exit_code)
        *out_exit_code = 0;
    return 0;
}
#else
#include <unistd.h>
#include <sys/wait.h>
static pid_t fork_exec(const char *cmd, char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(cmd, argv);
        perror("execvp");
        _exit(127);
    }
    return pid;
}

// Try executing a specific path first (e.g. ./build/proc_emisor), then fall back
// to execvp(cmd, argv) which searches PATH.
static pid_t fork_exec_try(const char *cmd, char *const argv[])
{
    char pathbuf[512];
    // If cmd looks like "proc_emisor", try ./build/proc_emisor
    if (strchr(cmd, '/') == NULL)
    {
        snprintf(pathbuf, sizeof(pathbuf), "./build/%s", cmd);
        if (access(pathbuf, X_OK) == 0)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                execv(pathbuf, argv);
                perror("execv");
                _exit(127);
            }
            return pid;
        }
    }
    // fallback
    return fork_exec(cmd, argv);
}
int wait_process(pid_t pid, int *out_exit_code)
{
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    if (out_exit_code)
    {
        if (WIFEXITED(status))
            *out_exit_code = WEXITSTATUS(status);
        else
            *out_exit_code = -1;
    }
    return 0;
}
#endif

#define DEFAULT_NAME "/mem_ascii"

pid_t launch_inicializador_heavy(const char *name, int buffer_capacity)
{
    const char *nm = (name && *name) ? name : DEFAULT_NAME;
    char capbuf[32];
    snprintf(capbuf, sizeof capbuf, "%d", buffer_capacity > 0 ? buffer_capacity : 4096);
    char *argv[] = {"proc_inicializador", "--create", "--buffer", capbuf, "--name", (char *)nm, NULL};
#ifdef _WIN32
    return spawnv_wrap("proc_inicializador", argv);
#else
    return fork_exec_try("proc_inicializador", argv);
#endif
}

pid_t launch_emisor_heavy(const char *name, const char *input_path, const char *key_bits, bool automatic)
{
    const char *nm = (name && *name) ? name : DEFAULT_NAME;
    char *argv_auto[] = {"proc_emisor", "--input", (char *)input_path, "--key-bits", (char *)key_bits, "--auto", "--name", (char *)nm, NULL};
    char *argv_man[] = {"proc_emisor", "--input", (char *)input_path, "--key-bits", (char *)key_bits, "--name", (char *)nm, NULL};
#ifdef _WIN32
    return spawnv_wrap("proc_emisor", automatic ? argv_auto : argv_man);
#else
    return fork_exec_try("proc_emisor", automatic ? argv_auto : argv_man);
#endif
}

pid_t launch_receptor_heavy(const char *name, const char *key_bits, bool automatic, const char *out_path)
{
    const char *nm = (name && *name) ? name : DEFAULT_NAME;
    char *argv_auto_out[] = {"proc_receptor", "--key-bits", (char *)key_bits, "--auto", "--out", (char *)(out_path ? out_path : ""), "--name", (char *)nm, NULL};
    char *argv_auto[] = {"proc_receptor", "--key-bits", (char *)key_bits, "--auto", "--name", (char *)nm, NULL};
    char *argv_man_out[] = {"proc_receptor", "--key-bits", (char *)key_bits, "--out", (char *)(out_path ? out_path : ""), "--name", (char *)nm, NULL};
    char *argv_man[] = {"proc_receptor", "--key-bits", (char *)key_bits, "--name", (char *)nm, NULL};
#ifdef _WIN32
    if (automatic)
        return spawnv_wrap("proc_receptor", out_path ? argv_auto_out : argv_auto);
    else
        return spawnv_wrap("proc_receptor", out_path ? argv_man_out : argv_man);
#else
    if (automatic)
        return fork_exec_try("proc_receptor", out_path ? argv_auto_out : argv_auto);
    else
        return fork_exec_try("proc_receptor", out_path ? argv_man_out : argv_man);
#endif
}

pid_t launch_finalizador_heavy(const char *name, int total_chars_written)
{
    const char *nm = (name && *name) ? name : DEFAULT_NAME;
    char totalbuf[32];
    snprintf(totalbuf, sizeof totalbuf, "%d", total_chars_written >= 0 ? total_chars_written : 0);
    char *argv[] = {"proc_finalizador", "--total", totalbuf, "--name", (char *)nm, NULL};
#ifdef _WIN32
    return spawnv_wrap("proc_finalizador", argv);
#else
    return fork_exec_try("proc_finalizador", argv);
#endif
}