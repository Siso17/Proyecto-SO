#ifndef CLI_HPP
#define CLI_HPP

#include "scheduler.hpp"

class CLI {
private:
    Scheduler& scheduler;
    bool running;

    void printWelcome();
    void printHelp();
    void processCommand(const std::string& line);

public:
    explicit CLI(Scheduler& sched);
    void run();
};

#endif // CLI_HPP
