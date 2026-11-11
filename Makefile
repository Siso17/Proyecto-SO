CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -Iinclude -std=c++17
SRCS = src/main.cpp src/pcb.cpp src/scheduler.cpp src/cli.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = main

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) main.exe src/*.o
