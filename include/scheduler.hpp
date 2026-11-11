#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <vector>
#include <string>
#include <queue>
#include "pcb.hpp"

// Abstracción de Concurrencia Multiplataforma (Windows Win32 API / Linux POSIX)
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

class PlatformMutex {
private:
    CRITICAL_SECTION cs;
public:
    PlatformMutex() { InitializeCriticalSection(&cs); }
    ~PlatformMutex() { DeleteCriticalSection(&cs); }
    void lock() { EnterCriticalSection(&cs); }
    void unlock() { LeaveCriticalSection(&cs); }
};

class PlatformThread {
private:
    HANDLE hThread;
    bool created;
public:
    PlatformThread() : hThread(NULL), created(false) {}
    ~PlatformThread() { join(); }

    bool start(void* (*func)(void*), void* arg) {
        if (created) return false;
        hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)arg, 0, NULL);
        if (hThread != NULL) {
            created = true;
            return true;
        }
        return false;
    }

    void join() {
        if (created && hThread != NULL) {
            WaitForSingleObject(hThread, INFINITE);
            CloseHandle(hThread);
            hThread = NULL;
            created = false;
        }
    }
};

inline void platform_sleep_ms(int ms) {
    Sleep(ms);
}

#else
#include <pthread.h>
#include <unistd.h>

class PlatformMutex {
private:
    pthread_mutex_t mtx;
public:
    PlatformMutex() { pthread_mutex_init(&mtx, NULL); }
    ~PlatformMutex() { pthread_mutex_destroy(&mtx); }
    void lock() { pthread_mutex_lock(&mtx); }
    void unlock() { pthread_mutex_unlock(&mtx); }
};

class PlatformThread {
private:
    pthread_t thread;
    bool created;
public:
    PlatformThread() : created(false) {}
    ~PlatformThread() { join(); }

    bool start(void* (*func)(void*), void* arg) {
        if (created) return false;
        int res = pthread_create(&thread, NULL, (void*(*)(void*))func, (void*)arg);
        if (res == 0) {
            created = true;
            return true;
        }
        return false;
    }

    void join() {
        if (created) {
            pthread_join(thread, NULL);
            created = false;
        }
    }
};

inline void platform_sleep_ms(int ms) {
    usleep(ms * 1000);
}

#endif

class PlatformMutexGuard {
private:
    PlatformMutex& mtx;
public:
    PlatformMutexGuard(PlatformMutex& m) : mtx(m) { mtx.lock(); }
    ~PlatformMutexGuard() { mtx.unlock(); }
};

enum class Algorithm {
    RR,
    SRTF
};

std::string algorithmToString(Algorithm alg);

struct GanttSegment {
    int start_tick;
    int end_tick;
    std::string process_label;
    int pid;
};

class Scheduler {
private:
    // Sincronización de Cerrojos de Exclusión Mutua (Mutex) e Hilo CPU
    PlatformMutex lock;
    PlatformThread cpu_thread;

    // Estado del motor de CPU
    bool thread_created;
    bool is_running;
    bool is_paused;
    bool should_exit;

    // Parámetros de la simulación
    int clock_ticks;
    int speed_ms;
    Algorithm active_algorithm;
    int rr_quantum;
    int next_pid;

    // Estructuras de datos de procesos
    std::vector<PCB> all_processes;         // Lista maestra de procesos registrados
    std::vector<PCB> pending_arrivals;      // Procesos pendientes por cargar según arrival_time
    PCB* running_process;                    // Puntero al proceso actualmente en CPU
    std::vector<GanttSegment> gantt_log;    // Registro de intervalos para el diagrama de Gantt

    int total_idle_ticks;

    // Métodos internos protegidos por Mutex
    void checkArrivals();
    void selectNextProcess();
    void updateGantt(const std::string& label, int pid);

public:
    Scheduler();
    ~Scheduler();

    void init();
    void cleanup();

    // Control de ejecución del motor
    void startEngine();
    void pauseEngine();
    void stopEngine();

    // Comandos de la CLI
    bool createProcess(const std::string& name, int burst, int priority = 3);
    bool loadWorkload(const std::string& filepath, std::string& err_msg);
    bool setAlgorithm(Algorithm alg, int quantum = 2, std::string* err_msg = nullptr);
    bool setSpeed(int ms_per_tick, std::string* err_msg = nullptr);

    // Vistas y estadísticas
    void showPS();
    void showPCB(int pid);
    void showGantt();
    void showStats();

    // Función principal del Hilo CPU
    void runCPULoop();
};

#endif // SCHEDULER_HPP
