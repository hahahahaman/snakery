/*
** In the C and C++ programming languages, #pragma once is a non-standard but
** widely supported preprocessor directive designed to cause the current source
** file to be included only once in a single compilation. Thus, #pragma once
** serves the same purpose as #include guards, but with several advantages,
** including: less code, avoiding name clashes, and improved compile speed.
*/

#include "wasm4.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int16_t x;
    int16_t y;
} point_t;

typedef struct {
    point_t *body; // an array of points
    point_t direction;
    uint16_t length;
    uint16_t alive;
} snake_t;

/*
void snake_create(snake_t *snake){
    if(snake->body != NULL) {
        free(snake->body);
        snake->body = NULL;
    }
    snake->length = 0;
}

void snake_push(snake_t *snake, point_t p){
    point_t *body = realloc(snake->body, sizeof(body) * (snake->length+1));
    if(body) {
        snake->body = body;
        snake->body[snake->length] = p;
        snake->length++;
    }
}

void snake_draw(snake_t *snake){
    *DRAW_COLORS = 0x0043;
    // size_t is an unsigned int of at least 16 bits
    for(size_t i = 0; i < snake->length; ++i){
        // screen is 160x160 pixels
        // each snake body part is 8x8, so we have 20x20 blocks to fill the screen
        rect(snake->body[i].x*8, snake->body[i].y*8, 8, 8);
    }
}
*/

/* const size_t screen_w, screen_h; */
unsigned int frame_count = 0;
snake_t snake;
point_t fruit;
uint8_t prev_state = 0, prev_mouse_state = 0;

void start() {
    // EN4 - https://lospec.com/palette-list/en4
    PALETTE[0] = 0xfbf7f3;
    PALETTE[1] = 0xe5b083;
    PALETTE[2] = 0x426e5d;
    PALETTE[3] = 0x20283d;

    // lava-gb - https://lospec.com/palette-list/lava-gb
    /* PALETTE[0] = 0x4a2480; */
    /* PALETTE[1] = 0xff8e80; */
    /* PALETTE[2] = 0xc53a9d; */
    /* PALETTE[3] = 0x051f39; */

    // create snake
    if(snake.body != NULL) {
        free(snake.body);
        snake.body = NULL;
    }
    snake.length = 0;

    point_t *body = realloc(snake.body, sizeof(body) * 3);
    if(body) {
        snake.body = body;
        snake.body[0] = (point_t){2,0};
        snake.body[1] = (point_t){1,0};
        snake.body[2] = (point_t){0,0};
        snake.length = 3;

        snake.direction = (point_t){1, 0};
    }
}

void update () {
    frame_count++;
    srand(frame_count);

    /*** input ***/
    // xors the previous state so we don't get repeating
    const uint8_t just_pressed = *GAMEPAD1 & (*GAMEPAD1 ^ prev_state);


    if (just_pressed & BUTTON_UP){
        if(snake.direction.y == 0)
            snake.direction = (point_t){0,-1};
    }

    if (just_pressed & BUTTON_DOWN) {
        if(snake.direction.y == 0)
            snake.direction = (point_t){0,1};
    }
    if (just_pressed & BUTTON_LEFT) {
        if(snake.direction.x == 0)
            snake.direction = (point_t){-1, 0};
    }
    if (just_pressed & BUTTON_RIGHT) {
        if(snake.direction.x == 0)
            snake.direction = (point_t){1, 0};
    }

    const uint8_t mouse = *MOUSE_BUTTONS;
    const uint8_t mouse_just_pressed = *MOUSE_BUTTONS & (*MOUSE_BUTTONS ^ prev_mouse_state);
    const int16_t mouse_x = *MOUSE_X, mouse_y = *MOUSE_Y;

    if(mouse_just_pressed & mouse & MOUSE_LEFT) {
        /* trace("mouse left"); */
        /* tracef("%d %d\n", mouse_x, mouse_y); */
    }

    prev_state = *GAMEPAD1;
    prev_mouse_state = *MOUSE_BUTTONS;

    /*** move ***/
    if (frame_count % 10 == 0) {
        for(int i = snake.length-1; i> 0; i--){
            snake.body[i] = snake.body[i-1];
        }

        snake.body[0].x = (snake.body[0].x + snake.direction.x) % 20;
        snake.body[0].y = (snake.body[0].y + snake.direction.y) % 20;

        if(snake.body[0].x < 0) snake.body[0].x = 19;
        if(snake.body[0].y < 0) snake.body[0].y = 19;
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
}
