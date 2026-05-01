# Makefile για το ergasia0.c
CC = gcc
CFLAGS = -Wall -O3
# Προσθήκη βιβλιοθηκών: Threads, Math, και CBLAS
LIBS = -pthread -lm -lopenblas

TARGET = ergasia0

all: $(TARGET)

$(TARGET): ergasia0.c
	$(CC) $(CFLAGS) ergasia0.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
