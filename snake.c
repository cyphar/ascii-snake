/* ascii-snake: a remake of the old nokia game
 * Copyright (c) 2013 Cyphar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>

/* for seeding randomness */
#include <unistd.h>
#include <time.h>

/* for killing input process */
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

/* for unbuffered input */
#include <termios.h>

#if !defined(bool)
#	define true 1
#	define false 0
#	define bool int
#endif

#define ANSI_RED	"\x1b[1;31m"
#define ANSI_GREEN	"\x1b[1;32m"
#define ANSI_YELLOW	"\x1b[1;33m"
#define ANSI_BLUE	"\x1b[1;34m"
#define ANSI_WHITE	"\x1b[1;37m"
#define ANSI_CLEAR	"\x1b[0m"

#define SNAKE_WRAP	true
#define SNAKE_BODY	'*'
#define SNAKE_HEAD	'x'
#define FOOD		'@'

/*
#define SNAKE_BODY	ANSI_WHITE  "*" ANSI_WHITE
#define SNAKE_HEAD	ANSI_BLUE   "x" ANSI_CLEAR
#define FOOD		ANSI_RED    "@" ANSI_CLEAR
*/
#define SCORE_FORMAT	"Score: " ANSI_WHITE "%d" ANSI_CLEAR

#define SCREEN_WIDTH	120
#define SCREEN_HEIGHT	40

/* for positions, (x, y)*/
#define X 0
#define Y 1

/* global states */
int quit = false;
char **game_state = NULL; /* stores the string of the current game state */

/* snake */
int snake_len = 5;
int snake_head[2]; /* position of head */
int **snake_body = NULL; /* position of all body parts */

enum {
	up,
	down,
	left,
	right
};

int snake_direction = right;

/* food / score */
int score;
int food[2]; /* (x, y) of current food position */

char getch(void) {
	char ch;
	static struct termios old, new;

	tcgetattr(0, &old); /* grab old terminal i/o settings */
	new = old; /* make new settings same as old settings */
	new.c_lflag &= ~ICANON; /* disable buffered i/o */
	new.c_lflag &= ~ECHO; /* set echo mode */
	tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */

	ch = getchar();
	tcsetattr(0, TCSANOW, &old); /* reset terminal i/o settings */

	/* if ctrl-c was pressed, exit */
	if(ch == 3)
		exit(2);
	return ch;
} /* getch_() */

int snake_rand(int min, int max) {
	/* generate x, where min <= x <= max */
	return (rand() % (max + 1 - min)) + min;
} /* snake_rand() */

void snake_init(void) {
	/* seed randomness */
	srand(time(NULL) ^ getpid());

	/* set defaults */
	score = 0;
	snake_len = 5;
	snake_head[X] =  SCREEN_WIDTH / 2;
	snake_head[Y] = SCREEN_HEIGHT / 2;
	food[X] = snake_rand(0,  SCREEN_WIDTH);
	food[Y] = snake_rand(0, SCREEN_HEIGHT);

	/* allocate all the things */
	int i;

	game_state = calloc(SCREEN_HEIGHT * SCREEN_WIDTH, sizeof(char *));
	for(i = 0; i < SCREEN_WIDTH; i++) {
		game_state[i] = calloc(SCREEN_WIDTH + 1, 1);
		game_state[i][SCREEN_WIDTH] = '\0';
	}

	/* snake body */
	snake_body = malloc(snake_len * sizeof(int *));
	for(i = 0; i < snake_len; i++) {
		snake_body[i] = malloc(2 * sizeof(int));

		/* set default position */
		snake_body[i][X] = snake_head[X] - i;
		snake_body[i][Y] = snake_head[Y];
	}
} /* snake_init() */

void snake_input(void) {
	int ch = getch(), weird = false;

	switch(ch) {
		case 'q':
		case 'Q':
			quit = true;
			break;
		case 'w':
		case 'W':
			if(snake_direction != down)
				snake_direction = up;
			break;
		case 's':
		case 'S':
			if(snake_direction != up)
				snake_direction = down;
			break;
		case 'd':
		case 'D':
			if(snake_direction != left)
				snake_direction = right;
			break;
		case 'a':
		case 'A':
			if(snake_direction != right)
				snake_direction = left;
			break;
		case 27:
			weird = true;
			break;
	}

	/* check if key was 'normal' */
	if(!weird)
		return;

	/* up, down, left, right keys -- specific to my machine and os */
	if(ch == 27 && getch() == 91) {
		switch(getch()) {
			case 65:
				if(snake_direction != down)
					snake_direction = up;
				break;
			case 66:
				if(snake_direction != up)
					snake_direction = down;
				break;
			case 67:
				if(snake_direction != left)
					snake_direction = right;
				break;
			case 68:
				if(snake_direction != right)
					snake_direction = left;
				break;
			default:
				break;
		}
	}
} /* snake_input() */

/* GAME STATE:
 *   Y ^
 *   0 |       @
 *     |       x
 *     |  ******
 *     |  *
 * max +------------>
 *     0        max X
 *
 * BODY:
 * [head, ..., tail]
 */

void shift_snake(void) {
	/* shifts all snake body parts down */
	int i;
	for(i = snake_len - 1; i > 0; i--) {
		snake_body[i][X] = snake_body[i-1][X];
		snake_body[i][Y] = snake_body[i-1][Y];
	}

	/* set top of head */
	snake_body[0][X] = snake_head[X];
	snake_body[0][Y] = snake_head[Y];
} /* shift_snake() */

int in_snake(int x, int y) {
	int i;
	for(i = 0; i < snake_len; i++)
		if(snake_body[i][X] == x && snake_body[i][Y] == y)
			return true;

	return false;
}

void snake_update(void) {
	/* first, update snake head */
	switch(snake_direction) {
		case up:
			snake_head[Y]--;
			break;
		case down:
			snake_head[Y]++;
			break;
		case left:
			snake_head[X]--;
			break;
		case right:
			snake_head[X]++;
			break;
	}

	/* check if snake intersects body -- and quit appropriately */
	int i;
	for(i = 1; i < snake_len; i++) {
		if(snake_head[X] == snake_body[i][X] &&
		   snake_head[Y] == snake_body[i][Y]) {
			quit = true;
			return;
		}
	}


	/* check if snake is outside screen */
	if(snake_head[Y] < 0 || snake_head[Y] >= SCREEN_HEIGHT ||
	   snake_head[X] < 0 || snake_head[X] >= SCREEN_WIDTH) {
		if(SNAKE_WRAP) {
			/* if wrapping is enabled, wrap the screen */
			snake_head[X] = (snake_head[X] < 0) ?  SCREEN_WIDTH - 1 : snake_head[X];
			snake_head[Y] = (snake_head[Y] < 0) ? SCREEN_HEIGHT - 1 : snake_head[Y];

			snake_head[X] %= SCREEN_WIDTH;
			snake_head[Y] %= SCREEN_HEIGHT;
		} else {
			/* otherwise, game over */
			quit = true;
			return;
		}

	}

	/* check if food has been eaten */
	if(snake_head[X] == food[X] &&
	   snake_head[Y] == food[Y]) {
		/* iterate score */
		score++;

		/* reallocate body */
		snake_body = realloc(snake_body, (snake_len + 1) * sizeof(int *));
		snake_body[snake_len] = malloc(2 * sizeof(int));

		/* set body to defaults */
		snake_body[snake_len][X] = -1;
		snake_body[snake_len][Y] = -1;

		/* update length of snake */
		snake_len++;

		/* regenerate food */
		do {
			food[X] = snake_rand(0,  SCREEN_WIDTH - 1);
			food[Y] = snake_rand(0, SCREEN_HEIGHT - 1);
		} while(in_snake(food[X], food[Y]));
	}

	shift_snake();

	/* -- update string representation of game -- */

	/* clear */
	int x, y;
	for(y = 0; y < SCREEN_HEIGHT; y++)
		for(x = 0; x < SCREEN_WIDTH; x++)
			game_state[y][x] = ' ';

	/* add snake body */
	for(i = 1; i < snake_len; i++) {
		x = snake_body[i][X];
		y = snake_body[i][Y];

		/* ignore 'fake' positions */
		if(x < 0 || y < 0)
			continue;

		game_state[y][x] = SNAKE_BODY;
	}

	/* add food */
	x = food[X];
	y = food[Y];
	game_state[y][x] = FOOD;

	/* add snake head */
	x = snake_head[X];
	y = snake_head[Y];
	game_state[y][x] = SNAKE_HEAD;
} /* snake_update() */

void snake_redraw(void) {
	/* move virtual cursor to top left */
	printf("\x1b[H");
	fflush(stdout);

	int i;
	for(i = 0; i < SCREEN_HEIGHT; i++)
		printf("%s\n", game_state[i]);

	printf("\n" SCORE_FORMAT "\n", score);
} /* snake_redraw() */

int main(void) {
	/* clear screen */
	printf("\x1b[H\x1b[2J");
	fflush(stdout);

	/* intialise snake */
	snake_init();
	snake_redraw();

	/* main snake loop */
	while(!quit) {
		snake_input();  /* get input */
		snake_update(); /* update game state */
		snake_redraw(); /* redraw screen */
	}

	/* game over */
	printf(ANSI_RED "Game Over!\n" ANSI_CLEAR);

	/* free memory */
	int i;
	for(i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
		free(game_state[i]);
	free(game_state);

	for(i = 0; i < snake_len; i++)
		free(snake_body[i]);
	free(snake_body);
	return 0;
}
