CC = gcc
CFLAGS = -Wall -pedantic -std=c99 -g -pthread

TARGET = lab_2

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET)