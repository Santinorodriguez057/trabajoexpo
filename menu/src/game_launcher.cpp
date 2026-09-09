#include "game_launcher.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

int GameLauncher::launch(const std::string& executable) {
#if defined(_WIN32)
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    std::string cmd = "\"" + executable + "\"";
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
#else
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        // Proceso hijo: sustituye la imagen del proceso por el juego.
        execl(executable.c_str(), executable.c_str(), static_cast<char*>(nullptr));
        _exit(127); // solo se llega aqui si execl falla
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}