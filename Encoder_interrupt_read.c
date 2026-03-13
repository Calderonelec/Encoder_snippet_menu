/**
 * @file encoder.c
 * @brief Implementación SIMPLE del encoder rotatorio
 * 
 * ARQUITECTURA:
 * 1. ISR captura cambios de pines (muy rápido)
 * 2. Variables globales almacenan estado
 * 3. Funciones públicas leen y limpian flags
 * 
 * PORTABLE: Solo necesitas:
 * - gpio_set_direction()
 * - gpio_set_pull_mode()
 * - gpio_get_level()
 * - gpio_set_intr_type()
 * - gpio_isr_handler_add()
 * - Temporizador para milisegundos
 */

#include "encoder.h"

// ============================================================================
// COMPATIBILIDAD CON DIFERENTES MCUs
// ============================================================================

#ifdef ESP_PLATFORM
    // ESP32 / ESP-IDF
    #include "driver/gpio.h"
    #include "esp_timer.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    
    #define GET_TIME_MS()       (esp_timer_get_time() / 1000)
    #define DELAY_MS(ms)        vTaskDelay(pdMS_TO_TICKS(ms))
    #define IRAM_ATTR_ENCODER   IRAM_ATTR
    
#elif defined(STM32)
    // STM32 HAL
    #include "stm32xxx_hal.h"
    
    #define GET_TIME_MS()       HAL_GetTick()
    #define DELAY_MS(ms)        HAL_Delay(ms)
    #define IRAM_ATTR_ENCODER
    
#else
    // Genérico - ADAPTAR SEGÚN TU MCU
    #warning "MCU no reconocido, usar funciones genéricas"
    #define GET_TIME_MS()       0  // Implementar: retornar milisegundos
    #define DELAY_MS(ms)           // Implementar: delay en milisegundos
    #define IRAM_ATTR_ENCODER
#endif

// ============================================================================
// VARIABLES GLOBALES PRIVADAS
// ============================================================================

/**
 * Estado del encoder (modificado por ISR, leído por usuario)
 * - volatile = puede cambiar en cualquier momento (por ISR)
 * - static = solo visible en este archivo
 */
static volatile int32_t  s_position = 0;           // Contador absoluto
static volatile uint8_t  s_last_state = 0;         // Último estado A/B
static volatile int8_t   s_direction_buffer = 0;   // Buffer de dirección
static volatile bool     s_button_clicked = false; // Flag de click
static volatile bool     s_button_long = false;    // Flag de long-press
static volatile bool     s_button_pressed = false; // Estado actual del botón
static volatile uint32_t s_button_press_time = 0;  // Tiempo inicio presión
static volatile bool     s_enabled = false;        // Habilitado/deshabilitado

/**
 * Variables para filtrado de rebotes
 */
static volatile uint32_t s_last_rotation_time = 0;
static volatile int8_t   s_rotation_counter = 0;   // Contador de pulsos

// ============================================================================
// FUNCIONES PRIVADAS - HELPERS
// ============================================================================

/**
 * @brief Lee estado digital de un pin (wrapper portable)
 */
static inline uint8_t read_pin(uint8_t pin)
{
#ifdef ESP_PLATFORM
    return gpio_get_level(pin);
#elif defined(STM32)
    return HAL_GPIO_ReadPin(GPIOx, GPIO_PIN_x);  // Ajustar según tu configuración
#else
    return 0;  // Implementar según tu MCU
#endif
}

// ============================================================================
// ISR - INTERRUPCIONES (CÓDIGO CRÍTICO)
// ============================================================================

/**
 * @brief ISR para rotación (pines A y B)
 * 
 * FUNCIONAMIENTO:
 * - Lee estado de pines A y B
 * - Compara con estado anterior
 * - Determina dirección usando tabla simple
 * - Filtra rebotes con contador
 * 
 * OPTIMIZACIÓN:
 * - Código mínimo en ISR
 * - Solo actualiza variables globales
 * - Filtrado básico de rebotes
 */
static void IRAM_ATTR_ENCODER encoder_rotation_isr(void *arg)
{
    // Si está deshabilitado, ignorar
    if (!s_enabled) return;
    
    // Leer estado actual de pines A y B
    uint8_t pin_a = read_pin(ENCODER_PIN_A);
    uint8_t pin_b = read_pin(ENCODER_PIN_B);
    
    // Codificar en 2 bits: [A][B]
    uint8_t current_state = (pin_a << 1) | pin_b;
    
    // Filtrar si no hubo cambio real
    if (current_state == s_last_state) {
        return;
    }
    
    // Combinar con estado anterior: [A_old][B_old][A_new][B_new]
    uint8_t combined = (s_last_state << 2) | current_state;
    
    // ========== TABLA GRAY CODE CORREGIDA PARA EC11 ==========
    // Solo cuenta transiciones válidas en secuencia correcta
    int8_t increment = 0;
    
    switch (combined) {
        // Secuencia CW (sentido horario): 00→01→11→10→00
        case 0b0001:  // 00 -> 01
        case 0b0111:  // 01 -> 11
        case 0b1110:  // 11 -> 10
        case 0b1000:  // 10 -> 00
            increment = 1;
            break;
            
        // Secuencia CCW (sentido antihorario): 00→10→11→01→00
        case 0b0010:  // 00 -> 10
        case 0b1011:  // 10 -> 11
        case 0b1101:  // 11 -> 01
        case 0b0100:  // 01 -> 00
            increment = -1;
            break;
            
        default:
            // Transición inválida (rebote o ruido), ignorar completamente
            s_last_state = current_state;
            return;
    }
    
    // Actualizar estado para próxima comparación
    s_last_state = current_state;
    
    // Acumular incremento
    s_rotation_counter += increment;
    
    // Filtrar por detents físicos del EC11 (4 pulsos por click)
    if (s_rotation_counter >= ENCODER_PULSES_PER_CLICK) {
        // Click completo en sentido CW
        s_position++;
        s_direction_buffer = 1;
        s_rotation_counter = 0;
        
    } else if (s_rotation_counter <= -ENCODER_PULSES_PER_CLICK) {
        // Click completo en sentido CCW
        s_position--;
        s_direction_buffer = -1;
        s_rotation_counter = 0;
    }
}

/**
 * @brief ISR para botón (pin SW)
 * 
 * FUNCIONAMIENTO:
 * - Detecta presión y liberación
 * - Mide duración de presión
 * - Clasifica como click o long-press
 * - Debounce por tiempo
 */
static void IRAM_ATTR_ENCODER encoder_button_isr(void *arg)
{
    if (!s_enabled) return;
    
    uint32_t now = GET_TIME_MS();
    uint8_t button_state = read_pin(ENCODER_PIN_SW);
    
    // Botón activo en BAJO (0 = presionado, 1 = suelto)
    bool is_pressed = (button_state == 0);
    
    // FLANCO DE BAJADA: Botón presionado
    if (is_pressed && !s_button_pressed) {
        s_button_pressed = true;
        s_button_press_time = now;
        
    // FLANCO DE SUBIDA: Botón liberado
    } else if (!is_pressed && s_button_pressed) {
        s_button_pressed = false;
        
        // Calcular duración de la presión
        uint32_t press_duration = now - s_button_press_time;
        
        // Debounce: ignorar presiones muy cortas
        if (press_duration < ENCODER_BUTTON_DEBOUNCE_MS) {
            return;  // Rebote
        }
        
        // Clasificar evento
        if (press_duration >= ENCODER_LONG_PRESS_MS) {
            s_button_long = true;     // Long-press
        } else {
            s_button_clicked = true;  // Click normal
        }
    }
}

// ============================================================================
// FUNCIONES PÚBLICAS - INICIALIZACIÓN
// ============================================================================

bool encoder_init(void)
{
#ifdef ESP_PLATFORM
    // ========== CONFIGURACIÓN ESP32 ==========
    
    // Configurar pines A y B como entrada con pull-up
    gpio_config_t io_conf_rotation = {
        .pin_bit_mask = (1ULL << ENCODER_PIN_A) | (1ULL << ENCODER_PIN_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  // Configurar después
    };
    gpio_config(&io_conf_rotation);
    
    // Configurar pin SW como entrada con pull-up
    gpio_config_t io_conf_button = {
        .pin_bit_mask = (1ULL << ENCODER_PIN_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_button);
    
    // Esperar estabilización de hardware
    DELAY_MS(50);
    
    // Leer estado inicial (importante para comparación)
    uint8_t pin_a = gpio_get_level(ENCODER_PIN_A);
    uint8_t pin_b = gpio_get_level(ENCODER_PIN_B);
    s_last_state = (pin_a << 1) | pin_b; //REVISAR
    
    // Instalar servicio de interrupciones GPIO (solo una vez)
    gpio_install_isr_service(0);
    
    // Registrar handlers de interrupción
    gpio_isr_handler_add(ENCODER_PIN_A, encoder_rotation_isr, NULL);
    gpio_isr_handler_add(ENCODER_PIN_B, encoder_rotation_isr, NULL);
    gpio_isr_handler_add(ENCODER_PIN_SW, encoder_button_isr, NULL);
    
    // AHORA habilitar interrupciones (cualquier flanco)
    gpio_set_intr_type(ENCODER_PIN_A, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(ENCODER_PIN_B, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(ENCODER_PIN_SW, GPIO_INTR_ANYEDGE);
    
#elif defined(STM32)
    // ========== CONFIGURACIÓN STM32 (EJEMPLO) ==========
    // Adaptar según tu configuración de HAL/LL
    
    // TODO: Configurar pines como entrada con pull-up
    // TODO: Habilitar interrupciones EXTI
    // TODO: Registrar callbacks
    
#else
    #error "Implementar inicialización para tu MCU"
#endif
    
    // Habilitar encoder
    s_enabled = true;
    
    return true;
}

// ============================================================================
// FUNCIONES PÚBLICAS - LECTURA DE ESTADO
// ============================================================================

encoder_direction_t encoder_get_direction(void)
{
    // Leer y limpiar buffer de dirección (atómico)
    encoder_direction_t result = ENCODER_NONE;
    
    if (s_direction_buffer > 0) {
        result = ENCODER_CW;
        s_direction_buffer = 0;  // Auto-limpia
        
    } else if (s_direction_buffer < 0) {
        result = ENCODER_CCW;
        s_direction_buffer = 0;  // Auto-limpia
    }
    
    return result;
}

bool encoder_button_clicked(void)
{
    // Leer y limpiar flag (atómico)
    if (s_button_clicked) {
        s_button_clicked = false;  // Auto-limpia
        return true;
    }
    return false;
}

bool encoder_button_long_press(void)
{
    // Leer y limpiar flag (atómico)
    if (s_button_long) {
        s_button_long = false;  // Auto-limpia
        return true;
    }
    return false;
}

int32_t encoder_get_position(void)
{
    return s_position;
}

void encoder_reset_position(void)
{
    s_position = 0;
}

bool encoder_is_pressed(void)
{
    return s_button_pressed;
}

// ============================================================================
// FUNCIONES PÚBLICAS - CONTROL
// ============================================================================

void encoder_enable(bool enable)
{
    s_enabled = enable;
}

void encoder_deinit(void)
{
#ifdef ESP_PLATFORM
    // Desactivar interrupciones
    s_enabled = false;
    
    // Remover handlers
    gpio_isr_handler_remove(ENCODER_PIN_A);
    gpio_isr_handler_remove(ENCODER_PIN_B);
    gpio_isr_handler_remove(ENCODER_PIN_SW);
#endif
    
    // Resetear variables
    s_position = 0;
    s_direction_buffer = 0;
    s_button_clicked = false;
    s_button_long = false;
    s_button_pressed = false;
}
