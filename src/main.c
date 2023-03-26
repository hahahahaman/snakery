#include "wasm4.h"
#include <stdint.h>
#include <stdlib.h>
/* #include <math.h> */

// nice idea from programming language talk
// https://www.youtube.com/watch?v=TH9VCN6UkyQ&list=PLmV5I2fxaiCKfxMBrNsU1kgKJXD3PkyxO
typedef unsigned long long u64;
typedef signed long long s64;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;
typedef signed char s8;

enum bool { FALSE, TRUE };

/* typedef enum bool bool; */

enum game_state { MAIN_MENU, LEVEL };

u8 max_u8(u8 a, u8 b){
  return a > b ? a : b;
}

s16 wrap(s16 x, s16 hi){
  if(0 <= x && x < hi) return x;
  if(x < 0) return hi + (x % hi);
  return x % hi;
}

typedef struct {
  s16 x, y;
} point_t;

typedef struct {
  point_t *body; // an array of points
  point_t direction;
  u16 length;
} snake_t;

typedef struct {
  /* char **map; */
  u8 w, h;
  u8 tw, th; // tile width and height
  u16 max_moves;
} level_t;

typedef struct {
  point_t *fruits;
  u8 *eaten;
  u16 n;
} fruit_basket_t;

// TODO saving?
typedef struct {
  u8 current_level;
  /* snake_t snake; */
  u8 move_history[400];
} save_data_t;

typedef struct {
  point_t *prev_tail_position; // size is num_moves + 1

  u16 *fruit_eaten_on_move;
  point_t *fruit_eaten_position;

  u16 num_saved_positions;
  u16 num_moves;
  u8 num_fruit_eaten;
} history_t;

// level0_fruit
#define fruitWidth 8
#define fruitHeight 8
#define fruitFlags BLIT_2BPP
const u8 fruit_sprite[16] = {
0x00,0xa0,0x02,0x00,0x0e,0xf0,0x36,0x5c,0xd6,0x57,0xd5,0x57,0x35,0x5c,0x0f,0xf0
};

u32 frame_count = 0;
snake_t snake;
fruit_basket_t basket;
history_t history;

point_t level0_fruit;

u8 prev_key_state = 0, prev_mouse_state = 0, has_moved = 0, paused = 0,
  state, prev_state,
  current_level = 0,
  level0_score = 0,
  num_moves = 0;

u8 stuck = FALSE;

void add_fruit(fruit_basket_t *basket, point_t fruit){
  point_t *fruits = realloc(basket->fruits, sizeof(fruits)*(basket->n+1));
  u8 *eaten = realloc(basket->eaten, sizeof(eaten)*(basket->n+1));
  if(fruits && eaten){
    basket->fruits = fruits;
    basket->fruits[basket->n] = fruit;

    basket->eaten = eaten;
    basket->eaten[basket->n] = FALSE;

    basket->n++;
  }
}

void free_fruit_basket(fruit_basket_t *basket) {
  if(basket->fruits != NULL) {
    free(basket->fruits);
    basket->fruits = NULL;
  }
  if (basket->eaten != NULL) {
    free(basket->eaten);
    basket->eaten = NULL;
  }
  basket->n = 0;
}

void free_snake(snake_t *snake) {
  if(snake->body != NULL) {
    free(snake->body);
    snake->body = NULL;
  }
  snake->length = 0;
  snake->direction = (point_t){0,0};
  stuck = FALSE;
}

void init_snake(snake_t *snake, u16 length) {
  free_snake(snake);

  point_t *body = realloc(snake->body, sizeof(point_t) * length);
  if(body == NULL) {
    trace("init_snake failed to realloc");
    // exit(0);
  } else {
    snake->body = body;
    snake->length = length;
  }
}

void add_body_part(snake_t *snake, point_t body_part) {
  point_t *body = (point_t *) realloc(snake->body,
                                      sizeof(body) * (snake->length+1));
  if(body == NULL) {
    trace("add_body_part failed to realloc");
    /* exit(0); */
  } else {
    snake->body = body;
    snake->body[snake->length] = body_part;
    snake->length++;
  }
}

void free_history(history_t* history){
  if(history->prev_tail_position != NULL) {
    free(history->prev_tail_position);
    free(history->fruit_eaten_on_move);
    free(history->fruit_eaten_position);

    history->prev_tail_position = NULL;
    history->fruit_eaten_on_move = NULL;
    history->fruit_eaten_position = NULL;
  }
  history->num_saved_positions = 0;
  history->num_moves = 0;
  history->num_fruit_eaten = 0;
}

void init_history(history_t *history, snake_t *snake){
  free_history(history);

  history->num_saved_positions = 1;
  point_t *prev_tail_pos = realloc(history->prev_tail_position,
                                   sizeof(point_t) * history->num_saved_positions);

  if(prev_tail_pos == NULL) {
    trace("prev_tail_pos failed to realloc");
  } else {
    history->prev_tail_position = prev_tail_pos;
    history->prev_tail_position[0] = snake->body[snake->length-1];
  }
}

void add_history(history_t *history, snake_t *snake, u8 fruit_eaten,
                 point_t fruit_position){
  history->num_saved_positions++;
  history->num_moves++;

  point_t *prev_tail_pos =
    realloc(history->prev_tail_position,
            sizeof(point_t) * history->num_saved_positions);

  if(prev_tail_pos == NULL) {
    trace("prev_tail_pos failed to realloc");
  } else {
    history->prev_tail_position = prev_tail_pos;
    history->prev_tail_position[history->num_saved_positions-1] =
      snake->body[snake->length-1];
  }

  if(fruit_eaten) {
    history->num_fruit_eaten++;

    u16 *fruit_eaten_on_move = realloc(history->fruit_eaten_on_move,
                                       sizeof(u16) * history->num_fruit_eaten);

    if(fruit_eaten_on_move == NULL) {
      trace("fruit_eaten_on_move failed to realloc");
    } else {
      history->fruit_eaten_on_move = fruit_eaten_on_move;
      history->fruit_eaten_on_move[history->num_fruit_eaten-1] =
        history->num_moves;
    }

    point_t *fruit_eaten_position =
      realloc(history->fruit_eaten_position,
              sizeof(point_t) * history->num_fruit_eaten);

    if(fruit_eaten_position == NULL) {
      trace("fruit_eaten_position failed to realloc");
    } else {
      history->fruit_eaten_position = fruit_eaten_position;
      history->fruit_eaten_position[history->num_fruit_eaten-1] = fruit_position;
    }
  }
}

void init_level(level_t *level, char map[][level->w], snake_t *snake,
                fruit_basket_t *basket, history_t *history) {

  free_snake(snake);
  free_fruit_basket(basket);
  free_history(history);

  u16 snake_len = 0;

  // map
  for(u8 row = 0; row < level->h; row++){
    for(u8 col = 0; col < level->w; col++) {
      char c = map[row][col];

      // snake body parts
      if('A' <= c && c <= 'Z') {
        snake_len++;
      }
    }
  }

  init_snake(snake, snake_len);

  for(s16 row = 0; row < level->h; row++){
    for(s16 col = 0; col < level->w; col++) {
      char c = map[row][col];

      if('A' <= c && c <= 'Z') { // snake body parts
        /* add_body_part(&snake, (point_t){col,row}); */
        snake->body[c-'A'] = (point_t){col,row};
      }
      else if(c == '$') { // add the fruit
        add_fruit(basket, (point_t){col,row});
      }
    }
  }

  init_history(history, snake);
}

void undo(history_t *history, snake_t *snake, fruit_basket_t *basket){
  tracef("undo");
  u16 saved_pos = history->num_saved_positions, num_moves = history->num_moves;
  u8 fruit_eaten = history->num_fruit_eaten;

  // nothing to undo
  if (!num_moves) return;

  if(snake->length > 1) {
    // snake move each body part back
    for(u16 i = 0; i < snake->length-1; i++)
      snake->body[i] = snake->body[i+1];
  }

  if (fruit_eaten > 0 &&
      history->fruit_eaten_on_move[fruit_eaten-1] == num_moves) {
    // fruit was eaten on this move

    // fruit no longer eaten
    for(u16 i = 0; i < basket->n; i++){
      if((basket->fruits[i].x == history->fruit_eaten_position[fruit_eaten-1].x) &&
         (basket->fruits[i].y == history->fruit_eaten_position[fruit_eaten-1].y)){
        basket->eaten[i] = FALSE;
        break;
      }
    }

    history->num_fruit_eaten--;
    snake->length--;
  } else { // fruit wasn't eaten
    snake->body[snake->length-1] = history->prev_tail_position[num_moves-1];
  }

  history->num_moves--;
  history->num_saved_positions--;

}

/* void input () { */
/*   const u8 mouse = *MOUSE_BUTTONS; */
/*   const u8 mouse_just_pressed = */
/*     *MOUSE_BUTTONS & (*MOUSE_BUTTONS ^ prev_mouse_state); */
/*   const s16 mouse_x = *MOUSE_X, mouse_y = *MOUSE_Y; */

/*   if(mouse_just_pressed & MOUSE_LEFT) { */
/*     /\* trace("mouse left"); *\/ */
/*     /\* tracef("%d %d\n", mouse_x, mouse_y); *\/ */
/*     /\* if(paused == 1) paused = 0; *\/ */
/*     /\* else paused = 1; *\/ */

/*   } */
/* } */

level_t level2, level4, level6, level8, level10, level12, level14;

// worm
char level2_map[3][10] =
   {"##########",
    "#CBA__$_$#",
    "##########",};

#define LEVEL4_W 7
#define LEVEL4_H 5

// snail
char level4_map[LEVEL4_H][LEVEL4_W] =
   {"#####__",
    "#$__#__",
    "#_#_###",
    "#CBA_$#",
    "#######",};

#define LEVEL6_W 4
#define LEVEL6_H 5

char level6_map[LEVEL6_H][LEVEL6_W] =
{
"####",
"#AD#",
"#BC#",
"#$##",
"####",
};

// save for later. actually i don't like this level anymore
// ABCDEFGHIJKLMNOPQRSTUVWXYZ
/* char level8_map[LEVEL8_H][LEVEL8_W] = */
/* { */
/* "##########", */
/* "#$BKO____#", */
/* "#Q___N___#", */
/* "#CRYWM___#", */
/* "#DSAXL___#", */
/* "#ETJVU___#", */
/* "#FGHIP___#", */
/* "#________#", */
/* "#________#", */
/* "##########", */
/* }; */


#define LEVEL8_W 6
#define LEVEL8_H 6
char level8_map[LEVEL8_H][LEVEL8_W] =
{
"######",
"#$DE$#",
"#ACB$#",
"#HGF$#",
"#$$$$#",
"######",
};

#define LEVEL10_W 6
#define LEVEL10_H 9

char level10_map[LEVEL10_H][LEVEL10_W] =
{
"#####_",
"#B#$#_",
"#A#_##",
"#$$$$#",
"#$$$$#",
"#$$$$#",
"#$$$$#",
"#__###",
"####__",
};

#define LEVEL12_W 14
#define LEVEL12_H 10

// pigs get slaughtered
char level12_map[LEVEL12_H][LEVEL12_W] =
{
"##############",
"###_____GFE_##",
"##__####ABC_D#",
"#$_$#$$______#",
"#__$#$$#######",
"#___#$$___####",
"#___#####____#",
"#$$$#####_$#_#",
"#########____#",
"##############",
};

#define LEVEL14_W 10
#define LEVEL14_H 11

char level14_map[LEVEL14_H][LEVEL14_W] =
{
"_###______",
"_#A#______",
"##_#######",
"#$$$_$$$##",
"#$$$#$$$##",
"#$$$#$$$##",
"##_###_###",
"#$$$_$$$##",
"#$$$_$$$##",
"#$$$#$$$##",
"##########",
};

// passing-a-multidimensional-variable-length-array-to-a-function
// https://stackoverflow.com/questions/14548753/passing-a-multidimensional-variable-length-array-to-a-function
u8 is_stuck(snake_t* s, s32 map_height, s32 map_width, char map[map_height][map_width]) {
  u8 can_go_right = TRUE, can_go_left = TRUE,
    can_go_up = TRUE, can_go_down = TRUE;

  point_t *h = &s->body[0];
  point_t *dir = &s->direction;

  s32 x_dist = abs(dir->x), y_dist = abs(dir->y);

  /* map is (row, col), which is map[y][x]. that's annoying. */
  if((h->y - y_dist > 0) && (map[h->y - y_dist][h->x] == '#'))
    can_go_up = FALSE;
  if((h->y + y_dist < map_height) && (map[h->y + y_dist][h->x] == '#'))
    can_go_down = FALSE;
  if((h->x - x_dist > 0) && (map[h->y][h->x - x_dist] == '#'))
    can_go_left = FALSE;
  if((h->x + x_dist < map_width) && (map[h->y][h->x + x_dist] == '#'))
    can_go_right = FALSE;

  for(int i = 1; i < s->length; i++){
    if((h->x == s->body[i].x) && (h->y - y_dist == s->body[i].y))
      can_go_up = FALSE;

    if((h->x == s->body[i].x) && (h->y + y_dist == s->body[i].y))
      can_go_down = FALSE;

    if((h->x - x_dist == s->body[i].x) && (h->y == s->body[i].y))
      can_go_left = FALSE;

    if((h->x + x_dist == s->body[i].x) && (h->y == s->body[i].y))
      can_go_right = FALSE;
  }

  /* tracef(" %c", map[h->y-1][h->x]); */
  /* tracef("%c %c", map[h->y][h->x-1], map[h->y][h->x+1]); */
  /* tracef(" %c", map[h->y+1][h->x]); */

  tracef(" %d", can_go_up);
  tracef("%d %d", can_go_left, can_go_right);
  tracef(" %d", can_go_down);

  return !(can_go_right || can_go_left || can_go_up || can_go_down);
}

/* void move_snake(snake_t *snake, level_t *level, char map[][level->w], */
/*                 fruit_basket_t *basket, history_t *history){ */
/* } */

/* void draw(snake_t *snake, level_t *level, char map[][level->w], fruit_basket_t *basket){ */
/* } */

void update_puzzle(snake_t* snake, level_t *level, char map[][level->w],
                  fruit_basket_t *basket, history_t *history){

  /*** draw ***/
  int x = SCREEN_SIZE/2 - (level->w * level->tw)/2,
    y = SCREEN_SIZE/2 - (level->h * level->th)/2;

  // draw level
  for(int row = 0; row < level->h; row++){
    for(int col = 0; col < level->w; col++) {
      if (map[row][col] == '#') {
        *DRAW_COLORS = 0x41;
        rect(x + (col*level->tw), y + (row*level->th), level->tw, level->th);
      }
    }
  }

  // draw snake
  for(u8 i = 0; i < snake->length; i++){
    // screen is 160x160 pixels
    // each snake body part is 8x8, so we have 20x20 blocks to fill the screen

    /* u8 w = max_u8(level6.tw-i+1, 5), */
    /*   h = max_u8(level6.th-i+1, 5); */

    /* rect(x+snake.body[i].x*level6.tw + (level6.tw - w), */
    /*      y+snake.body[i].y*level6.th + (level6.th - h), */
    /*      w, */
    /*      h); */

    /* rect(x+snake.body[i].x*level6.tw, */
    /*      y+snake.body[i].y*level6.th, */
    /*      level6.tw, */
    /*      level6.th); */

    if (i == 0 ) {
      *DRAW_COLORS = 0x02;
    } else if (i == snake->length-1) {
      *DRAW_COLORS = 0x04;
    } else {
      *DRAW_COLORS = 0x03;
    }

    if('A'+(char)i == '_'){
      text("-", x + (snake->body[i].x*level->tw), y + (snake->body[i].y*level->th));
    } else {
      const char s[] = {'A'+(char)i, '\0'};
      text(s, x + (snake->body[i].x*level->tw), y + (snake->body[i].y*level->th));
    }
  }

  // draw fruit
  for(int i = 0; i < basket->n; i++){
    if(!basket->eaten[i]){
      *DRAW_COLORS = 0x02;

      text("$",
           x+(basket->fruits[i].x*level->tw),
           y+(basket->fruits[i].y*level->th));
      /* rect(x+(basket->fruits[i].x*level->tw), */
      /*      y+(basket->fruits[i].y*level->th), */
      /*      level->tw, level->th); */

      /* blit(fruit_sprite, */
      /*      x+(basket->fruits[i].x*level->tw), */
      /*      y+(basket->fruits[i].y*level->th), */
      /*      level->tw, level->th, */
      /*      BLIT_2BPP); */
    }
  }


  const u8 just_pressed = *GAMEPAD1 & (*GAMEPAD1 ^ prev_key_state);

  /*** input ***/

  if (just_pressed & BUTTON_UP){
    snake->direction = (point_t){0,-1};
  }

  if (just_pressed & BUTTON_DOWN){
    snake->direction = (point_t){0,1};
  }

  if (just_pressed & BUTTON_LEFT){
    snake->direction = (point_t){-1,0};
  }

  if (just_pressed & BUTTON_RIGHT){
    snake->direction = (point_t){1,0};
  }

  /*** MOVE ***/
  s16 next_x = snake->body[0].x + snake->direction.x,
    next_y = snake->body[0].y + snake->direction.y;

  if ((snake->direction.x != 0 || snake->direction.y != 0) &&
      (0 <= next_x && next_x < level->w &&
       0 <= next_y && next_y < level->h) &&
      map[next_y][next_x] != '#'){

    snake->direction = (point_t){0,0};

    // body collision
    u8 head_body_collision = FALSE;
    for(u16 i = 1; i < snake->length-1; i++){ // tail can swap with head
      if(next_x == snake->body[i].x &&
         next_y == snake->body[i].y) {
        head_body_collision = 1;
        break;
      }
    }

    if(head_body_collision){
      /* stuck = TRUE; */
    } else{

      point_t tail = snake->body[snake->length-1];

      for(u16 i = snake->length-1; i > 0; i--){
        snake->body[i] = snake->body[i-1];
      }

      snake->body[0].x = next_x;
      snake->body[0].y = next_y;

      u8 fruit_eaten = FALSE;
      point_t fruit_pos;

      // fruit collision
      u8 num_eaten = 0;
      for (u16 i = 0; i < basket->n; i++){
        if(next_x == basket->fruits[i].x &&
           next_y == basket->fruits[i].y &&
           !basket->eaten[i]){

          add_body_part(snake, tail);
          basket->eaten[i] = TRUE;
          fruit_eaten = TRUE;
          fruit_pos = (point_t){next_x, next_y};
        }

        if(basket->eaten[i]) num_eaten++;
      }

      if(num_eaten < basket->n) {
        snake->body[0].x = next_x;
        snake->body[0].y = next_y;

        add_history(history, snake, fruit_eaten, fruit_pos);

        tracef("num_moves: %d, saved_pos: %d, num_fruit: %d",
               history->num_moves, history->num_saved_positions,
               history->num_fruit_eaten);

        /* for(u8 i = 0; i < history.num_saved_positions; i++){ */
        /*   tracef("tail[%d]: {%d,%d}", i, */
        /*          history.prev_tail_position[i].x, */
        /*          history.prev_tail_position[i].y); */
        /* } */

        /* tracef("is_stuck? %d\n", is_stuck(&snake, level6.w, level6_map)); */
      } else {
        current_level++;
        free_fruit_basket(basket);
        free_snake(snake);
        free_history(history);

        // HACK way to initial the snake level at the end of the game
        if(current_level == 15) {

          // create snake
          init_snake(snake, 3);
          snake->body[0] = (point_t){2,0};
          snake->body[1] = (point_t){1,0};
          snake->body[2] = (point_t){0,0};
          snake->direction = (point_t){1,0};

          // make level0_fruit
          level0_fruit.x = rand()%10;
          level0_fruit.y = rand()%10;
        }
      }
    }
  }

  // undo and reset
  if (just_pressed & BUTTON_2) {
    undo(history, snake, basket);
    return;
  }

  if (just_pressed & BUTTON_1) {
    tracef("reset level");
    init_level(level, map, snake, basket, history);
  }
}

void start() {

  /* can't tracef longs */
  /* im using 32 bit memory addresses, pointers are 4 bytes, 32 bits */
  /* tracef("%d %d", sizeof(char*), sizeof(u64*)); */
  /* tracef("%d %d\n", FALSE, TRUE); */

  state = MAIN_MENU;
  /*
  state = LEVEL;
  current_level = 15;

          init_snake(&snake, 3);
          snake.body[0] = (point_t){2,0};
          snake.body[1] = (point_t){1,0};
          snake.body[2] = (point_t){0,0};
          snake.direction = (point_t){1,0};

          // make level0_fruit
          level0_fruit.x = rand()%10;
          level0_fruit.y = rand()%10;
*/

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

  // current_level 2 init
  level2.h = 3;
  level2.w = 10;
  level2.tw = 8; // tile width
  level2.th = 8; // tile height

  // level 4 init
  level4.h = LEVEL4_H;
  level4.w = LEVEL4_W;
  level4.tw = 8;
  level4.th = 8;

  level6.h = LEVEL6_H;
  level6.w = LEVEL6_W;
  level6.tw = 8;
  level6.th = 8;

  level8.h = LEVEL8_H;
  level8.w = LEVEL8_W;
  level8.tw = 8;
  level8.th = 8;

  level10.h = LEVEL10_H;
  level10.w = LEVEL10_W;
  level10.tw = 8;
  level10.th = 8;

  level12.h = LEVEL12_H;
  level12.w = LEVEL12_W;
  level12.tw = 8;
  level12.th = 8;

  level14.h = LEVEL14_H;
  level14.w = LEVEL14_W;
  level14.tw = 8;
  level14.th = 8;
}

void update_main_menu(){
  // xors the previous state so we don't get repeating
  const u8 just_pressed = *GAMEPAD1 & (*GAMEPAD1 ^ prev_key_state);

  *DRAW_COLORS = 0x0024;
  text("snakery", 53, 50);
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
    current_level = 2;

    init_level(&level2, level2_map, &snake, &basket, &history);
  }

  /* if (just_pressed & BUTTON_2){ // z just pressed */
  /*   trace("button 2"); */
  /* } */
}

void update_level() {
  /*
  const u8 mouse = *MOUSE_BUTTONS;
  const u8 mouse_just_pressed =
    *MOUSE_BUTTONS & (*MOUSE_BUTTONS ^ prev_mouse_state);
  const s16 mouse_x = *MOUSE_X, mouse_y = *MOUSE_Y;
  */

  /* if(mouse_just_pressed & MOUSE_LEFT) { */
  /*   tracef("mouse left"); */
  /*   tracef("%d\n", snake.length); */
  /* } */

  // xors the previous state so we don't get repeating
  const u8 just_pressed = *GAMEPAD1 & (*GAMEPAD1 ^ prev_key_state);

  if (current_level == 0) { // classic snake
  }
  else if (current_level == 1) { // how about a puzzle game instead?
    /* *DRAW_COLORS = 0x0032; */
    /* text("how about a\npuzzle game\n instead?  ", 35, 50); */

    /* if ((frame_count % 60) < 30) { */
    /*   *DRAW_COLORS = 0x0001; */
    /* } else { */
    /*   *DRAW_COLORS = 0x0003; */
    /* } */
    /* text("X to continue", 30, 130); */

    /* if (just_pressed & BUTTON_1) { // x just pressed */
    /*   current_level = 2; */

    /*   /\* initialize current_level 2 *\/ */
    /*   init_level(&level2, level2_map, &snake, &basket, &history); */
    /* } */
  }
  else if (current_level == 2) { // puzzle1, easy as CAB
    update_puzzle(&snake, &level2, level2_map, &basket, &history);
  }
  else if (current_level == 3) {
    *DRAW_COLORS = 0x0032;
    text("press:", 10, 30);
    text(" Z to undo", 10, 50);
    text(" X to reset level", 10, 70);
    text(" R to reload game", 10, 90);


    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 4;

      /* initialize current_level 4 */

      init_level(&level4, level4_map, &snake, &basket, &history);
    }
  }
  else if (current_level == 4) {
    update_puzzle(&snake, &level4, level4_map, &basket, &history);
  }
  else if (current_level == 5) {
    *DRAW_COLORS = 0x0032;
    text("ouroboros", 45, 50);

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 6;

      /* initialize current_level 6 */
      init_level(&level6, level6_map, &snake, &basket, &history);
    }
  }
  else if (current_level == 6) {
    update_puzzle(&snake, &level6, level6_map, &basket, &history);
  }
  else if (current_level == 7) {
    *DRAW_COLORS = 0x0032;
    text("fruity loop", 35, 50);

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 8;

      /* initialize 8 */
      init_level(&level8, level8_map, &snake, &basket, &history);
    }
  }
  else if(current_level == 8) {
    update_puzzle(&snake, &level8, level8_map, &basket, &history);
  }
  else if(current_level == 9) {
    *DRAW_COLORS = 0x0032;
    text("treasure box", 30, 50);
    // pigs get\nslaughtered

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 10;

      init_level(&level10, level10_map, &snake, &basket, &history);
    }
  }
  else if(current_level == 10) {
    update_puzzle(&snake, &level10, level10_map, &basket, &history);
  }
  else if(current_level == 11) {
    *DRAW_COLORS = 0x0032;
    text("bulls make money,\nbears make money,\n...", 12, 50);

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 12;

      init_level(&level12, level12_map, &snake, &basket, &history);
    }
  }
  else if(current_level == 12) {
    update_puzzle(&snake, &level12, level12_map, &basket, &history);
  }
  else if(current_level == 13) {
    *DRAW_COLORS = 0x0032;
    text("odd 4 square", 30, 50);

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    if (just_pressed & BUTTON_1) { // x just pressed
      current_level = 14;

      init_level(&level14, level14_map, &snake, &basket, &history);
    }
  }
  else if(current_level == 14) {
    update_puzzle(&snake, &level14, level14_map, &basket, &history);
  }
  else if(current_level == 15) {

    *DRAW_COLORS = 0x0032;
    text("Congratulations!", 15, 50);

    if ((frame_count % 60) < 30) {
      *DRAW_COLORS = 0x0001;
    } else {
      *DRAW_COLORS = 0x0003;
    }
    text("X to continue", 30, 130);

    // draw level0_fruit
    *DRAW_COLORS = 0x4320;
    blit(fruit_sprite, level0_fruit.x*8, level0_fruit.y*8, 8, 8, BLIT_2BPP);

    /*** draw ***/
    *DRAW_COLORS = 0x0033;
    // draw body
    // size_t is an unsigned int of at least 16 bits
    for(size_t i = 0; i < snake.length-1; i++){
      // screen is 160x160 pixels
      // each snake body part is 8x8, so we have 20x20 blocks to fill the screen
      rect(snake.body[i].x*8, snake.body[i].y*8, 8, 8);
    }

    // draw tail
    *DRAW_COLORS = 0x0044;
    rect(snake.body[snake.length-1].x*8,
         snake.body[snake.length-1].y*8, 8, 8);

    // draw head
    *DRAW_COLORS = 0x0002;
    rect(snake.body[0].x*8, snake.body[0].y*8, 8,8);

    /*** move ***/
    if (frame_count % 5 == 0) {
      has_moved = 1;
      point_t last_body_part = snake.body[snake.length-1];
      for(int i = snake.length-1; i> 0; i--){
        snake.body[i] = snake.body[i-1];
      }

      snake.body[0].x = wrap(snake.body[0].x + snake.direction.x, 20);
      snake.body[0].y = wrap(snake.body[0].y + snake.direction.y, 20);

      // level0_fruit collision
      if(snake.body[0].x == level0_fruit.x && snake.body[0].y == level0_fruit.y){
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
          level0_fruit.x = rand()%20;
          level0_fruit.y = rand()%20;
          fruit_collides_snake = 0;

          for(int i = 0; i < snake.length; i++){
            if(level0_fruit.x == snake.body[i].x &&
               level0_fruit.y == snake.body[i].y) {
              fruit_collides_snake = 1;
            }
          }
        } while(fruit_collides_snake);
      }
    }

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

    // go back to main menu
    if (just_pressed & BUTTON_1) { // x just pressed

      state = MAIN_MENU;

      level0_score = 0;
      free_fruit_basket(&basket);
      free_snake(&snake);
    }
  }
  else {
    state = MAIN_MENU;
  }
}

void update () {
  frame_count++;
  srand(frame_count);

  if (state == MAIN_MENU) {
    update_main_menu();
  }
  else if(state == LEVEL) {
    update_level();
  }

  prev_key_state = *GAMEPAD1;
  prev_mouse_state = *MOUSE_BUTTONS;
}
