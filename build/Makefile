CC = qcc -Vgcc_ntox86_64
CXX = q++ -Vgcc_ntox86_64
LD = $(CXX)
CFLAGS = -g -Wall -I../src/include
LDFLAGS = -g
SRCS = $(wildcard ../src/main/*.cpp ../src/subsystems/*.cpp)
OBJS = $(patsubst ../src/%.cpp, %.o, $(SRCS))
TARGET = x86_64-debug/ATC

all: $(TARGET)

%.o: ../src/main/%.cpp
	@mkdir -p x86_64-debug
	$(CXX) $(CFLAGS) -c $< -o x86_64-debug/$@

%.o: ../src/subsystems/%.cpp
	@mkdir -p x86_64-debug
	$(CXX) $(CFLAGS) -c $< -o x86_64-debug/$@

$(TARGET): $(OBJS)
	@mkdir -p x86_64-debug
	$(LD) $(LDFLAGS) -o $@ $(addprefix x86_64-debug/, $(OBJS))

clean:
	rm -rf x86_64-debug/*