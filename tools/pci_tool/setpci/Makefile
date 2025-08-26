CC := gcc
CFLAGS := -Wall -O0
TARGET := setpci
SRC := setpci.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
