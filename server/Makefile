CC = gcc
CFLAGS = -Wall -g -I/usr/include        
LIBS = -lmysqlclient -lcjson -pthread      

SRCS = src/server.c \
       src/config/db.c \
       src/service/auth_service.c \
       src/service/room_service.c \
       src/utils/logger.c \
       src/utils/net_utils.c \
       src/route/route.c


TARGET = server

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)
