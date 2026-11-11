#include "../include/cli.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

CLI::CLI(Scheduler& sched) : scheduler(sched), running(true) {}

void CLI::printWelcome() {
    std::cout << "======================================================================\n";
    std::cout << "       SIMULADOR DE KERNEL Y GESTOR DE PROCESOS EN CONSOLA           \n";
    std::cout << "                 Sistemas Operativos - UCAB                           \n";
    std::cout << " Autores: Emanuel Siso, Daniela Goncalves, Christian Vilera          \n";
    std::cout << "======================================================================\n";
    std::cout << " Escriba 'help' para visualizar el catálogo de comandos disponibles.\n";
    std::cout << "======================================================================\n\n";
}

void CLI::printHelp() {
    std::cout << "\nCATÁLOGO DE COMANDOS DISPONIBLES:\n";
    std::cout << "  create <name> <burst> [priority] : Crea un proceso interactivo (priority 1-5, def: 3)\n";
    std::cout << "  load <file_path>                : Carga un archivo de ráfagas/lote de trabajo\n";
    std::cout << "  ps                              : Muestra la tabla de procesos y sus estados\n";
    std::cout << "  pcb <pid>                       : Muestra el bloque de control del proceso (PCB)\n";
    std::cout << "  set_sched <RR|SRTF> [quantum]   : Cambia la política de planificación (quantum def: 2)\n";
    std::cout << "  set_speed <ms>                  : Ajusta la velocidad de simulación (50ms - 5000ms)\n";
    std::cout << "  start                           : Inicia o reanuda la simulación de CPU\n";
    std::cout << "  pause                           : Pausa la simulación de CPU\n";
    std::cout << "  gantt                           : Despliega la traza histórica en diagrama de Gantt\n";
    std::cout << "  stats                           : Muestra métricas de rendimiento y tiempos promedio\n";
    std::cout << "  help                            : Imprime este menú de ayuda\n";
    std::cout << "  exit                            : Finaliza el simulador\n\n";
}

void CLI::processCommand(const std::string& line) {
    size_t first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;

    std::stringstream ss(line);
    std::string cmd;
    ss >> cmd;

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "help") {
        printHelp();
    } else if (cmd == "create") {
        std::string name;
        int burst = 0, priority = 3;
        if (ss >> name >> burst) {
            if (!(ss >> priority)) priority = 3;
            if (scheduler.createProcess(name, burst, priority)) {
                std::cout << "[ÉXITO]: Proceso '" << name << "' (Ráfaga: " << burst
                          << "s, Prioridad: " << priority << ") registrado correctamente.\n";
            } else {
                std::cout << "[ERROR DE SINTAXIS]: La ráfaga debe ser > 0 y la prioridad entre 1 y 5.\n";
            }
        } else {
            std::cout << "[ERROR DE SINTAXIS]: Uso correcto -> create <nombre> <ráfaga> [prioridad]\n";
        }
    } else if (cmd == "load") {
        std::string filepath;
        if (ss >> filepath) {
            std::string err_msg;
            if (scheduler.loadWorkload(filepath, err_msg)) {
                std::cout << "[ÉXITO]: Carga en lote completada exitosamente desde '" << filepath << "'.\n";
            } else {
                std::cout << "[ERROR DE CARGA]: " << err_msg << "\n";
            }
        } else {
            std::cout << "[ERROR DE SINTAXIS]: Uso correcto -> load <ruta_del_archivo>\n";
        }
    } else if (cmd == "ps") {
        scheduler.showPS();
    } else if (cmd == "pcb") {
        int pid;
        if (ss >> pid) {
            scheduler.showPCB(pid);
        } else {
            std::cout << "[ERROR DE SINTAXIS]: Uso correcto -> pcb <pid>\n";
        }
    } else if (cmd == "set_sched") {
        std::string alg_str;
        if (ss >> alg_str) {
            std::transform(alg_str.begin(), alg_str.end(), alg_str.begin(), ::toupper);
            if (alg_str == "RR") {
                int q = 2;
                if (!(ss >> q)) q = 2;
                std::string err;
                if (scheduler.setAlgorithm(Algorithm::RR, q, &err)) {
                    std::cout << "[ÉXITO]: Política cambiada a Round Robin (Quantum: " << q << ").\n";
                } else {
                    std::cout << "[ERROR]: " << err << "\n";
                }
            } else if (alg_str == "SRTF") {
                std::string err;
                if (scheduler.setAlgorithm(Algorithm::SRTF, 2, &err)) {
                    std::cout << "[ÉXITO]: Política cambiada a SRTF (Shortest Remaining Time First).\n";
                } else {
                    std::cout << "[ERROR]: " << err << "\n";
                }
            } else {
                std::cout << "[ERROR DE SINTAXIS]: Algoritmo no reconocido. Opciones válidas: RR | SRTF\n";
            }
        } else {
            std::cout << "[ERROR DE SINTAXIS]: Uso correcto -> set_sched <RR|SRTF> [quantum]\n";
        }
    } else if (cmd == "set_speed") {
        int ms;
        if (ss >> ms) {
            std::string err;
            if (scheduler.setSpeed(ms, &err)) {
                std::cout << "[ÉXITO]: Velocidad de simulación ajustada a " << ms << " ms por tick.\n";
            } else {
                std::cout << "[ERROR]: " << err << "\n";
            }
        } else {
            std::cout << "[ERROR DE SINTAXIS]: Uso correcto -> set_speed <milisegundos_por_tick>\n";
        }
    } else if (cmd == "start") {
        scheduler.startEngine();
        std::cout << "[SISTEMA]: Simulación de CPU INICIADA/REANUDADA.\n";
    } else if (cmd == "pause") {
        scheduler.pauseEngine();
        std::cout << "[SISTEMA]: Simulación de CPU PAUSADA.\n";
    } else if (cmd == "gantt") {
        scheduler.showGantt();
    } else if (cmd == "stats") {
        scheduler.showStats();
    } else if (cmd == "exit") {
        running = false;
        std::cout << "[SISTEMA]: Saliendo del simulador de kernel...\n";
    } else {
        std::cout << "[COMANDO NO RECONOCIDO]: '" << cmd << "'. Escriba 'help' para ver la lista de comandos.\n";
    }
}

void CLI::run() {
    printWelcome();
    std::string line;
    while (running) {
        std::cout << "ucab_kernel> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        processCommand(line);
    }
}
