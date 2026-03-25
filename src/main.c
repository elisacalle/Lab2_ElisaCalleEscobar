#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"

// Pines

// Segmentos para el display de 7 segmentos
#define SEG_A 4
#define SEG_B 5
#define SEG_C 18
#define SEG_D 19
#define SEG_E 21
#define SEG_F 22
#define SEG_G 23

// Comunes para los displays (usando transistor 2N3906)
#define DIG_1 26 // Primer dígito (puntaje)
#define DIG_2 27 // Segundo dígito (vidas)

// Matriz 8x8 (filas y columnas)
#define ROW_0 33
#define ROW_1 32
#define ROW_2 35
#define ROW_3 34
#define ROW_4 25
#define ROW_5 26
#define ROW_6 27
#define ROW_7 14

#define COL_R_0 18
#define COL_R_1 19
#define COL_R_2 21
#define COL_R_3 22
#define COL_R_4 23
#define COL_R_5 12
#define COL_R_6 13
#define COL_R_7 15

#define COL_G_0 2
#define COL_G_1 4
#define COL_G_2 5
#define COL_G_3 17
#define COL_G_4 16
#define COL_G_5 8
#define COL_G_6 9
#define COL_G_7 10

// Botones para mover el jugador y reiniciar el juego
#define BTN_LEFT 32
#define BTN_RIGHT 33
#define BTN_START 35

// Var Globales

// Estado del juego inicialmente
static volatile int player_x = 3;
static volatile int player_y = 7;
static volatile int apple_x = 4;
static volatile int apple_y = 0;
static volatile int score = 0;
static volatile int lives = 3;
static volatile bool game_over = false;
static volatile bool game_started = false;
static volatile bool led_verde_estado = false;
static volatile uint8_t matrix[8][8]; // Matriz para el juego (0 = apagado, 1 = rojo, 2 = verde)

// Display de ánodo común
// 0 = segmento encendido, 1 = segmento apagado
static const uint8_t NUMBERS[10][7] = {
    {0, 0, 0, 0, 0, 0, 1}, // 0
    {1, 0, 0, 1, 1, 1, 1}, // 1
    {0, 0, 1, 0, 0, 1, 0}, // 2
    {0, 0, 0, 0, 1, 1, 0}, // 3
    {1, 0, 0, 1, 1, 0, 0}, // 4
    {0, 1, 0, 0, 1, 0, 0}, // 5
    {0, 1, 0, 0, 0, 0, 0}, // 6
    {0, 0, 0, 1, 1, 1, 1}, // 7
    {0, 0, 0, 0, 0, 0, 0}, // 8
    {0, 0, 0, 0, 1, 0, 0}  // 9
};

void cargar_segmentos(int numero) {
    if (numero == -1) {  // Mostrar guion "--"
        gpio_set_level(SEG_A, 1);
        gpio_set_level(SEG_B, 1);
        gpio_set_level(SEG_C, 1);
        gpio_set_level(SEG_D, 1);
        gpio_set_level(SEG_E, 1);
        gpio_set_level(SEG_F, 1);
        gpio_set_level(SEG_G, 0);   // solo g encendido
        return;
    }
    uint8_t segments = NUMBERS[numero];
    gpio_set_level(SEG_A, (segments & 0x01) ? 1 : 0);
    gpio_set_level(SEG_B, (segments & 0x02) ? 1 : 0);
    gpio_set_level(SEG_C, (segments & 0x04) ? 1 : 0);
    gpio_set_level(SEG_D, (segments & 0x08) ? 1 : 0);
    gpio_set_level(SEG_E, (segments & 0x10) ? 1 : 0);
    gpio_set_level(SEG_F, (segments & 0x20) ? 1 : 0);
    gpio_set_level(SEG_G, (segments & 0x40) ? 1 : 0);
}

// Función para apagar los dígitos del display
void apagar_digitos(void) {
    gpio_set_level(DIG_1, 1);
    gpio_set_level(DIG_2, 1);
}

// Refrescar el display
void refrescar_displays(void) {
    apagar_digitos();
    cargar_segmentos(score / 10); // Decenas
    gpio_set_level(DIG_1, 0);    // Activar primer dígito (para puntaje)
    vTaskDelay(pdMS_TO_TICKS(5));

    // Mostrar las vidas en el segundo dígito
    apagar_digitos();
    cargar_segmentos(lives);     // Mostrar vidas
    gpio_set_level(DIG_2, 0);    // Activar segundo dígito (para vidas)
    vTaskDelay(pdMS_TO_TICKS(5));
}

// ==================== MATRIZ 8x8 ====================

void clear_matrix(void) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            matrix[row][col] = 0;
        }
    }
}

// Activa el pixel en la matriz
void set_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < 8 && y >= 0 && y < 8) {
        matrix[y][x] = color;
    }
}

// Dibuja el juego en la matriz 8x8
void draw_game(void) {
    clear_matrix();
    set_pixel(player_x, player_y, 2);  // Verde
    if (!game_over) {
        set_pixel(apple_x, apple_y, 1);  // Rojo
    }
}

// ==FUNCIONES DEL JUEGO ==

// Reinicia el juego
void reset_game(void) {
    player_x = 3;
    player_y = 7;
    apple_x = rand() % 8;
    apple_y = 0;
    score = 0;
    lives = 3;
    game_over = false;
    game_started = true;
}

// Jugador hacia la izquierda
void move_player_left(void) {
    if (player_x > 0) player_x--;
}

// Jugador hacia la derecha
void move_player_right(void) {
    if (player_x < 7) player_x++;
}

// Manzana hacia abajo
void move_apple_down(void) {
    apple_y++;
}

// Respawn de la manzana
void respawn_apple(void) {
    apple_x = rand() % 8;
    apple_y = 0;
}

// Verifica la captura de la manzana o si se ha perdido una vida
void check_collision_or_miss(void) {
    if (apple_y == player_y) {
        if (apple_x == player_x) {
            score++;
        } else {
            lives--;
            if (lives <= 0) {
                game_over = true;
            }
        }
        respawn_apple();
    }

    if (apple_y >= 8) {
        lives--;
        if (lives <= 0) {
            game_over = true;
        }
        respawn_apple();
    }
}

// Función para mostrar la pantalla de Game Over
void show_game_over_screen(void) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            matrix[row][col] = 1; // Rojo
        }
    }
    for (int i = 0; i < 40; i++) {
        refrescar_displays();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// == FUNCION PRINCIPAL ==

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SEG_A) | 
                        (1ULL << SEG_B) | 
                        (1ULL << SEG_C) | 
                        (1ULL << SEG_D) |
                        (1ULL << SEG_E) | 
                        (1ULL << SEG_F) | 
                        (1ULL << SEG_G) |
                        (1ULL << DIG_1) | 
                        (1ULL << DIG_2),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_config_t button_conf = {
        .pin_bit_mask = (1ULL << BTN_LEFT) | 
                        (1ULL << BTN_RIGHT) | 
                        (1ULL << BTN_START),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&button_conf);

    clear_matrix();
    reset_game();

    while (1) {
        draw_game();
        refrescar_displays();
        
        if (!game_started) {
            if (gpio_get_level(BTN_START) == 0) {
                reset_game();
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (game_over) {
            show_game_over_screen();
            if (gpio_get_level(BTN_START) == 0) {
                reset_game();
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (gpio_get_level(BTN_LEFT) == 0) {
            move_player_left();
            vTaskDelay(pdMS_TO_TICKS(120));
        }

        if (gpio_get_level(BTN_RIGHT) == 0) {
            move_player_right();
            vTaskDelay(pdMS_TO_TICKS(120));
        }

        move_apple_down();
        check_collision_or_miss();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}