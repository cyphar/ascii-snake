CC ?= gcc
NAME = snake

WARN = -Wall -Wextra -Werror -pedantic
CFLAGS = -std=c99
LFLAGS =

SRC = snake.c

$(NAME): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LFLAGS) $(WARN) -o $(NAME)

debug: $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LFLAGS) $(WARN) -o $(NAME) -g -O0
