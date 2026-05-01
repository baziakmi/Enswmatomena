# Makefile για το producer_cons.c
CC = gcc
CFLAGS = -Wall -O3
LIBS = -pthread -lm

TARGET = producer_cons

all: $(TARGET)

$(TARGET): producer_cons.c
	$(CC) $(CFLAGS) producer_cons.c -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
