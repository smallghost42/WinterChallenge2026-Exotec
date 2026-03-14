CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
TARGET = bot

.PHONY: all clean

all: $(TARGET)

$(TARGET): bot.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) bot.cpp

clean:
	rm -f $(TARGET)
