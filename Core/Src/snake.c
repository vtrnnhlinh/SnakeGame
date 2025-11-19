/*
 * snake.c
 */

/* Includes ------------------------------------------------------------------*/
#include "snake.h"
#include "button.h"
#include "lcd.h"
#include "led_7seg.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

/* Enums ---------------------------------------------------------------------*/
typedef enum DIRECTION {
	UP, DOWN, LEFT, RIGHT, STOP
} E_DIRECTION;

/* Structs -------------------------------------------------------------------*/
typedef struct CELL_POS {
	uint8_t i; /* row 0..MAZE_ROW_N-1 */
	uint8_t j; /* col 0..MAZE_COLUMN_N-1 */
} S_CELL_POS;

/* Private Objects -----------------------------------------------------------*/
#define MAX_CELLS (MAZE_COLUMN_N * MAZE_ROW_N)

static S_CELL_POS snake[MAX_CELLS];
static uint16_t snake_length;
static E_DIRECTION snake_dir;
static S_CELL_POS food;
static int score;

/* helper prototypes */
static void draw_frame(void);
static void draw_cell(uint8_t i, uint8_t j, uint16_t color);
static void clear_cell(uint8_t i, uint8_t j);
static void place_food(void);
static uint8_t cell_equal(const S_CELL_POS *a, const S_CELL_POS *b);
static uint8_t snake_cell_at(uint8_t i, uint8_t j);
static void draw_score(void);
static void restart_game(void);

/* button wrappers (adjust implementation to your button driver) */
uint8_t is_button_up(void);
uint8_t is_button_down(void);
uint8_t is_button_left(void);
uint8_t is_button_right(void);

/* Public Functions ----------------------------------------------------------*/
void game_init(void) {
	/* Seed RNG for food placement */
	srand((unsigned)time(NULL));

	/* Background & frame */
	lcd_clear(BACKGROUND_COLOR);
	draw_frame();

	/* Initialize score text */
	score = 0;
	lcd_show_string(20, 230, "Snake (use buttons)", TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
	draw_score();

	/* Initialize snake mid-board, horizontal to the right */
	snake_length = 3;
	uint8_t mid_i = MAZE_ROW_N / 2;
	uint8_t mid_j = MAZE_COLUMN_N / 2 - 1;

	for (uint16_t k = 0; k < snake_length; ++k) {
		snake[k].i = mid_i;
		snake[k].j = mid_j - (snake_length - 1) + k; /* left -> right */
	}

	snake_dir = RIGHT;

	/* Draw initial snake */
	for (uint16_t k = 0; k < snake_length; ++k) {
		draw_cell(snake[k].i, snake[k].j, SNAKE_COLOR);
	}

	/* Place first food */
	place_food();
	draw_cell(food.i, food.j, FOOD_COLOR);
}

void game_process(void) {
	static uint8_t counter_game = 0;
	counter_game = (counter_game + 1) % 5; /* slow down ticks */

	/* read direction first (non-blocking) */
	if (is_button_up() && snake_dir != DOWN) {
		snake_dir = UP;
	} else if (is_button_down() && snake_dir != UP) {
		snake_dir = DOWN;
	} else if (is_button_left() && snake_dir != RIGHT) {
		snake_dir = LEFT;
	} else if (is_button_right() && snake_dir != LEFT) {
		snake_dir = RIGHT;
	}

	if (counter_game == 0) {
		/* compute new head position */
		S_CELL_POS new_head = snake[snake_length - 1]; /* current head */

		switch (snake_dir) {
		case UP:
			if (new_head.i == 0) { /* hits top wall */
				restart_game();
				return;
			}
			new_head.i -= 1;
			break;
		case DOWN:
			if (new_head.i >= MAZE_ROW_N - 1) {
				restart_game();
				return;
			}
			new_head.i += 1;
			break;
		case LEFT:
			if (new_head.j == 0) {
				restart_game();
				return;
			}
			new_head.j -= 1;
			break;
		case RIGHT:
			if (new_head.j >= MAZE_COLUMN_N - 1) {
				restart_game();
				return;
			}
			new_head.j += 1;
			break;
		case STOP:
			/* do nothing */
			return;
		default:
			return;
		}

		/* check self collision */
		if (snake_cell_at(new_head.i, new_head.j)) {
			restart_game();
			return;
		}

		uint8_t ate_food = cell_equal(&new_head, &food);

		/* if not eating, remove tail visually and shift positions */
		if (!ate_food) {
			/* clear tail from screen */
			clear_cell(snake[0].i, snake[0].j);

			/* shift left: drop tail */
			for (uint16_t k = 0; k < snake_length - 1; ++k) {
				snake[k] = snake[k + 1];
			}
			/* append new head */
			snake[snake_length - 1] = new_head;
		} else {
			/* grow: append new head without removing tail */
			if (snake_length < MAX_CELLS) {
				snake[snake_length] = new_head;
				++snake_length;
				score += 10;
				draw_score();
			} else {
				/* filled entire board -> win */
				lcd_show_string(20, 250, "You win!", TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
				/* restart after short delay (or immediate) */
				restart_game();
				return;
			}

			/* place new food (if board not full) */
			if (snake_length < MAX_CELLS) {
				place_food();
			}
		}

		/* draw new head */
		draw_cell(new_head.i, new_head.j, SNAKE_COLOR);

		/* draw food (in case it was overwritten) */
		draw_cell(food.i, food.j, FOOD_COLOR);
	}
}

/* Private helper implementations -------------------------------------------*/

static void draw_frame(void) {
	/* draw outer rectangle frame (same as pacman template) */
	lcd_draw_rectangle(MAZE_TOP_BORDER, MAZE_LEFT_BORDER, MAZE_BOTTOM_BORDER, MAZE_RIGHT_BORDER, GRID_COLOR);

	/* optionally draw grid lines */
	for (uint8_t r = 0; r < MAZE_ROW_N; ++r) {
		for (uint8_t c = 0; c < MAZE_COLUMN_N; ++c) {
			/* draw empty cell background */
			clear_cell(r, c);
		}
	}
}

static inline int cell_pixel_x(uint8_t j) {
	/* left-top pixel x for column j */
	return MAZE_LEFT_BORDER + j * MAZE_CELL_WIDTH;
}

static inline int cell_pixel_y(uint8_t i) {
	/* left-top pixel y for row i */
	return MAZE_TOP_BORDER + i * MAZE_CELL_WIDTH;
}

static void draw_cell(uint8_t i, uint8_t j, uint16_t color) {
	int x = cell_pixel_x(j);
	int y = cell_pixel_y(i);
	/* Fill the cell area. Adjust to your lcd API if different. */
	/* We assume a function lcd_fill_rectangle(x, y, x2, y2, color) exists.
	   If your API name differs, change it here. */
	lcd_fill_rectangle(y, x, y + MAZE_CELL_WIDTH - 1, x + MAZE_CELL_WIDTH - 1, color);
	/* optional border */
	lcd_draw_rectangle(cell_pixel_y(i), cell_pixel_x(j), cell_pixel_y(i) + MAZE_CELL_WIDTH - 1,
	                   cell_pixel_x(j) + MAZE_CELL_WIDTH - 1, GRID_COLOR);
}

static void clear_cell(uint8_t i, uint8_t j) {
	/* fill with background; draws a blank cell */
	draw_cell(i, j, BACKGROUND_COLOR);
}

static void place_food(void) {
	/* Choose a random empty cell */
	if (snake_length >= MAX_CELLS) {
		/* board full */
		return;
	}

	uint16_t free_count = 0;
	/* we can collect free cell indices to pick uniformly */
	S_CELL_POS free_cells[MAX_CELLS];
	for (uint8_t r = 0; r < MAZE_ROW_N; ++r) {
		for (uint8_t c = 0; c < MAZE_COLUMN_N; ++c) {
			if (!snake_cell_at(r, c)) {
				free_cells[free_count].i = r;
				free_cells[free_count].j = c;
				free_count++;
			}
		}
	}

	if (free_count == 0) {
		/* no free cell */
		return;
	}
	uint16_t idx = rand() % free_count;
	food = free_cells[idx];
}

static uint8_t cell_equal(const S_CELL_POS *a, const S_CELL_POS *b) {
	return (a->i == b->i && a->j == b->j) ? 1 : 0;
}

static uint8_t snake_cell_at(uint8_t i, uint8_t j) {
	for (uint16_t k = 0; k < snake_length; ++k) {
		if (snake[k].i == i && snake[k].j == j) return 1;
	}
	return 0;
}

static void draw_score(void) {
	char buf[32];
	snprintf(buf, sizeof(buf), "Score: %d  Len:%d", score, (int)snake_length);
	/* clear previous display area -> redraw */
	lcd_fill_rectangle(230, 20, 270, 200, BACKGROUND_COLOR); /* adapt coords if needed */
	lcd_show_string(20, 250, buf, TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
}

static void restart_game(void) {
	/* small restart procedure: clear maze and re-init */
	lcd_show_string(20, 260, "Game over. Restarting...", TEXT_COLOR, BACKGROUND_COLOR, 12, 0);
	/* tiny delay if your platform supports (not provided here) */
	/* Reset state */
	score = 0;
	memset(snake, 0, sizeof(snake));
	snake_length = 3;
	uint8_t mid_i = MAZE_ROW_N / 2;
	uint8_t mid_j = MAZE_COLUMN_N / 2 - 1;

	for (uint16_t k = 0; k < snake_length; ++k) {
		snake[k].i = mid_i;
		snake[k].j = mid_j - (snake_length - 1) + k;
	}

	snake_dir = RIGHT;

	/* redraw entire board */
	lcd_clear(BACKGROUND_COLOR);
	draw_frame();
	for (uint16_t k = 0; k < snake_length; ++k) {
		draw_cell(snake[k].i, snake[k].j, SNAKE_COLOR);
	}
	place_food();
	draw_cell(food.i, food.j, FOOD_COLOR);
	draw_score();
}
uint8_t is_button_up(void) {
	if (button_count[1] > 0) {
		return 1;
	}
	return 0;
}

uint8_t is_button_down(void) {
	if (button_count[9] > 0) {
		return 1;
	}
	return 0;
}

uint8_t is_button_left(void) {
	if (button_count[4] > 0) {
		return 1;
	}
	return 0;
}

uint8_t is_button_right(void) {
	if (button_count[6] > 0) {
		return 1;
	}
	return 0;
}

