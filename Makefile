CC = qcc -V8.3.0,gcc_ntox86_64
CFLAGS = -O0 -g -I./include -lang-c++
LDFLAGS = -lc++ -lm

all: launcher aircraft radar computer operator_display communication

launcher: src/launcher/main.o src/shared_memory.o
	$(CC) -o launcher src/launcher/main.o src/shared_memory.o $(LDFLAGS)

aircraft: src/aircraft/main.o src/shared_memory.o
	$(CC) -o aircraft src/aircraft/main.o src/shared_memory.o $(LDFLAGS)

radar: src/radar/main.o src/shared_memory.o
	$(CC) -o radar src/radar/main.o src/shared_memory.o $(LDFLAGS)

computer: src/computer/main.o src/shared_memory.o
	$(CC) -o computer src/computer/main.o src/shared_memory.o $(LDFLAGS)

operator_display: src/operator_display/main.o src/shared_memory.o
	$(CC) -o operator_display src/operator_display/main.o src/shared_memory.o $(LDFLAGS)

communication: src/communication/main.o src/shared_memory.o
	$(CC) -o communication src/communication/main.o src/shared_memory.o $(LDFLAGS)

src/launcher/main.o: src/launcher/main.cpp
	$(CC) $(CFLAGS) -c src/launcher/main.cpp -o src/launcher/main.o

src/aircraft/main.o: src/aircraft/main.cpp
	$(CC) $(CFLAGS) -c src/aircraft/main.cpp -o src/aircraft/main.o

src/radar/main.o: src/radar/main.cpp
	$(CC) $(CFLAGS) -c src/radar/main.cpp -o src/radar/main.o

src/computer/main.o: src/computer/main.cpp
	$(CC) $(CFLAGS) -c src/computer/main.cpp -o src/computer/main.o

src/operator_display/main.o: src/operator_display/main.cpp
	$(CC) $(CFLAGS) -c src/operator_display/main.cpp -o src/operator_display/main.o

src/communication/main.o: src/communication/main.cpp
	$(CC) $(CFLAGS) -c src/communication/main.cpp -o src/communication/main.o

src/shared_memory.o: src/shared_memory.cpp
	$(CC) $(CFLAGS) -c src/shared_memory.cpp -o src/shared_memory.o

clean:
	rm -f *.o src/*/main.o launcher aircraft radar computer operator_display communication