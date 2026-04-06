#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

#define MATRIX_W 6 //defino tamaño de la matriz, 6 columnas (1 verde + 5 rojas)
#define MATRIX_H 6

//defino los niveles activos
#define ROW_ACTIVE_LEVEL   1
#define ROW_INACTIVE_LEVEL 0

#define COL_ACTIVE_LEVEL   0
#define COL_INACTIVE_LEVEL 1

//Matriz[x][y] guarda un estado del pixel: apagado, rojo o verde
#define PIXEL_OFF    0
#define PIXEL_RED    1
#define PIXEL_GREEN  2

#define REFRESH_ROW_MS        1  //tiempo activación de cada fila para multiplexar (en ms)
#define REFRESH_FRAME_MS      5  //tiempo total para refrescar toda la matriz (en ms) 
#define GAME_STEP_MS          20000 //tiempo entre cada paso del juego (cuanto tiempo se mantiene la escena actual antes de mover la manzana)
#define BUTTON_DEBOUNCE_MS    120 //evita que un solo toque de botón se registre como múltiples eventos debido a rebotes mecánicos del botón
#define START_DEBOUNCE_MS     200 //lo mismo pero para el botón de inicio  
#define FLASH_MS              180 //cuanto dura el destello verde cuando atrapa manzana

// 6 filas
#define ROW_1 16
#define ROW_2 17
#define ROW_3 18
#define ROW_4 19
#define ROW_5 21
#define ROW_6 22

// 5 columnas rojas
#define COL_R_2 23
#define COL_R_3 25
#define COL_R_4 26
#define COL_R_5 27
#define COL_R_6 32

// 1 sola columna verde: la canasta
#define COL_G_1 33

#define BTN_UP    14
#define BTN_DOWN  5
#define BTN_START 13

static const int row_pins[MATRIX_H] = {
    ROW_1, ROW_2, ROW_3, ROW_4, ROW_5, ROW_6
};

static const int red_col_pins[5] = {
    COL_R_2, COL_R_3, COL_R_4, COL_R_5, COL_R_6
};

static volatile uint8_t matrix[MATRIX_H][MATRIX_W];

static volatile int basket_y = 2;   
static volatile int apple_x = 5;    
static volatile int apple_y = 3;

static volatile int score = 0; //aciertos
static volatile int lives = 3; //vidas antes de gameover

static volatile bool game_started = false;
static volatile bool game_over = false;

//== FUNCIONES BÁSICAS ==
void clear_matrix(void) { //la recorre toda y la pone en off
    for (int y = 0; y < MATRIX_H; y++) {
        for (int x = 0; x < MATRIX_W; x++) {
            matrix[y][x] = PIXEL_OFF;
        }
    }
}

void set_pixel(int x, int y, uint8_t color) { //escribe color específico en una posición válida
    if (x >= 0 && x < MATRIX_W && y >= 0 && y < MATRIX_H) {
        matrix[y][x] = color;
    }
}

void all_rows_off(void) { //evitar que queden líneas activas
    for (int i = 0; i < MATRIX_H; i++) {
        gpio_set_level(row_pins[i], ROW_INACTIVE_LEVEL);
    }
}

void all_red_cols_off(void) {
    for (int i = 0; i < 5; i++) {
        gpio_set_level(red_col_pins[i], COL_INACTIVE_LEVEL);
    }
}

void green_col_off(void) {
    gpio_set_level(COL_G_1, COL_INACTIVE_LEVEL);
}

void all_matrix_off(void) {
    all_rows_off();
    all_red_cols_off();
    green_col_off();
}

void refresh_matrix_once(void) {
    for (int row = 0; row < MATRIX_H; row++) {
        all_matrix_off(); //apaga todo antes de activar la fila actual para evitar ghosting

        for (int col = 0; col < MATRIX_W; col++) {
            if (matrix[row][col] == PIXEL_GREEN) {
                // solo se permite verde en columna 0
                if (col == 0) {
                    gpio_set_level(COL_G_1, COL_ACTIVE_LEVEL);
                }
            } else if (matrix[row][col] == PIXEL_RED) {
                // rojo solo en columnas 1..5
                if (col >= 1 && col <= 5) {
                    gpio_set_level(red_col_pins[col - 1], COL_ACTIVE_LEVEL);
                }
            }
        }

        gpio_set_level(row_pins[row], ROW_ACTIVE_LEVEL);
        vTaskDelay(pdMS_TO_TICKS(REFRESH_ROW_MS));
    }

    all_matrix_off();
}

void refresh_matrix_for_ms(int total_ms) { //mantener visible
    int loops = total_ms / REFRESH_FRAME_MS;
    if (loops < 1) loops = 1;

    for (int i = 0; i < loops; i++) {
        refresh_matrix_once();
    }
}


void draw_game(void) {
    clear_matrix();

    if (!game_over) {
        // Canasta verde fija en columna 0, altura de 2 pixeles
        set_pixel(0, basket_y, PIXEL_GREEN);
        if (basket_y + 1 < MATRIX_H) {
            set_pixel(0, basket_y + 1, PIXEL_GREEN);
        }

        // Manzana roja moviéndose desde la derecha hacia la izquierda
        set_pixel(apple_x, apple_y, PIXEL_RED);
    }
}

void show_wait_screen(void) {
    clear_matrix();

    // muestra solo la canasta mientras empieza
    set_pixel(0, basket_y, PIXEL_GREEN);
    if (basket_y + 1 < MATRIX_H) {
        set_pixel(0, basket_y + 1, PIXEL_GREEN);
    }
}

void show_game_over_screen(void) {
    clear_matrix();

    // llena de rojo las columnas rojas disponibles
    for (int y = 0; y < MATRIX_H; y++) {
        for (int x = 1; x < MATRIX_W; x++) {
            matrix[y][x] = PIXEL_RED;
        }
    }

    refresh_matrix_for_ms(500);
}

void show_catch_flash(void) {
    clear_matrix();

    // deja la primera columna verde completa como flash de captura
    for (int y = 0; y < MATRIX_H; y++) {
        matrix[y][0] = PIXEL_GREEN;
    }

    refresh_matrix_for_ms(FLASH_MS);
}

void respawn_apple(void) {
    apple_x = 5;               // extremo derecho de la zona roja
    apple_y = rand() % 6;      // elije fila aleatoria para aparecer
}

void reset_game(void) { //condición inicial conocida
    basket_y = 2;
    score = 0;
    lives = 3;
    game_over = false;
    game_started = true;
    respawn_apple();
}

void move_basket_up(void) {
    if (basket_y > 0) {
        basket_y--;
    }
}

void move_basket_down(void) {
    if (basket_y < MATRIX_H - 2) {
        basket_y++;
    }
}

void move_apple_left(void) {
    apple_x--;
}

void check_collision_or_miss(void) {
    // cuando la manzana llega a la columna 1 (la última roja antes de la verde)
    if (apple_x == 1) {
        // la canasta ocupa basket_y y basket_y+1 en la columna verde
        if (apple_y == basket_y || apple_y == basket_y + 1) {
            score++;
            show_catch_flash();
            respawn_apple();
        } else {
            lives--;
            if (lives <= 0) {
                game_over = true;
            } else {
                respawn_apple();
            }
        }
    }

    // seguridad extra
    if (apple_x < 1) {
        lives--;
        if (lives <= 0) {
            game_over = true;
        } else {
            respawn_apple();
        }
    }
}

// Botones  
int button_pressed(int pin) {
    return gpio_get_level(pin) == 0;
}

void configure_matrix_outputs(void) {
    uint64_t output_mask =
        (1ULL << ROW_1) |
        (1ULL << ROW_2) |
        (1ULL << ROW_3) |
        (1ULL << ROW_4) |
        (1ULL << ROW_5) |
        (1ULL << ROW_6) |
        (1ULL << COL_R_2) |
        (1ULL << COL_R_3) |
        (1ULL << COL_R_4) |
        (1ULL << COL_R_5) |
        (1ULL << COL_R_6) |
        (1ULL << COL_G_1);

    gpio_config_t io_conf;
    io_conf.pin_bit_mask = output_mask;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&io_conf);
    all_matrix_off();
}

void configure_buttons(void) {
    uint64_t input_mask =
        (1ULL << BTN_UP) |
        (1ULL << BTN_DOWN) |
        (1ULL << BTN_START);

    gpio_config_t btn_conf;
    btn_conf.pin_bit_mask = input_mask;
    btn_conf.mode = GPIO_MODE_INPUT;
    btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    btn_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_conf.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&btn_conf);
}

void app_main(void) {
    configure_matrix_outputs();
    configure_buttons();
    clear_matrix();

    while (1) {
        if (!game_started) { //antes de iniciar
            show_wait_screen();
            refresh_matrix_for_ms(30);

            if (button_pressed(BTN_START)) {
                reset_game();
                vTaskDelay(pdMS_TO_TICKS(START_DEBOUNCE_MS));
            }

            continue;
        }

        if (game_over) { //si game over
            show_game_over_screen();

            if (button_pressed(BTN_START)) {
                reset_game();
                vTaskDelay(pdMS_TO_TICKS(START_DEBOUNCE_MS));
            }

            continue;
        }

        if (button_pressed(BTN_UP)) {
            move_basket_up();
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
        }

        if (button_pressed(BTN_DOWN)) {
            move_basket_down();
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
        }

        draw_game();
        refresh_matrix_for_ms(GAME_STEP_MS);

        move_apple_left();
        check_collision_or_miss();
    }
}