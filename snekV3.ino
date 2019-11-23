#include "typedefs.h"

/***** Measure Memory Use *****/

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__
 
int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

/***** Graphics Definitions *****/

#define BLACK     0x0000
#define BLUE      0x001F
#define GREEN     0x07E0
#define RED       0xF800
#define YELLOW    0xFFE0

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// For the Adafruit shield, these are the default.
#define TFT_CLK 13
#define TFT_MISO 12
#define TFT_MOSI 11
#define TFT_DC 9
#define TFT_CS 10
#define TFT_RST 8
// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
//Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
// If using the breakout, change pins as desired
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

/***** Button Definitions *****/

#define BUTTON_LEFT 2
#define BUTTON_UP 4
#define BUTTON_DOWN 3
#define BUTTON_RIGHT 5

/***** Game Logic Definitions *****/

#define TIMESTEP 125
unsigned long tick = millis() + TIMESTEP;

#define GRID_WIDTH 20
#define GRID_HEIGHT 20

// Offsets the scorebar at the top of the screen
#define Y_OFFSET 80

// Object definitions for the game state matrices. Use powers of two.
#define EMPTY 1
#define SNAKE_HEAD 2
#define SNAKE_BODY 4
#define PELLET 8

typedef struct {
  unsigned char x;
  unsigned char y;
} Pellet;

Pellet pellet;
bool is_pellet_collision = false;
bool game_over = false;

Node *head;
unsigned char snake_start_length = 5;
unsigned char snake_max_length = 64;

typedef enum {North, East, South, West} Direction;
Direction directions[4] = {North, East, South, West};
Direction snake_direction = East;

char grid_current[GRID_HEIGHT][GRID_WIDTH];
char grid_next[GRID_HEIGHT][GRID_WIDTH];
char grid_draw[GRID_HEIGHT][GRID_WIDTH];

unsigned char score = 0;
bool scoreChanged = false;


/***** Game Code *****/

void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  
  logic_setup();
  draw_setup();
}

void draw_setup() {
  tft.begin();
  // Portrait mode, top-left corner is at the end with the pins, at the opposite side
  // from the Vcc
  tft.setRotation(2);
  tft.fillScreen(BLACK);
  tft.drawLine(0, 79, 239, 79, RED);
  tft.setCursor(10, 10);
  tft.setTextColor(RED, BLACK);
  tft.setTextSize(3);
  tft.print("Score: ");
  tft.setCursor(120, 10);
  tft.print(score);
  // Draw the snake head
  tft.fillRect(head->x * 12, head->y * 12 + Y_OFFSET, 12, 12, GREEN);
  // Draw the first pellet
  tft.fillRect(pellet.x * 12, pellet.y * 12 + Y_OFFSET, 12, 12, YELLOW);
}

void logic_setup() {
  // Initialise grids
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      grid_current[y][x] = EMPTY;
      grid_next[y][x] = EMPTY;
      grid_draw[y][x] = 0;
    }
  }

  head = snake_init(snake_start_length);
  
  // Place snake on grid
  grid_current[head->y][head->x] = SNAKE_HEAD;

  pellet_init();
}

Node *snake_init(unsigned char num_nodes) {
  Node *head, *n;
  head = malloc(sizeof(Node));
  head->x = 10;
  head->y = 0;
  head->next = NULL;
  n = head;
  // Place the additional nodes in a straight line behind the head
  for (unsigned char i = 1; i < num_nodes; i++) {
    Node *o = malloc(sizeof(Node));
    o->x = head->x - i;
    o->y = 0;
    o->next = NULL;
    n->next = o;
    n = o;
  }
  
  return head;
}

void pellet_init() {
  pellet.x = random(GRID_WIDTH);
  pellet.y = random(GRID_HEIGHT);
  // If the chosen location is not empty, try again
  if (grid_current[pellet.y][pellet.x] != EMPTY || grid_next[pellet.y][pellet.x] != EMPTY) {
    pellet_init();
  }
  else {
    // Add pellet to the grid
//    grid_current[pellet.y][pellet.x] = PELLET;
    grid_next[pellet.y][pellet.x] = PELLET;
  }
}

// An iterative implementation. Not sure the Arduino can handle recursion...
void snake_move(Node *n, unsigned char newX, unsigned char newY, bool is_pellet_collision) {
  unsigned char oldX, oldY;
  while(n != NULL) {
    oldX = n->x;
    oldY = n->y;
    n->x = newX;
    n->y = newY;
    n = n->next;
    newX = oldX;
    newY = oldY;
  }
  if (is_pellet_collision && snake_get_length() < snake_max_length) {
    Serial.println("hi");
    n = head;
    while (n->next != NULL) {
      n = n->next;
    }
    Node *newNode = malloc(sizeof(Node));
    newNode->x = newX;
    newNode->y = newY;
    newNode->next = NULL;
    n->next = newNode;
  }
}

// For debugging
unsigned char snake_get_length() {
  unsigned char len = 0;
  Node *n = head;
  while (n != NULL) {
    len++;
    n = n->next;
  }
  return len;
}

bool snake_is_self_collision() {
  Serial.println("hi");
  Node *n = head->next;
  while (n != NULL){
    if (head->x == n->x && head->y == n->y) {
      return true;
    }
    n = n->next;
  }
  return false;
}

void loop() {

  Serial.println(game_over);

//  Node *n = head;
//  while (n != NULL) {
//    Serial.print(n->x);
//    Serial.print(" ");
//    Serial.print(n->y);
//    Serial.println();
//    n = n->next;
//  }
//  Serial.println();
//  Serial.println();
//  Serial.println();

  while(millis() < tick) {
    // Check for user input. Most recent button press will be snake_direction
    if (digitalRead(BUTTON_LEFT) == HIGH && snake_direction != East) {
      snake_direction = West;
    }
    else if (digitalRead(BUTTON_UP) == HIGH && snake_direction != South) {
      snake_direction = North;
    }
    else if (digitalRead(BUTTON_DOWN) == HIGH && snake_direction != North) {
      snake_direction = South;
    }
    else if (digitalRead(BUTTON_RIGHT) == HIGH && snake_direction != West) {
      snake_direction = East;
    }
  }
  tick = millis() + TIMESTEP;

  unsigned long time_start = millis();

  game_update();
  game_draw();
}

void game_update() {

  if (game_over) {
    return;
  }

  is_pellet_collision = false;
  
  // Add the pellet to grid_next
  grid_next[pellet.y][pellet.x] = PELLET;

  // Holds the new position of the snake head
  unsigned char newX, newY;

  if (snake_direction == North) {
    newX = head->x;
    newY = head->y == 0 ? GRID_HEIGHT - 1 : head->y - 1;
//    head->y = head->y == 0 ? GRID_HEIGHT - 1 : head->y - 1;
  } 
  else if (snake_direction == East) {
    newX = head->x == GRID_WIDTH - 1 ? 0 : head->x + 1;
    newY = head->y;
//    head->x = head->x == GRID_WIDTH - 1 ? 0 : head->x + 1;
  }
  else if (snake_direction == South) {
    newX = head->x;
    newY = head->y == GRID_HEIGHT - 1 ? 0 : head->y + 1;
//    head->y = head->y == GRID_HEIGHT - 1 ? 0 : head->y + 1;
  }
  else if (snake_direction == West) {
    newX = head->x == 0 ? GRID_WIDTH - 1 : head->x - 1;
    newY = head->y;
//    head->x = head->x == 0 ? GRID_WIDTH - 1 : head->x - 1;
  }

  // Check for collision with pellet
  is_pellet_collision = newX == pellet.x && newY == pellet.y;

  // Update the position of the snake nodes
  snake_move(head, newX, newY, is_pellet_collision);

  // Check for collision with self
  game_over = snake_is_self_collision();
  if (game_over) {
    tft.setCursor(10, 45);
    tft.print("Game Over");
    return;
  }
  
  // Put the new positions on grid_next
  Node *n = head;
  while (n != NULL) {
    grid_next[n->y][n->x] = SNAKE_HEAD;
    n = n->next;
  }
//  grid_next[head->y][head->x] = SNAKE_HEAD;

  // If a pellet collision occurred, update the score and draw a new pellet
  if (is_pellet_collision) {
    score++;
    scoreChanged = true;
    pellet_init();
  }

  // Derive grid_draw
  // Will this be too slow? Only need to do it twice a second...
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      grid_draw[y][x] = grid_current[y][x] - grid_next[y][x];
      grid_current[y][x] = grid_next[y][x];
      grid_next[y][x] = EMPTY;
    }
  }
}

void game_draw() {

  // Update the score, if it's changed
  if (scoreChanged) {
    tft.setCursor(120, 10);
    tft.print(score);
    scoreChanged = false;
  }
  
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      switch (grid_draw[y][x]) {
        case -7:  // EMPTY --> PELLET
          tft.fillRect(x * 12, y * 12 + Y_OFFSET, 12, 12, YELLOW);
          break;
        case -1:  // EMPTY --> SNAKE_HEAD
          tft.fillRect(x * 12, y * 12 + Y_OFFSET, 12, 12, GREEN);
          break;
        case 1:   // SNAKE_HEAD --> EMPTY
          tft.fillRect(x * 12, y * 12 + Y_OFFSET, 12, 12, BLACK);
          break;
        case 6:   // PELLET --> SNAKE_HEAD
          tft.fillRect(x * 12, y * 12 + Y_OFFSET, 12, 12, GREEN);
          break;
        case 7:   // PELLET --> EMPTY
          tft.fillRect(x * 12, y * 12 + Y_OFFSET, 12, 12, BLACK);
          break;
      }
    }
  }
}
