#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"


#define MATRIX_SIZE 8
typedef enum {
    PIXEL_OFF = 0,
    PIXEL_RED,
    PIXEL_GREEN
} pixel_t;

static pixel_t framebuffer [MATRIX_SIZE][MATRIX_SIZE];

#define MAX_SNAKE_LENGHT 64
typedef struct {
    int x;
    int y;
} point_t; // guarda coordenada

typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} direction_t;

static point_t snake[MAX_SNAKE_LENGHT];
static int snake_lenght = 3;

static direction_t current_dir = DIR_RIGHT; //dirección en la que se mueve
static direction_t next_dir = DIR_RIGHT; //dirección pedida por el jugador

static point_t apple;
static int game_over = 0;
static int score = 0;

// REEMPLAZAR POR PINES REALIES !!!!

static const gpio_num_t COMMON_PINS[8] = {
    GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_13, GPIO_NUM_14,
    GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_19
};

static const gpio_num_t RED_PINS[8] = {
    GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23, GPIO_NUM_25,
    GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33
};

static const gpio_num_t GREEN_PINS[8] = {
    GPIO_NUM_2, GPIO_NUM_15, GPIO_NUM_0, GPIO_NUM_12,
    GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33
};

void matrix_clear(void) //apaga toda la matrix
{
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            framebuffer[y][x] = PIXEL_OFF;
        }
    }
}

void matrix_set_pixel(int x, int y, pixel_t color) //pone un color en cada posición 
{
    if (x < 0 || x >= MATRIX_SIZE || y < 0 || y >= MATRIX_SIZE) {
        return;
    }

    framebuffer[y][x] = color;
}


void matrix_all_off (void)  //led solo se enciende si el común está en 1 y el cátodo de color en 0
{
    for (int i=0; i<8; i++){
        gpio_set_level(COMMON_PINS[i], 0); //comunes apagados
        gpio_set_level(RED_PINS[i],1); //rojo inactivo
        gpio_set_level(GREEN_PINS[i],1); //verde inactivo
    }
}

void matrix_refresh_step (int row){ //es para una fila dada
    matrix_all_off();
    for(int col=0; col<8; col++){
        pixel_t pixel = framebuffer[row][col];

        if (pixel == PIXEL_RED){
            gpio_set_level(RED_PINS[col],0);
        }
        else if (pixel == PIXEL_GREEN){
            gpio_set_level(GREEN_PINS[col],0);
        }
    }
    gpio_set_level (COMMON_PINS[row],1);
}

void matrix_init (void){
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 0
    };

    for (int i = 0; i<8; i ++){
        io_conf.pin_bit_mask |=(1ULL << COMMON_PINS[i]);
        io_conf.pin_bit_mask |=(1ULL << RED_PINS[i]);
        io_conf.pin_bit_mask |=(1ULL << GREEN_PINS[i]);
    }

    gpio_config (&io_conf);

    matrix_all_off();
    matrix_clear();
}

void matrix_task (void*arg){
    while(1){
        for (int row=0; row<8; row ++){
            matrix_refresh_step(row);
            esp_rom_delay_us(1000);
        }
    }
}

void app_main (void){
    matrix_init();

    matrix_set_pixel (2,2, PIXEL_GREEN);
    matrix_set_pixel (5,5, PIXEL_RED);

    xTaskCreate(matrix_task, "matrix_task", 2048, NULL, 10, NULL);
}

///serpiente

int is_on_snake (int x, int y){  //revisa posición de la serpiente, evita poner la manzana encima, detecta colisión

    for (int i = 0, i < snake_lenght; i++){
        if (snake[i].x == x && snake[i].y == y){
            return 1;
        }
    }
    return 0;
}

//crear la manzana

#include "esp_random.h"
void spawn_apple(void){  //escoger posición aleatoria para la manzana
    int x,y;

    do{ //para repetir hasta encontrar una celda libre
        x=esp_random() %8;
        y=esp_random()%8;
    } while (is_on_snake(x,y));

    apple.x = x;
    apple.y = y;
}

void game_init (void){ // inicia colocando una serpiente de 3 bloques y una manzana aleatoria
    snake_lenght =3;
    snake [0].x = 2; snake[0].y = 4; //cabeza
    snake [1].x = 1; snake[1].y = 4;
    snake[2].x = 0; snake[2].y = 4;

    current_dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;

    score=0;
    game_over = 0;

    spawn_apple();
}

void render_game(void){
    matrix_clear();

    if (game_over){
        return;
    }

    //dibujar serpiente verde
    for (int i = 0; i< snake_lenght; i++){
        matrix_set_pixel(snake[i].x, snake[i].y, PIXEL_GREEN);
    }

    //dibujar manzana en rojo
    matrix_set_pixel (apple.x, apple.y, PIXEL_RED);
}

int is_opposite(direction_t a, direction_t b) //evitar que la serpienta se devuelva
{
    return ((a == DIR_UP && b == DIR_DOWN) ||
            (a == DIR_DOWN && b == DIR_UP) ||
            (a == DIR_LEFT && b == DIR_RIGHT) ||
            (a == DIR_RIGHT && b == DIR_LEFT));
}

void move_snake(void){
    if (game_over){
        return;
    }
    if (!is_opposite(current_dir,next_dir)){
        current_dir=next_dir;
    }

    point_t new_head = snake[0];

    switch (current_dir){
        case DIR_UP:
            new_head.y--;
            break;
        case DIR_DOWN:
            new_head.y++;
            break;
        case DIR_LEFT:
            new_head.x--;
            break;
        case DIR_RIGHT:
            new_head.x ++;
            break;
    }

    
}





