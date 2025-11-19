/*
 * snake.c
 */

/* Includes ------------------------------------------------------------------*/
#include "snake.h"
#include "button.h"
#include "lcd.h"
#include "led_7seg.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Enums ---------------------------------------------------------------------*/
typedef enum DIRECTION { UP, DOWN, LEFT, RIGHT, STOP } E_DIRECTION;

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
static void start_screen(void);
static void game_over_screen(void);

/* button wrappers (adjust implementation to your button driver) */
uint8_t is_button_up(void);
uint8_t is_button_down(void);
uint8_t is_button_left(void);
uint8_t is_button_right(void);
uint8_t is_button_select(void);  // e.g., to start game

/* Public Functions ----------------------------------------------------------*/
void game_init(void) {
  srand((unsigned)time(NULL));
  
  start_screen(); // Show start screen
  while (!is_button_select()); // Wait until user presses "select/start"
  
  /* Clear screen & initialize game */
  lcd_clear(BACKGROUND_COLOR);
  draw_frame();

  score = 0;
  draw_score();

  /* Initialize snake mid-board, horizontal to the right */
  snake_length = 3;
  uint8_t mid_i = MAZE_ROW_N / 2;
  uint8_t mid_j = MAZE_COLUMN_N / 2 - 1;
  for (uint16_t k = 0; k < snake_length; ++k) {
    snake[k].i = mid_i;
    snake[k].j = mid_j - (snake_length - 1) + k;
    draw_cell(snake[k].i, snake[k].j, SNAKE_COLOR);
  }

  snake_dir = RIGHT;

  /* Place first food */
  place_food();
  draw_cell(food.i, food.j, FOOD_COLOR);
}

void game_process(void) {
  static uint8_t counter_game = 0;
  counter_game = (counter_game + 1) % 5; /* slow down ticks */

  /* read direction first (non-blocking) */
  if (is_button_up() && snake_dir != DOWN) snake_dir = UP;
  else if (is_button_down() && snake_dir != UP) snake_dir = DOWN;
  else if (is_button_left() && snake_dir != RIGHT) snake_dir = LEFT;
  else if (is_button_right() && snake_dir != LEFT) snake_dir = RIGHT;

  if (counter_game != 0) return; // wait for next tick

  /* compute new head position */
  S_CELL_POS new_head = snake[snake_length - 1]; /* current head */
  switch (snake_dir) {
    case UP:    if (new_head.i == 0) { game_over_screen(); return; } new_head.i--; break;
    case DOWN:  if (new_head.i >= MAZE_ROW_N - 1) { game_over_screen(); return; } new_head.i++; break;
    case LEFT:  if (new_head.j == 0) { game_over_screen(); return; } new_head.j--; break;
    case RIGHT: if (new_head.j >= MAZE_COLUMN_N - 1) { game_over_screen(); return; } new_head.j++; break;
    case STOP: return;
  }

  /* check self collision */
  if (snake_cell_at(new_head.i, new_head.j)) {
    game_over_screen();
    return;
  }

  uint8_t ate_food = cell_equal(&new_head, &food);

  if (!ate_food) {
    clear_cell(snake[0].i, snake[0].j);
    for (uint16_t k = 0; k < snake_length - 1; ++k) snake[k] = snake[k + 1];
    snake[snake_length - 1] = new_head;
  } else {
    if (snake_length < MAX_CELLS) {
      snake[snake_length] = new_head;
      ++snake_length;
      score += 10;
      draw_score();
    } else {
      lcd_show_string(20, 250, "You win!", TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
      game_over_screen();
      return;
    }
    if (snake_length < MAX_CELLS) place_food();
  }

  draw_cell(new_head.i, new_head.j, SNAKE_COLOR);
  draw_cell(food.i, food.j, FOOD_COLOR);
}

/* Private helper implementations -------------------------------------------*/
static void draw_frame(void) {
  lcd_draw_rectangle(MAZE_TOP_BORDER, MAZE_LEFT_BORDER, MAZE_BOTTOM_BORDER, MAZE_RIGHT_BORDER, GRID_COLOR);
  for (uint8_t r = 0; r < MAZE_ROW_N; ++r)
    for (uint8_t c = 0; c < MAZE_COLUMN_N; ++c)
      clear_cell(r, c);
}

static inline int cell_pixel_x(uint8_t j) { return MAZE_LEFT_BORDER + j * MAZE_CELL_WIDTH; }
static inline int cell_pixel_y(uint8_t i) { return MAZE_TOP_BORDER + i * MAZE_CELL_WIDTH; }

static void draw_cell(uint8_t i, uint8_t j, uint16_t color) {
  lcd_fill(cell_pixel_y(i), cell_pixel_x(j), cell_pixel_y(i)+MAZE_CELL_WIDTH-1, cell_pixel_x(j)+MAZE_CELL_WIDTH-1, color);
  lcd_draw_rectangle(cell_pixel_y(i), cell_pixel_x(j), cell_pixel_y(i)+MAZE_CELL_WIDTH-1, cell_pixel_x(j)+MAZE_CELL_WIDTH-1, GRID_COLOR);
}

static void clear_cell(uint8_t i, uint8_t j) { draw_cell(i, j, BACKGROUND_COLOR); }

static void place_food(void) {
  if (snake_length >= MAX_CELLS) return;
  uint16_t free_count = 0;
  S_CELL_POS free_cells[MAX_CELLS];
  for (uint8_t r = 0; r < MAZE_ROW_N; ++r)
    for (uint8_t c = 0; c < MAZE_COLUMN_N; ++c)
      if (!snake_cell_at(r, c)) {
        free_cells[free_count].i = r;
        free_cells[free_count].j = c;
        free_count++;
      }
  if (free_count == 0) return;
  food = free_cells[rand() % free_count];
}

static uint8_t cell_equal(const S_CELL_POS *a, const S_CELL_POS *b) { return (a->i == b->i && a->j == b->j); }

static uint8_t snake_cell_at(uint8_t i, uint8_t j) { for (uint16_t k=0;k<snake_length;k++) if (snake[k].i==i && snake[k].j==j) return 1; return 0; }

static void draw_score(void) {
  char buf[32];
  snprintf(buf, sizeof(buf), "Score: %d  Len:%d", score, (int)snake_length);
  lcd_draw_rectangle(230, 20, 270, 200, BACKGROUND_COLOR);
  lcd_show_string(20, 250, buf, TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
}

static void restart_game(void) {
  lcd_clear(BACKGROUND_COLOR);
  draw_frame();
  snake_length = 3;
  score = 0;
  uint8_t mid_i = MAZE_ROW_N / 2;
  uint8_t mid_j = MAZE_COLUMN_N / 2 - 1;
  for (uint16_t k=0;k<snake_length;k++) {
    snake[k].i = mid_i;
    snake[k].j = mid_j - (snake_length - 1) + k;
    draw_cell(snake[k].i, snake[k].j, SNAKE_COLOR);
  }
  snake_dir = RIGHT;
  place_food();
  draw_cell(food.i, food.j, FOOD_COLOR);
  draw_score();
}

static void start_screen(void) {
  lcd_clear(BACKGROUND_COLOR);
  lcd_show_string(20, 80, "==== SNAKE GAME ====", TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
  lcd_show_string(20, 120, "Use buttons to move", TEXT_COLOR, BACKGROUND_COLOR, 12, 0);
  lcd_show_string(20, 150, "Press SELECT to start", TEXT_COLOR, BACKGROUND_COLOR, 12, 0);
}

static void game_over_screen(void) {
  lcd_clear(BACKGROUND_COLOR);
  char buf[32];
  snprintf(buf, sizeof(buf), "GAME OVER");
  lcd_show_string(40, 100, buf, TEXT_COLOR, BACKGROUND_COLOR, 20, 0);
  snprintf(buf, sizeof(buf), "Total Score: %d", score);
  lcd_show_string(40, 140, buf, TEXT_COLOR, BACKGROUND_COLOR, 16, 0);
  lcd_show_string(20, 200, "Press SELECT to restart", TEXT_COLOR, BACKGROUND_COLOR, 12, 0);

  /* Wait for restart button press */
  while (!is_button_select());
  restart_game();
}

/* Button wrapper implementations (adapt to your driver) */
uint8_t is_button_up(void) { return button_count[1] > 0; }
uint8_t is_button_down(void) { return button_count[9] > 0; }
uint8_t is_button_left(void) { return button_count[4] > 0; }
uint8_t is_button_right(void) { return button_count[6] > 0; }
uint8_t is_button_select(void) { return button_count[0] > 0; }  // choose a free button

