#include "../include/scheduler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

std::string algorithmToString(Algorithm alg) {
    switch (alg) {
        case Algorithm::RR:   return "RR";
        case Algorithm::SRTF: return "SRTF";
        default:             return "UNKNOWN";
    }
}

// C-bridge function para inicio del hilo CPU
static void* threadEntry(void* arg) {
    Scheduler* sched = static_cast<Scheduler*>(arg);
    sched->runCPULoop();
    return nullptr;
}

Scheduler::Scheduler()
    : thread_created(false), is_running(false), is_paused(false), should_exit(false),
      clock_ticks(0), speed_ms(1000), active_algorithm(Algorithm::RR),
      rr_quantum(2), next_pid(1), running_process(nullptr), total_idle_ticks(0) {
}

Scheduler::~Scheduler() {
    cleanup();
}

void Scheduler::init() {
    {
        PlatformMutexGuard guard(lock);
        should_exit = false;
        is_running = false;
        is_paused = false;
    }

    if (!thread_created) {
        if (cpu_thread.start(threadEntry, this)) {
            thread_created = true;
        }
    }
}

void Scheduler::cleanup() {
    {
        PlatformMutexGuard guard(lock);
        should_exit = true;
        is_running = false;
    }

    if (thread_created) {
        cpu_thread.join();
        thread_created = false;
    }
}

void Scheduler::startEngine() {
    PlatformMutexGuard guard(lock);
    is_running = true;
    is_paused = false;
}

void Scheduler::pauseEngine() {
    PlatformMutexGuard guard(lock);
    is_paused = true;
}

void Scheduler::stopEngine() {
    PlatformMutexGuard guard(lock);
    is_running = false;
    is_paused = false;
}

bool Scheduler::createProcess(const std::string& name, int burst, int priority) {
    if (burst <= 0 || priority < 1 || priority > 5) {
        return false;
    }

    PlatformMutexGuard guard(lock);

    PCB p;
    p.pid = next_pid++;
    p.name = name;
    p.initial_burst = burst;
    p.remaining_burst = burst;
    p.priority = priority;
    p.arrival_time = clock_ticks; // Creado interactivamente en el tick actual
    p.state = ProcessState::READY;
    p.start_time = -1;
    p.completion_time = -1;
    p.waiting_time = 0;
    p.turnaround_time = 0;
    p.response_time = -1;
    p.time_in_current_run = 0;

    all_processes.push_back(p);

    return true;
}

bool Scheduler::loadWorkload(const std::string& filepath, std::string& err_msg) {
    std::ifstream file(filepath.c_str());
    if (!file.is_open()) {
        err_msg = "No se pudo abrir el archivo especificado: " + filepath;
        return false;
    }

    std::string line;
    int loaded_count = 0;

    PlatformMutexGuard guard(lock);

    while (std::getline(file, line)) {
        // Limpiar espacios en blanco al inicio/fin
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue; // Línea vacía
        if (line[first] == '#') continue;        // Comentario

        // Reemplazar comas por espacios para parsing unificado
        std::string clean_line = line;
        std::replace(clean_line.begin(), clean_line.end(), ',', ' ');

        std::stringstream ss(clean_line);
        std::string name;
        int burst, priority = 3, arrival = 0;

        if (ss >> name >> burst) {
            if (!(ss >> priority)) priority = 3;
            if (!(ss >> arrival)) arrival = 0;

            if (burst > 0 && priority >= 1 && priority <= 5 && arrival >= 0) {
                PCB p;
                p.pid = next_pid++;
                p.name = name;
                p.initial_burst = burst;
                p.remaining_burst = burst;
                p.priority = priority;
                p.arrival_time = arrival;
                p.start_time = -1;
                p.completion_time = -1;
                p.waiting_time = 0;
                p.turnaround_time = 0;
                p.response_time = -1;
                p.time_in_current_run = 0;

                if (arrival <= clock_ticks) {
                    p.state = ProcessState::READY;
                    all_processes.push_back(p);
                } else {
                    p.state = ProcessState::NEW;
                    pending_arrivals.push_back(p);
                }
                loaded_count++;
            }
        }
    }

    file.close();

    if (loaded_count == 0) {
        err_msg = "El archivo no contiene registros de procesos válidos.";
        return false;
    }

    return true;
}

bool Scheduler::setAlgorithm(Algorithm alg, int quantum, std::string* err_msg) {
    if (alg == Algorithm::RR && quantum <= 0) {
        if (err_msg) *err_msg = "El quantum para Round Robin debe ser un entero mayor a 0.";
        return false;
    }

    PlatformMutexGuard guard(lock);
    active_algorithm = alg;
    if (alg == Algorithm::RR) {
        rr_quantum = quantum;
    }
    return true;
}

bool Scheduler::setSpeed(int ms_per_tick, std::string* err_msg) {
    if (ms_per_tick < 50 || ms_per_tick > 5000) {
        if (err_msg) *err_msg = "La velocidad del tick debe estar entre 50 ms y 5000 ms.";
        return false;
    }

    PlatformMutexGuard guard(lock);
    speed_ms = ms_per_tick;
    return true;
}

void Scheduler::checkArrivals() {
    // Debe llamarse dentro de candado mutex
    auto it = pending_arrivals.begin();
    while (it != pending_arrivals.end()) {
        if (it->arrival_time <= clock_ticks) {
            it->state = ProcessState::READY;
            all_processes.push_back(*it);
            it = pending_arrivals.erase(it);
        } else {
            ++it;
        }
    }
}

void Scheduler::selectNextProcess() {
    // Debe llamarse dentro de candado mutex
    if (running_process != nullptr && running_process->state == ProcessState::RUNNING) {
        return;
    }

    PCB* best_candidate = nullptr;

    if (active_algorithm == Algorithm::RR) {
        // En RR, seleccionar el proceso READY con menor arrival_time
        for (auto& proc : all_processes) {
            if (proc.state == ProcessState::READY) {
                if (best_candidate == nullptr || proc.arrival_time < best_candidate->arrival_time ||
                    (proc.arrival_time == best_candidate->arrival_time && proc.pid < best_candidate->pid)) {
                    best_candidate = &proc;
                }
            }
        }
    } else if (active_algorithm == Algorithm::SRTF) {
        // En SRTF, seleccionar el proceso READY con menor ráfaga restante
        for (auto& proc : all_processes) {
            if (proc.state == ProcessState::READY) {
                if (best_candidate == nullptr) {
                    best_candidate = &proc;
                } else if (proc.remaining_burst < best_candidate->remaining_burst) {
                    best_candidate = &proc;
                } else if (proc.remaining_burst == best_candidate->remaining_burst) {
                    if (proc.arrival_time < best_candidate->arrival_time ||
                        (proc.arrival_time == best_candidate->arrival_time && proc.pid < best_candidate->pid)) {
                        best_candidate = &proc;
                    }
                }
            }
        }
    }

    if (best_candidate != nullptr) {
        running_process = best_candidate;
        running_process->state = ProcessState::RUNNING;
        if (running_process->start_time == -1) {
            running_process->start_time = clock_ticks;
            running_process->response_time = running_process->start_time - running_process->arrival_time;
        }
        running_process->time_in_current_run = 0;
    }
}

void Scheduler::updateGantt(const std::string& label, int pid) {
    if (!gantt_log.empty() && gantt_log.back().pid == pid) {
        gantt_log.back().end_tick = clock_ticks + 1;
    } else {
        GanttSegment seg;
        seg.start_tick = clock_ticks;
        seg.end_tick = clock_ticks + 1;
        seg.process_label = label;
        seg.pid = pid;
        gantt_log.push_back(seg);
    }
}

void Scheduler::runCPULoop() {
    while (true) {
        bool active = false;
        int current_speed = 1000;
        bool exit_signal = false;

        {
            PlatformMutexGuard guard(lock);
            if (should_exit) {
                exit_signal = true;
            } else {
                active = is_running && !is_paused;
                current_speed = speed_ms;
            }
        }

        if (exit_signal) break;

        if (!active) {
            platform_sleep_ms(50);
            continue;
        }

        {
            PlatformMutexGuard guard(lock);

            // 1. Verificar llegadas en el tick actual
            checkArrivals();

            // 2. SRTF Desapropiación inmediata
            if (running_process != nullptr && running_process->state == ProcessState::RUNNING) {
                if (active_algorithm == Algorithm::SRTF) {
                    for (auto& proc : all_processes) {
                        if (proc.state == ProcessState::READY && proc.remaining_burst < running_process->remaining_burst) {
                            running_process->state = ProcessState::READY;
                            running_process = nullptr;
                            break;
                        }
                    }
                }
            }

            // 3. Seleccionar siguiente proceso si CPU está libre
            selectNextProcess();

            // 4. Ejecutar 1 tick de CPU
            if (running_process != nullptr) {
                updateGantt(running_process->name, running_process->pid);
                running_process->remaining_burst--;
                running_process->time_in_current_run++;

                for (auto& proc : all_processes) {
                    if (proc.state == ProcessState::READY && proc.pid != running_process->pid) {
                        proc.waiting_time++;
                    }
                }

                if (running_process->remaining_burst == 0) {
                    running_process->state = ProcessState::TERMINATED;
                    running_process->completion_time = clock_ticks + 1;
                    running_process->turnaround_time = running_process->completion_time - running_process->arrival_time;
                    running_process = nullptr;
                } else if (active_algorithm == Algorithm::RR && running_process->time_in_current_run >= rr_quantum) {
                    bool has_other_ready = false;
                    for (auto& proc : all_processes) {
                        if (proc.state == ProcessState::READY) {
                            has_other_ready = true;
                            break;
                        }
                    }
                    if (has_other_ready) {
                        running_process->state = ProcessState::READY;
                        running_process->arrival_time = clock_ticks + 1;
                        running_process = nullptr;
                    } else {
                        running_process->time_in_current_run = 0;
                    }
                }
            } else {
                updateGantt("IDLE", -1);
                total_idle_ticks++;

                for (auto& proc : all_processes) {
                    if (proc.state == ProcessState::READY) {
                        proc.waiting_time++;
                    }
                }
            }

            clock_ticks++;
        }

        platform_sleep_ms(current_speed);
    }
}

void Scheduler::showPS() {
    PlatformMutexGuard guard(lock);

    std::cout << "\n"
              << std::setw(6)  << "PID" << " | "
              << std::setw(15) << "NOMBRE" << " | "
              << std::setw(12) << "ESTADO" << " | "
              << std::setw(16) << "BURST RESTANTE" << " | "
              << std::setw(10) << "PRIORIDAD" << "\n";
    std::cout << std::string(70, '-') << "\n";

    if (all_processes.empty() && pending_arrivals.empty()) {
        std::cout << " [ No hay procesos registrados en el sistema ]\n";
    } else {
        for (const auto& proc : all_processes) {
            std::cout << std::setw(6)  << proc.pid << " | "
                      << std::setw(15) << proc.name << " | "
                      << std::setw(12) << stateToString(proc.state) << " | "
                      << std::setw(14) << (std::to_string(proc.remaining_burst) + "s") << " | "
                      << std::setw(10) << proc.priority << "\n";
        }
        for (const auto& proc : pending_arrivals) {
            std::cout << std::setw(6)  << proc.pid << " | "
                      << std::setw(15) << proc.name << " | "
                      << std::setw(12) << "NEW (Pend)" << " | "
                      << std::setw(14) << (std::to_string(proc.remaining_burst) + "s") << " | "
                      << std::setw(10) << proc.priority << "\n";
        }
    }
    std::cout << std::endl;
}

void Scheduler::showPCB(int pid) {
    PlatformMutexGuard guard(lock);

    const PCB* target = nullptr;
    for (const auto& proc : all_processes) {
        if (proc.pid == pid) {
            target = &proc;
            break;
        }
    }
    if (!target) {
        for (const auto& proc : pending_arrivals) {
            if (proc.pid == pid) {
                target = &proc;
                break;
            }
        }
    }

    if (!target) {
        std::cout << "[ERROR DE PARAMETRO]: El PID " << pid << " no existe en el sistema.\n";
        return;
    }

    std::cout << "==================================================\n";
    std::cout << "CONTROL BLOCK DE PROCESO (PCB) - PID: " << target->pid << "\n";
    std::cout << "==================================================\n";
    std::cout << "Nombre del Proceso : " << target->name << "\n";
    std::cout << "Estado Actual      : " << stateToString(target->state) << "\n";
    std::cout << "Prioridad          : " << target->priority << "\n";
    std::cout << "Tiempo de Llegada  : " << target->arrival_time << " ticks\n";
    std::cout << "Ráfaga Inicial     : " << target->initial_burst << " ticks\n";
    std::cout << "Ráfaga Restante    : " << target->remaining_burst << " ticks\n";
    std::cout << "Tiempo de Espera   : " << target->waiting_time << " ticks\n";

    if (target->state == ProcessState::TERMINATED) {
        std::cout << "Tiempo de Retorno  : " << target->turnaround_time << " ticks\n";
    } else {
        std::cout << "Tiempo de Retorno  : N/A (En ejecución/pendiente)\n";
    }

    if (target->response_time != -1) {
        std::cout << "Tiempo de Respuesta: " << target->response_time << " ticks\n";
    } else {
        std::cout << "Tiempo de Respuesta: N/A (No ha ejecutado)\n";
    }
    std::cout << "==================================================\n" << std::endl;
}

void Scheduler::showGantt() {
    PlatformMutexGuard guard(lock);

    std::cout << "\nDIAGRAMA DE GANTT (0 - " << clock_ticks << " ticks):\n";
    if (gantt_log.empty()) {
        std::cout << "[ Sin actividad registrada ]\n\n";
        return;
    }

    for (size_t i = 0; i < gantt_log.size(); ++i) {
        const auto& seg = gantt_log[i];
        std::cout << "[" << seg.start_tick << "-" << seg.end_tick << "s: " << seg.process_label << "]";
        if (i + 1 < gantt_log.size()) {
            std::cout << " -> ";
        }
    }
    std::cout << "\n" << std::endl;
}

void Scheduler::showStats() {
    PlatformMutexGuard guard(lock);

    int total_procs = all_processes.size() + pending_arrivals.size();
    int terminated_procs = 0;
    double sum_wt = 0.0;
    double sum_tat = 0.0;
    double sum_rt = 0.0;
    int rt_count = 0;

    for (const auto& proc : all_processes) {
        if (proc.state == ProcessState::TERMINATED) {
            terminated_procs++;
            sum_wt += proc.waiting_time;
            sum_tat += proc.turnaround_time;
        } else {
            sum_wt += proc.waiting_time;
        }
        if (proc.response_time != -1) {
            sum_rt += proc.response_time;
            rt_count++;
        }
    }

    double avg_wt = (total_procs > 0) ? (sum_wt / total_procs) : 0.0;
    double avg_tat = (terminated_procs > 0) ? (sum_tat / terminated_procs) : 0.0;
    double avg_rt = (rt_count > 0) ? (sum_rt / rt_count) : 0.0;

    double cpu_utilization = 0.0;
    if (clock_ticks > 0) {
        int busy_ticks = clock_ticks - total_idle_ticks;
        cpu_utilization = (static_cast<double>(busy_ticks) / clock_ticks) * 100.0;
    }

    std::cout << "\nESTADÍSTICAS DEL SISTEMA:\n";
    std::cout << "- Algoritmo Activo          : " << algorithmToString(active_algorithm);
    if (active_algorithm == Algorithm::RR) {
        std::cout << " (Quantum: " << rr_quantum << ")";
    }
    std::cout << "\n";
    std::cout << "- Velocidad de Tick         : " << speed_ms << " ms\n";
    std::cout << "- Ticks Totales Transcurridos: " << clock_ticks << "\n";
    std::cout << "- Procesos Finalizados      : " << terminated_procs << " / " << total_procs << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "- Tiempo Promedio de Espera : " << avg_wt << " ticks\n";
    std::cout << "- Tiempo Promedio de Retorno: " << avg_tat << " ticks\n";
    std::cout << "- Tiempo Promedio Respuesta : " << avg_rt << " ticks\n";
    std::cout << "- Uso de CPU                : " << cpu_utilization << "%\n" << std::endl;
}
