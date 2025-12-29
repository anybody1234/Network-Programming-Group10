CC = gcc
CFLAGS = -Wall -g

LIBS = -lcjson -pthread


SRCS = main.c ui.c network.c logic.c


TARGET = client

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)