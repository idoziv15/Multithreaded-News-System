CC = gcc
CFLAGS = -Wall -Wextra -pthread

SRCS = main.c
HDRS = main.h
OBJS = $(SRCS:.c=.o)
TARGET = news_system.out

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)