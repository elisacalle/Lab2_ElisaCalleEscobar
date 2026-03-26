//=========== JUEGO ATRAPA LA MANZANA ========================
// Matriz 6x6 para el juego
// 2 botones para mover el jugador (izquierda y derecha)
// 1 botón para reiniciar el juego
// LED verde que parpadea cuando llega al destino
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
// ==================== PINES ====================
// Matriz 6x6 (filas y columnas)
#define ROW_1 32
#define ROW_2 35
#define ROW_3 34
#define ROW_4 25
#define ROW_5 26
#define ROW_6 27

#define COL_R_1 19  // Columna 1 (Rojo)
#define COL_R_2 21  // Columna 2 (Rojo)
#define COL_R_3 22  // Columna 3 (Rojo)
#define COL_R_4 23  // Columna 4 (Rojo)
#define COL_R_5 12  // Columna 5 (Rojo)
#define COL_R_6 18  // Columna 6 (Rojo)

#define COL_G_1 4   // Columna 1 (Verde)
#define COL_G_2 5   // Columna 2 (Verde)
#define COL_G_3 17   // Columna 3 (Verde)
#define COL_G_4 16  // Columna 4 (Verde)
#define COL_G_5 2  // Columna 5 (Verde)
#define COL_G_6 15   // Columna 6 (Verde)

// Botones para mover el jugador y reiniciar el juego
#define BTN_LEFT 33   // Botón para mover a la izquierda
#define BTN_RIGHT 0 // Botón para mover a la derecha
#define BTN_START 14 // Botón para reiniciar el juego

// ==================== VARIABLES GLOBALES ====================
// Estado del juego
static volatile int player_x = 2;
static volatile int player_y = 5;  // Ajustado para que esté dentro de la matriz reducida
static volatile int apple_x = 4;
static volatile int apple_y = 0;
static volatile int score = 0;
static volatile int lives = 3;
static volatile bool game_over = false;
static volatile bool game_started = false;
static volatile uint8_t matrix[6][6]; // Matriz 6x6 para el juego

// ==================== FUNCIONES DE LA MATRIZ 6x6 ====================
// Limpia la matriz 6x6
void clear_matrix(void) {
   for (int row = 0; row < 6; row++) {
       for (int col = 0; col < 6; col++) {
           matrix[row][col] = 0;  // Apagar toda la matriz
       }
   }
}
// Activa el pixel en la matriz
void set_pixel(int x, int y, uint8_t color) {
   if (x >= 0 && x < 6 && y >= 0 && y < 6) {
       matrix[y][x] = color;  // Establece el color del pixel
   }
}
// Dibuja el juego en la matriz 6x6
void draw_game(void) {
   clear_matrix();  // Limpia la matriz
   // El jugador es rojo (1)
   set_pixel(player_x, player_y, 1);  // Rojo
   // La manzana es verde (2)
   if (!game_over) {
       set_pixel(apple_x, apple_y, 2);  // Verde
   }
}
// ==================== FUNCIONES DEL JUEGO ====================
// Reinicia el juego
void reset_game(void) {
   player_x = 2;  // Ajustado para la nueva matriz
   player_y = 5;  // Ajustado para la nueva matriz
   apple_x = rand() % 6; // La manzana solo aparece entre las columnas 1 a 6
   apple_y = 0;
   score = 0;
   lives = 3;
   game_over = false;
   game_started = true;
}
// Mueve el jugador hacia la izquierda
void move_player_left(void) {
   if (player_x > 0) player_x--;
}
// Mueve el jugador hacia la derecha
void move_player_right(void) {
   if (player_x < 5) player_x++; // Ajustado a la nueva matriz
}
// Mueve la manzana hacia abajo
void move_apple_down(void) {
   apple_y++;
}
// Respawn de la manzana
void respawn_apple(void) {
   apple_x = rand() % 6; // Asegura que la manzana aparezca entre las columnas 1 a 6
   apple_y = 0;
}
// Verifica la captura de la manzana o si se ha perdido una vida
void check_collision_or_miss(void) {
   if (apple_y == player_y) {
       if (apple_x == player_x) {
           // Cuando el jugador atrapa la manzana, ilumina toda la matriz en verde
           for (int row = 0; row < 6; row++) {
               for (int col = 0; col < 6; col++) {
                   matrix[row][col] = 2;  // Ilumina toda la matriz de verde (2)
               }
           }
           score++;  // Incrementa el puntaje
       } else {
           lives--;  // Si no atrapó la manzana, pierde una vida
           if (lives <= 0) {
               game_over = true;  // Si no tiene vidas, termina el juego
           }
       }
       respawn_apple();  // Respawn de la manzana
   }
   if (apple_y >= 6) {
       lives--;  // Si la manzana cae fuera de la matriz, pierde una vida
       if (lives <= 0) {
           game_over = true;  // Si no tiene vidas, termina el juego
       }
       respawn_apple();  // Respawn de la manzana
   }
}
// Función para mostrar la pantalla de Game Over
void show_game_over_screen(void) {
   for (int row = 0; row < 6; row++) {
       for (int col = 0; col < 6; col++) {
           matrix[row][col] = 1;  // Ilumina toda la matriz en rojo cuando termina el juego
       }
   }
   for (int i = 0; i < 40; i++) {
       // Llamada a refrescar la matriz 6x6
       vTaskDelay(pdMS_TO_TICKS(10));
   }
}
// ==================== FUNCIONES DEL TEMPORIZADOR ====================
void setup_timer(void) {
   // Configuración del temporizador para multiplexar las filas
   // Aquí va la configuración específica de tu temporizador
}

// ==================== FUNCION PRINCIPAL ====================
void app_main(void) {
   gpio_config_t io_conf = {
       .pin_bit_mask = (1ULL << ROW_1) | (1ULL << ROW_2) | (1ULL << ROW_3) | (1ULL << ROW_4) |
                       (1ULL << ROW_5) | (1ULL << ROW_6) |
                       (1ULL << COL_R_1) | (1ULL << COL_R_2) | (1ULL << COL_R_3) | (1ULL << COL_R_4) |
                       (1ULL << COL_R_5) | (1ULL << COL_R_6) |
                       (1ULL << COL_G_1) | (1ULL << COL_G_2) | (1ULL << COL_G_3) | (1ULL << COL_G_4) |
                       (1ULL << COL_G_5) | (1ULL << COL_G_6),
       .mode = GPIO_MODE_OUTPUT,
   };
   gpio_config(&io_conf);
   // Configurar el temporizador para multiplexar las filas
   setup_timer();
   // Iniciar el juego o la lógica principal
   while (1) {
       draw_game();  // Dibuja el estado actual del juego
       // Refrescar la matriz 6x6
       vTaskDelay(pdMS_TO_TICKS(250));
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