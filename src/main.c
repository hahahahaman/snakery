#include "wasm4.h"
#include <stdint.h>
#include <stdlib.h>

enum bool { FALSE, TRUE };
enum game_state { MAIN_MENU, LEVEL };
enum level_type { PUZZLE, TEXT };

int16_t wrap(int16_t x, int16_t hi){
    if(0 <= x && x < hi) return x;
    if(x < 0) return hi + (x % hi);
    return x % hi;
}

typedef struct {
    int16_t x, y;
} point_t;

typedef struct {
    point_t *body; // an array of points
    point_t direction;
    uint16_t length;
    uint8_t dead;
} normal_snake_t;

typedef struct {
    char **tiles;
    uint8_t w, h;
    uint8_t type;
} level_t;

typedef struct {
    uint8_t level;
    /* normal_snake_t snake; */
    uint8_t move_history[400];
} save_data_t;

// fruit
#define fruitWidth 8
#define fruitHeight 8
#define fruitFlags BLIT_2BPP
const uint8_t fruit_sprite[16] = {
0x00,0xa0,0x02,0x00,0x0e,0xf0,0x36,0x5c,0xd6,0x57,0xd5,0x57,0x35,0x5c,0x0f,0xf0
};

unsigned int frame_count = 0;
normal_snake_t snake;
point_t fruit;
uint8_t prev_key_state = 0, prev_mouse_state = 0, has_moved = 0, paused = 0,
    dead = 0,
    state, prev_state,
    level = 0, level0_score = 0;

uint8_t move_history[400];

void start() {
    state = MAIN_MENU;
    /* state = LEVEL; */
    /* level = 1; */

    // EN4 - https://lospec.com/palette-list/en4
    /* PALETTE[0] = 0xfbf7f3; */
    /* PALETTE[1] = 0xe5b083; */
    /* PALETTE[2] = 0x426e5d; */
    /* PALETTE[3] = 0x20283d; */

    // lava-gb - https://lospec.com/palette-list/lava-gb
    PALETTE[0] = 0x4a2480;
    PALETTE[1] = 0xff8e80;
    PALETTE[2] = 0xc53a9d;
    PALETTE[3] = 0x051f39;


    /* tracef("%d %d %d \n", wrap(-1, 10), wrap(0, 11), wrap(19, 15)); */
}

void level_update () {
    const uint8_t mouse = *MOUSE_BUTTONS;
    const uint8_t mouse_just_pressed =
        *MOUSE_BUTTONS & (*MOUSE_BUTTONS ^ prev_mouse_state);
    const int16_t mouse_x = *MOUSE_X, mouse_y = *MOUSE_Y;

    if(mouse_just_pressed & MOUSE_LEFT) {
        /* trace("mouse left"); */
        /* tracef("%d %d\n", mouse_x, mouse_y); */
        /* if(paused == 1) paused = 0; */
        /* else paused = 1; */

    }
}

void update () {
    // xors the previous state so we don't get repeating
    const uint8_t just_pressed = *GAMEPAD1 & (*GAMEPAD1 ^ prev_key_state);

    frame_count++;
    srand(frame_count);

    if (state == MAIN_MENU) {
        *DRAW_COLORS = 0x0024;
        text("snakery", 50, 50);
        *DRAW_COLORS = 0x0002;
        text("arrow keys to move", 8, 100);

        if ((frame_count % 60) < 30) {
            *DRAW_COLORS = 0x0001;
        } else {
            *DRAW_COLORS = 0x0002;
        }
        text("press X to start", 18, 130);

        if (just_pressed & BUTTON_1){ // x just pressed
            /* trace("button 1"); */
            /* prev_state = MAIN_MENU; */
            state = LEVEL;

            /*** initialize level 0 ***/
            // create snake
            if(snake.body != NULL) {
                free(snake.body);
                snake.body = NULL;
            }
            snake.length = 0;

            point_t *body = realloc(snake.body, sizeof(point_t) * 3);
            if(body) {
                snake.body = body;
                snake.body[0] = (point_t){2,0};
                snake.body[1] = (point_t){1,0};
                snake.body[2] = (point_t){0,0};
                snake.length = 3;

                snake.direction = (point_t){1, 0};
            }

            // make fruit
            fruit.x = rand()%20;
            fruit.y = rand()%20;
        }
        if (just_pressed & BUTTON_2){ // z just pressed
            trace("button 2");
        }
    } else if(state == LEVEL) {
        if (level == 0) {

            /*** input ***/
            if (just_pressed & BUTTON_UP){
                if(snake.direction.y == 0 &&
                   (snake.length > 1 &&
                    wrap(snake.body[0].y-1, 20) != snake.body[1].y)){
                    snake.direction = (point_t){0,-1};
                }
            }

            if (just_pressed & BUTTON_DOWN) {
                if(snake.direction.y == 0 &&
                   (snake.length > 1 &&
                    wrap(snake.body[0].y+1, 20) != snake.body[1].y)){
                    snake.direction = (point_t){0,1};
                }
            }
            if (just_pressed & BUTTON_LEFT) {
                if(snake.direction.x == 0 &&
                   (snake.length > 1 &&
                    wrap(snake.body[0].x-1, 20) != snake.body[1].x)){
                    snake.direction = (point_t){-1, 0};
                }
            }
            if (just_pressed & BUTTON_RIGHT) {
                if(snake.direction.x == 0 &&
                   (snake.length > 1 &&
                    wrap(snake.body[0].x+1, 20) != snake.body[1].x)){
                    snake.direction = (point_t){1, 0};
                }
            }

            /*** move ***/
            if (frame_count % 5 == 0) {
                has_moved = 1;
                point_t last_body_part = snake.body[snake.length-1];
                for(int i = snake.length-1; i> 0; i--){
                    snake.body[i] = snake.body[i-1];
                }

                snake.body[0].x = wrap(snake.body[0].x + snake.direction.x, 20);
                snake.body[0].y = wrap(snake.body[0].y + snake.direction.y, 20);

                // fruit collision
                if(snake.body[0].x == fruit.x && snake.body[0].y == fruit.y){
                    level0_score++;

                    point_t *body = realloc(snake.body,
                                            sizeof(point_t) * (snake.length+1));
                    if(body) {
                        snake.body = body;
                        snake.length++;
                        snake.body[snake.length-1] = last_body_part;
                    }
                    int fruit_collides_snake;

                    do {
                        fruit.x = rand()%20;
                        fruit.y = rand()%20;
                        fruit_collides_snake = 0;

                        for(int i = 0; i < snake.length; i++){
                            if(fruit.x == snake.body[i].x &&
                               fruit.y == snake.body[i].y) {
                                fruit_collides_snake = 1;
                            }
                        }
                    } while(fruit_collides_snake);
                }

                /*
                // body collision
                int head_body_collision = 0;
                for(int i = 1; i< snake.length-1; i++){
                    if(snake.body[0].x == snake.body[i].x &&
                       snake.body[0].y == snake.body[i].y) {
                        head_body_collision = 1;
                        break;
                    }
                }

                if(head_body_collision){
                    dead = 1;
                }
                */
            }

            /*** draw ***/
            *DRAW_COLORS = 0x0043;
            // draw body
            // size_t is an unsigned int of at least 16 bits
            for(size_t i = 0; i < snake.length; i++){
                // screen is 160x160 pixels
                // each snake body part is 8x8, so we have 20x20 blocks to fill the screen
                rect(snake.body[i].x*8, snake.body[i].y*8, 8, 8);
            }

            // draw head
            *DRAW_COLORS = 0x0004;
            rect(snake.body[0].x*8, snake.body[0].y*8, 8,8);

            // draw fruit
            *DRAW_COLORS = 0x4320;
            blit(fruit_sprite, fruit.x*8, fruit.y*8, 8, 8, BLIT_2BPP);

            /*** handle next level start **/
            if (level0_score == 2 ){
                level = 1;
            }
        } else if (level == 1) {

            *DRAW_COLORS = 0x4320;
            text("something is up\n with this fruit", 10, 50);

            if ((frame_count % 60) < 30) {
                *DRAW_COLORS = 0x0001;
            } else {
                *DRAW_COLORS = 0x0002;
            }
            text("X to continue", 30, 130);

        } else if (level == 2) {

        } else {
            state = MAIN_MENU;
        }

    }

    prev_key_state = *GAMEPAD1;
    prev_mouse_state = *MOUSE_BUTTONS;
}
