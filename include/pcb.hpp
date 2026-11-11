#ifndef PCB_HPP
#define PCB_HPP

#include <string>

enum class ProcessState {
    NEW,
    READY,
    RUNNING,
    PAUSED,
    TERMINATED
};

std::string stateToString(ProcessState state);

struct PCB {
    int pid;
    std::string name;
    int initial_burst;
    int remaining_burst;
    int priority;
    ProcessState state;

    int arrival_time;
    int start_time;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int response_time;

    int time_in_current_run;

    PCB()
        : pid(0), name(""), initial_burst(0), remaining_burst(0), priority(3),
          state(ProcessState::NEW), arrival_time(0), start_time(-1),
          completion_time(-1), waiting_time(0), turnaround_time(0),
          response_time(-1), time_in_current_run(0) {}
};

#endif // PCB_HPP
