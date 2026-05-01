CXX = g++
CXXFlags = -std=c++20 -O3 -march=native -mbmi2 -flto -Wall -Wextra -pthread -DNDEBUG -MMD -MP

SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)

TARGET = tuffchess

-include $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFlags) $(OBJ) -o $(TARGET)
src/%.o: src/%.cpp
	$(CXX) $(CXXFlags) -c $< -o $@

clean:
	rm $(TARGET) $(OBJ)