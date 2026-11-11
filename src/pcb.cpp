#include "../include/pcb.hpp"

std::string stateToString(ProcessState state) {
    switch (state) {
        case ProcessState::NEW:        return "NEW";
        case ProcessState::READY:      return "READY";
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::PAUSED:     return "PAUSED";
        case ProcessState::TERMINATED: return "TERMINATED";
        default:                       return "UNKNOWN";
    }
}
