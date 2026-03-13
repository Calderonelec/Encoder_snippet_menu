/**
 * @file encoder.h
 * @brief Driver SIMPLE para encoder rotatorio EC11
 * 
 * DISEÑO:
 * - Código claro y comentado para aprender
 * - Portable: funciona en cualquier MCU con GPIO e interrupciones
 * - Solo lo esencial para menús OLED
 * - ISR optimizada pero fácil de entender
 * 
 * HARDWARE:
 * - PIN A (CLK): Señal de reloj del encoder
 * - PIN B (DT):  Señal de datos del encoder
 * - PIN SW:      Botón push del encoder
 * 
 * FUNCIONAMIENTO BÁSICO:
 * 1. encoder_init() - Configurar pines e interrupciones
 * 2. encoder_get_direction() - Leer si giró CW/CCW
 * 3. encoder_button_clicked() - Detectar si se presionó
 * 4. encoder_button_long_press() - Detectar presión larga
 * 
 * @author GEL-VAC SDDV - Versión Educativa
 * @date 2025
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURACIÓN - AJUSTAR SEGÚN TU HARDWARE
 * ========================================================================== */

/** 
 * Pines del encoder - CAMBIAR SEGÚN TU ESP32/MCU
 */
#define ENCODER_PIN_A       19      // CLK (Clock) (invertidos)
#define ENCODER_PIN_B       17      // DT (Data)
#define ENCODER_PIN_SW      14      // SW (Switch/Button)

/**
 * Tiempos de debounce (en milisegundos)
 * - Ajustar si detecta rebotes o pierde eventos
 */
#define ENCODER_DEBOUNCE_MS         30      // Debounce para rotación
#define ENCODER_BUTTON_DEBOUNCE_MS  50      // Debounce para botón
#define ENCODER_LONG_PRESS_MS       1000     // Tiempo para long-press

/**
 * Sensibilidad del encoder
 * - EC11 típicamente usa 4 pulsos por "click" mecánico
 * - Si es muy sensible o poco sensible, ajustar este valor
 */
#define ENCODER_PULSES_PER_CLICK    4

/* ============================================================================
 * TIPOS DE DATOS
 * ========================================================================== */

/**
 * @brief Dirección de rotación del encoder
 */
typedef enum {
    ENCODER_NONE = 0,       // Sin movimiento
    ENCODER_CW,             // Giró en sentido horario (derecha)
    ENCODER_CCW             // Giró en sentido antihorario (izquierda)
} encoder_direction_t;

/* ============================================================================
 * FUNCIONES PÚBLICAS - API SIMPLE
 * ========================================================================== */

/**
 * @brief Inicializa el encoder
 * 
 * QUÉ HACE:
 * - Configura pines GPIO con pull-up
 * - Habilita interrupciones
 * - Resetea contadores internos
 * 
 * LLAMAR UNA VEZ al inicio del programa
 * 
 * @return true si OK, false si error
 */
bool encoder_init(void);

/**
 * @brief Obtiene dirección de rotación (NO bloqueante)
 * 
 * USAR EN LOOP PRINCIPAL:
 * ```c
 * encoder_direction_t dir = encoder_get_direction();
 * if (dir == ENCODER_CW) {
 *     menu_index++;
 * } else if (dir == ENCODER_CCW) {
 *     menu_index--;
 * }
 * ```
 * 
 * @return ENCODER_CW, ENCODER_CCW, o ENCODER_NONE
 */
encoder_direction_t encoder_get_direction(void);

/**
 * @brief Verifica si el botón fue presionado (click corto)
 * 
 * USAR PARA SELECCIONAR EN MENÚ:
 * ```c
 * if (encoder_button_clicked()) {
 *     ejecutar_opcion(menu_index);
 * }
 * ```
 * 
 * NOTA: Auto-limpia el flag después de leerlo
 * 
 * @return true si hubo click, false si no
 */
bool encoder_button_clicked(void);

/**
 * @brief Verifica si el botón fue presionado por largo tiempo
 * 
 * USAR PARA ENTRAR/SALIR DE MENÚ:
 * ```c
 * if (encoder_button_long_press()) {
 *     entrar_menu();
 * }
 * ```
 * 
 * NOTA: Auto-limpia el flag después de leerlo
 * 
 * @return true si hubo long-press, false si no
 */
bool encoder_button_long_press(void);

/**
 * @brief Obtiene posición absoluta del encoder
 * 
 * USAR SI NECESITAS CONTADOR:
 * ```c
 * int32_t pos = encoder_get_position();
 * printf("Valor: %ld\n", pos);
 * ```
 * 
 * @return Posición actual (contador incremental)
 */
int32_t encoder_get_position(void);

/**
 * @brief Resetea posición del encoder a cero
 * 
 * USAR AL ENTRAR A UN NUEVO MENÚ:
 * ```c
 * encoder_reset_position();
 * menu_index = 0;
 * ```
 */
void encoder_reset_position(void);

/**
 * @brief Verifica si botón está actualmente presionado
 * 
 * USAR PARA DETECTAR ESTADO EN TIEMPO REAL:
 * ```c
 * if (encoder_is_pressed()) {
 *     printf("Botón presionado ahora\n");
 * }
 * ```
 * 
 * @return true si presionado ahora, false si no
 */
bool encoder_is_pressed(void);

/* ============================================================================
 * FUNCIONES OPCIONALES - AVANZADO
 * ========================================================================== */

/**
 * @brief Habilita/deshabilita el encoder
 * 
 * USAR PARA PAUSAR TEMPORALMENTE:
 * ```c
 * encoder_enable(false);  // Pausar
 * // ... hacer algo ...
 * encoder_enable(true);   // Reanudar
 * ```
 * 
 * @param enable true = habilitar, false = deshabilitar
 */
void encoder_enable(bool enable);

/**
 * @brief Libera recursos del encoder
 * 
 * LLAMAR AL FINALIZAR (normalmente no se usa)
 */
void encoder_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_H
