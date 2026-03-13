/**
 * @file menu.h
 * @brief Sistema de menú para configuración del sistema de vacunación
 * 
 * CARACTERÍSTICAS:
 * - Configurar distancia de recorrido del rack
 * - Configurar tiempo de vacunación
 * - Modo purga con tiempo configurable
 * - Navegación con encoder rotatorio
 * - Interfaz OLED clara
 * 
 * USO:
 * 1. menu_init() al inicio
 * 2. menu_check_entry() en loop principal
 * 3. Long-press encoder para entrar
 * 
 * @author GEL-VAC SDDV
 * @date 2025
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VALORES POR DEFECTO
 * ========================================================================== */

#define MENU_DISTANCIA_DEFAULT  600     ///< Distancia inicial rack (mm)
#define MENU_TIEMPO_DEFAULT     3000    ///< Tiempo vacunación inicial (ms)
#define MENU_PURGA_DEFAULT      5000    ///< Tiempo purga inicial (ms)

/* ============================================================================
 * LÍMITES DE CONFIGURACIÓN
 * ========================================================================== */

#define MENU_DISTANCIA_MIN      100     ///< Mínimo: 10 cm
#define MENU_DISTANCIA_MAX      800     ///< Máximo: 80 cm
#define MENU_DISTANCIA_STEP     10      ///< Paso: 1 cm

#define MENU_TIEMPO_MIN         1000    ///< Mínimo: 1 segundo
#define MENU_TIEMPO_MAX         10000   ///< Máximo: 10 segundos
#define MENU_TIEMPO_STEP        100     ///< Paso: 0.1 segundo

#define MENU_PURGA_MIN          1000    ///< Mínimo: 1 segundo
#define MENU_PURGA_MAX          30000   ///< Máximo: 30 segundos
#define MENU_PURGA_STEP         1000    ///< Paso: 1 segundo

/* ============================================================================
 * CONFIGURACIÓN GLOBAL
 * ========================================================================== */

/**
 * @brief Estructura de configuración del sistema
 * 
 * Variables globales accesibles desde cualquier parte del código
 */
typedef struct {
    uint32_t distancia_mm;      ///< Distancia recorrido rack (mm)
    uint32_t tiempo_ms;         ///< Tiempo objetivo vacunación (ms)
    uint32_t purga_ms;          ///< Tiempo purga bomba (ms)
} menu_config_t;

/* ============================================================================
 * FUNCIONES PÚBLICAS
 * ========================================================================== */

/**
 * @brief Inicializa el sistema de menú
 * 
 * QUÉ HACE:
 * - Inicializa configuración con valores por defecto
 * - Prepara variables internas
 * 
 * LLAMAR una vez al inicio del programa
 * 
 * @return true si OK
 */
bool menu_init(void);

/**
 * @brief Verifica si se debe entrar al menú (NO bloqueante)
 * 
 * USAR EN LOOP PRINCIPAL:
 * ```c
 * if (menu_check_entry()) {
 *     menu_show();  // Entrar al menú
 * }
 * ```
 * 
 * Detecta long-press del encoder para entrar
 * 
 * @return true si se debe entrar al menú
 */
bool menu_check_entry(void);

/**
 * @brief Muestra y ejecuta el menú principal (BLOQUEANTE)
 * 
 * QUÉ HACE:
 * - Muestra opciones en OLED
 * - Permite navegar con encoder
 * - Permite configurar parámetros
 * - Sale cuando usuario selecciona "Volver"
 * 
 * BLOQUEANTE: No retorna hasta que usuario sale del menú
 */
void menu_show(void);

/**
 * @brief Obtiene la configuración actual
 * 
 * USAR para leer parámetros:
 * ```c
 * menu_config_t *cfg = menu_get_config();
 * motor_mover_hasta_limite(1, cfg->distancia_mm);
 * ```
 * 
 * @return Puntero a estructura de configuración
 */
menu_config_t* menu_get_config(void);

/**
 * @brief Guarda configuración en NVS (opcional)
 * 
 * FUTURO: Guardar en memoria no volátil
 */
void menu_save_config(void);

/**
 * @brief Carga configuración desde NVS (opcional)
 * 
 * FUTURO: Cargar desde memoria no volátil
 */
void menu_load_config(void);

#ifdef __cplusplus
}
#endif

#endif // MENU_H
