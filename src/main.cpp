#include "../include/scheduler.hpp"
#include "../include/cli.hpp"
#include <iostream>

int main() {
    Scheduler scheduler;
    scheduler.init();

    CLI cli(scheduler);
    cli.run();

    scheduler.cleanup();
    std::cout << "[SISTEMA]: Recursos liberados correctamente. Simulación finalizada." << std::endl;
    return 0;
}
