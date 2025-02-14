#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ON    1
#define OFF   0
#define EST_INIT 0 // Estado Inicial
#define EST_WAIT 1 // Estado Espera
#define EST_CERN 2 // Estado Cerrando
#define EST_CER  3 // Estado Cerrado
#define EST_ABIN 4 // Estado Abriendo
#define EST_ABI  5 // Estado Abierto
#define EST_ERR  6 // Estado Error
#define EST_EMER 7 // Estado Emergencia
#define time 25 // Tiempo de muestreo en MS
TickType_t mov_time = pdMS_TO_TICKS(180000); // Tiempo de puerta en movimiento en MS (3 min)

int Func_INIT();
int Func_WAIT();
int Func_CERN();
int Func_CER();
int Func_ABIN();
int Func_ABI();
int Func_ERR();
int Func_EMER();

volatile int EST_ACT = EST_INIT; //Estado Actual
volatile int EST_SIG = EST_INIT; //Estado Siguiente
volatile int EST_ANT = EST_INIT; //Estado Anterior
static TimerHandle_t timer; //Timer para las interrupciones
static TimerHandle_t mov_timer; //Timer para la puerta en movimiento
static TickType_t start_ticks;

volatile struct INPUT
{
    unsigned int LSA : 1;
    unsigned int LSC : 1;
    unsigned int CA : 1;
    unsigned int CC : 1;
    unsigned int FC : 1;
} in;

volatile struct OUTPUT
{
    unsigned int MC : 1;
    unsigned int MA : 1;
    unsigned int LED_EMER : 1;
    unsigned int LED_MOV : 1;
} out;

esp_err_t set_gpio(void) //Configuracion de pines
{
    gpio_config_t imputs;

    imputs.mode = GPIO_MODE_INPUT;
    imputs.pin_bit_mask  = (1ULL << GPIO_NUM_15 | //Entrada LSA
                            1ULL << GPIO_NUM_2  | //Entrada LSC
                            1ULL << GPIO_NUM_4  | //Entrada CA
                            1ULL << GPIO_NUM_16 | //Entrada CC
                            1ULL << GPIO_NUM_17); //Entrada FC
    imputs.pull_up_en = GPIO_PULLUP_DISABLE;
    imputs.pull_down_en = GPIO_PULLDOWN_ENABLE;
    imputs.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&imputs);

    gpio_config_t outputs;

    outputs.mode = GPIO_MODE_OUTPUT;
    outputs.pin_bit_mask = (1ULL << GPIO_NUM_23 | //Salida MC
                            1ULL << GPIO_NUM_22 | //Salida MA
                            1ULL << GPIO_NUM_1  | //Salida LED_EMER
                            1ULL << GPIO_NUM_3);  //Salida LED_MOV
    outputs.pull_up_en = GPIO_PULLUP_DISABLE;
    outputs.pull_down_en = GPIO_PULLDOWN_DISABLE;
    outputs.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&outputs);
}


int app_main()
{
    while (1)
    {
        switch (EST_SIG)
        {
        case EST_INIT: EST_SIG = Func_INIT(); 
            break; 
        case EST_WAIT: EST_SIG = Func_WAIT(); 
            break; 
        case EST_CERN: EST_SIG = Func_CERN(); 
            break; 
        case EST_CER:  EST_SIG = Func_CER(); 
            break; 
        case EST_ABIN: EST_SIG = Func_ABIN(); 
            break; 
        case EST_ABI:  EST_SIG = Func_ABI(); 
            break; 
        case EST_ERR:  EST_SIG = Func_ERR(); 
            break; 
        case EST_EMER: EST_SIG = Func_EMER(); 
            break;
        }
    }
}

void VtimerCallBack(TimerHandle_t pxTimer) //Funcion que se ejecuta con cada interrupcion
{
    //Actualizacion de las entradas
    in.LSA = gpio_get_level(GPIO_NUM_15);
    in.LSC = gpio_get_level(GPIO_NUM_2);
    in.CA  = gpio_get_level(GPIO_NUM_4);
    in.CC  = gpio_get_level(GPIO_NUM_16);
    in.FC  = gpio_get_level(GPIO_NUM_17);

    //Actualizacion de las salidas
    gpio_set_level(GPIO_NUM_23, out.MC);
    gpio_set_level(GPIO_NUM_22, out.MA);
    gpio_set_level(GPIO_NUM_1,  out.LED_EMER);
    gpio_set_level(GPIO_NUM_3,  out.LED_MOV);
}

int Func_INIT()
{   
    EST_ANT = EST_ACT;
    EST_ACT = EST_INIT;

    ESP_ERROR_CHECK(set_timer());
    ESP_ERROR_CHECK (set_gpio());

    while(1)
    {
        return EST_WAIT;
    }
}
int Func_WAIT()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_WAIT;

    // Valores de salida
    out.LED_EMER = OFF;
    out.LED_MOV = OFF;
    out.MA = OFF;
    out.MC = OFF;

    while(1)
    {
        if (in.LSC == false && in.LSA == false && in.FC == false) //La puerta esta medio abierta
        {
            return EST_CERN;
        }

        if (in.LSC == false && in.CC == true && in.FC == false) //Comando para cerrar 
        {
            return EST_CERN;
        }

        if (in.CA == true && in.FC == false && in.LSA == false) //Comando para abrir
        {
            return EST_ABIN;
        }
        //Condicion desconocida
        /*if (in.CA == true && in.FC == false && in.LSA == false)
        {
            return EST_ABIN;
        }*/
    }
}
int Func_CERN()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_CERN;

    out.LED_EMER = OFF;
    out.LED_MOV = ON;
    out.MA = OFF;
    out.MC = ON;

    xTimerChangePerio(mov_timer, mov_time, 0); //Se inicia el timer de la puerta en movimiento
    xTimerStart(mov_timer, 0);

    while(1)
    {
        TickType_t current_ticks = xTaskGetTickCount(); //Devuelve el tiempo que lleva el contador
        TickType_t expiry_time = xTimerGetExpiryTime(mov_timer); //Devuelve el tiempo total del contador
        return current_ticks >= expiry_time;

        if (current_ticks >= expiry_time) //Si la puerta dura mas de 3m abriendose
        {
            xTimerStop(mov_timer, 0);
            return EST_ERR;
        }
        
        if (in.LSC == true && in.LSA == false) //La puerta termino de cerrar
        {
            xTimerStop(mov_timer, 0);
            return EST_CER;
        }
    
        if (in.FC == true) //Se activo el sensor de la puerta
        {
            xTimerStop(mov_timer, 0);
            return EST_EMER;
        }

        if (in.CA == true && in.FC == false) //Comando para abrir
        {
            xTimerStop(mov_timer, 0);
            return EST_ABIN;
        }

        if (in.LSA == true && in.LSC == true) //Condicion no clara
        {
            xTimerStop(mov_timer, 0);
            return EST_ERR;
        }
        
    }
}
int Func_CER()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_CER;

    out.LED_EMER = OFF;
    out.LED_MOV = OFF;
    out.MA = false;
    out.MC = false;

    while(1)
    {
        return EST_WAIT;
    }
}
int Func_ABIN()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_ABIN;

    // Valores de salida
    out.LED_EMER = OFF;
    out.LED_MOV = ON;
    out.MA = ON;
    out.MC = OFF;

    xTimerChangePerio(mov_timer, mov_time, 0); //Se inicia el timer de la puerta en movimiento
    xTimerStart(mov_timer, 0);

    while(1)
    {
        TickType_t current_ticks = xTaskGetTickCount(); //Devuelve el tiempo que lleva el contador
        TickType_t expiry_time = xTimerGetExpiryTime(mov_timer); //Devuelve el tiempo total del contador
        return current_ticks >= expiry_time;

        if (current_ticks >= expiry_time) //Si la puerta dura mas de 3m abriendose
        {
            xTimerStop(mov_timer, 0);
            return EST_ERR;
        }

        if (in.LSA == true && in.LSC == false) //La puerta termino de abrir
        {
            xTimerStop(mov_timer, 0);
            return EST_ABI;
        }
    
        if (in.FC == true) //Se activo el sensor de la puerta
        {
            xTimerStop(mov_timer, 0);
            return EST_EMER;
        }

        if (in.CC == true && in.FC == false) //Comando para cerrar
        {
            xTimerStop(mov_timer, 0);
            return EST_CERN;
        }

        if (in.LSA == true && in.LSC == true)
        {
            xTimerStop(mov_timer, 0);
            return EST_ERR;
        }
    }   
}
int Func_ABI()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_ABI;

    // Valores de salida
    out.LED_EMER = OFF;
    out.LED_MOV = OFF;
    out.MA = OFF;
    out.MC = OFF;

    while(1)
    {
        return EST_WAIT;
    }
}
int Func_ERR()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_ERR;

    // Valores de salida
    out.LED_EMER = ON;
    out.LED_MOV = OFF;
    out.MA = OFF;
    out.MC = OFF;
    while (1)
    {
        printf("ERROR");
    }
}
int Func_EMER()
{
    // actualizar variables de EST
    EST_ANT = EST_ACT;
    EST_ACT = EST_EMER;

    // Valores de salida
    out.LED_EMER = ON;
    out.LED_MOV = OFF;
    out.MA = OFF;
    out.MC = OFF;

    while(1)
    {
        if (in.FC == false)
        {
            if (EST_ANT == EST_ABIN)
            {
                return EST_ABIN;
            }
            else
            {
                return EST_CERN;
            }
        }
        
    }
}
