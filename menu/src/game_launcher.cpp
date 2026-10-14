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

    // Normalizar ruta para Windows (reemplazar / por \)
    std::string winPath = executable;
    for (char& c : winPath) {
        if (c == '/') c = '\\';
    }

    std::string targetExe = winPath;
    if (GetFileAttributesA(targetExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // Si no existe, probar con solo el nombre del archivo (ej. si se corre desde build/bin/)
        size_t lastSlash = winPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            std::string baseName = winPath.substr(lastSlash + 1);
            if (GetFileAttributesA(baseName.c_str()) != INVALID_FILE_ATTRIBUTES) {
                targetExe = baseName;
            }
        }
    }

    std::string cmd = "\"" + targetExe + "\"";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0,
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
    std::string targetExe = executable;
    if (access(targetExe.c_str(), X_OK) != 0) {
        size_t lastSlash = executable.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string baseName = executable.substr(lastSlash + 1);
            if (access(baseName.c_str(), X_OK) == 0) {
                targetExe = "./" + baseName;
            }
        }
    }

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        // Proceso hijo: sustituye la imagen del proceso por el juego.
        execl(targetExe.c_str(), targetExe.c_str(), static_cast<char*>(nullptr));
        _exit(127); // solo se llega aqui si execl falla
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}