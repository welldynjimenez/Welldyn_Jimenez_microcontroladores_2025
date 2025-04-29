/*
 * emulador_555.h
 *
 *  Created on: 16 abr. 2025
 *      Author: mecat
 */

#ifndef MAIN_EMULADOR_555_H_
#define MAIN_EMULADOR_555_H_

#include <stdio.h>
#include <stdbool.h>

// Tipos de circuitos
typedef enum {
    MODE_ASTABLE,
    MODE_MONOSTABLE,
    MODE_PWM
} emulator_mode_t;

// Funciones principales
void emulator_init(void);
void emulator_start_astable(float r1, float r2, float c);
void emulator_trigger_monostable(float r, float c);
void emulator_start_pwm(float r_var, float r_fixed, float c);
void emulator_stop(void);
void emulator_get_parameters(emulator_mode_t *mode, float *frequency, float *duty_cycle);

// Funciones de cálculo
void calculate_astable(float r1, float r2, float c, float *frequency, float *duty_cycle);
float calculate_monostable(float r, float c);
void calculate_pwm(float r_var, float r_fixed, float c, float *frequency, float *duty_cycle);




#endif /* MAIN_EMULADOR_555_H_ */
