/*
 * snake.h
 */

#ifndef INC_SNAKE_H_
#define INC_SNAKE_H_

/* Includes */
#include <stdint.h>

/* Reuse maze constants from pacman template */
#define MAZE_COLUMN_N      		10
#define MAZE_ROW_N         		10
#define MAZE_CELL_WIDTH    		20

#define MAZE_TOP_BORDER			20
#define MAZE_BOTTOM_BORDER 		220
#define MAZE_LEFT_BORDER		20
#define MAZE_RIGHT_BORDER		220

/* Colors (use your platform color defines) */
#define BACKGROUND_COLOR		WHITE
#define SNAKE_COLOR				YELLOW
#define FOOD_COLOR				GREEN
#define GRID_COLOR				BLACK
#define TEXT_COLOR				BLACK

/* Functions */
void game_init(void);
void game_process(void);

#endif /* INC_SNAKE_H_ */

