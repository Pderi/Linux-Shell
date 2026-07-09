CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -D_GNU_SOURCE -Iinclude
LDFLAGS = -lreadline -ltinfo

SRCS = src/main.c \
       src/parser.c \
       src/executor.c \
       src/builtins.c \
       src/builtin_ls.c \
       src/builtin_cat.c \
       src/builtin_grep.c \
       src/history.c \
       src/alias.c \
       src/input.c \
       src/type.c \
       src/completion.c \
       src/shell.c

OBJS = $(SRCS:.c=.o)
TARGET = myshell

.PHONY: all clean test

all: $(TARGET)

test: $(TARGET)
	bash scripts/run_tests.sh

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
