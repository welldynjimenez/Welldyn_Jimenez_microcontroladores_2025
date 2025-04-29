//codigo actualizado con mejoras significativas 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_adc/adc_oneshot.h"  
// Definiciones mejoradas
#define WIFI_SSID      "ESP32_555_Emulator"
#define WIFI_PASS      "12345678"
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4
#define GPIO_OUTPUT    26  // Pin 26 como salida común para todos los modos
#define LED_BUILTIN    2   // LED incorporado del ESP32 para indicación visual

// Definición del canal ADC para el potenciómetro (GPIO34)
#define POT_ADC_CHANNEL ADC_CHANNEL_6
#define POT_ADC_UNIT ADC_UNIT_1
// Definición del pin para el trigger del monoestable
#define MONOSTABLE_TRIGGER_PIN 27

static const char *TAG = "emulador_555";

// HTML para la página web (lo pondremos como una cadena constante)
const char index_html[] = "<!DOCTYPE html>\
<html lang=\"es\">\
<head>\
    <meta charset=\"UTF-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
    <title>Emulador de 555 - ESP32</title>\
    <style>\
        body { font-family: 'Quicksand', sans-serif; margin: 0; padding: 0; background: #f1f4f9; color: #2c3e50; }\
        h1 { color: #6c5ce7; text-align: center; padding: 30px 0; font-size: 2.5em; font-weight: 700; }\
        .container { max-width: 850px; margin: 30px auto; background: #ffffff; padding: 35px; border-radius: 24px; box-shadow: 12px 12px 24px #d1d9e6, -12px -12px 24px #ffffff; }\
        .form-group { margin-bottom: 25px; }\
        label { display: block; margin-bottom: 8px; font-weight: 600; color: #555; }\
        select, input { width: 100%; padding: 12px 14px; border: none; border-radius: 12px; background: #e8f0fe; box-shadow: inset 2px 2px 5px #cdd3dc, inset -2px -2px 5px #ffffff; font-size: 1em; }\
        .button-container { margin-top: 30px; text-align: center; }\
        button { padding: 12px 28px; background-color: #6c5ce7; color: white; border: none; border-radius: 12px; cursor: pointer; font-weight: 600; font-size: 1em; transition: background-color 0.3s ease, transform 0.2s ease; box-shadow: 4px 4px 12px #d1d9e6, -4px -4px 12px #ffffff; }\
        button:hover { background-color: #5a4bd8; transform: translateY(-2px); }\
        button#stopBtn { background-color: #ff7675; }\
        button#stopBtn:hover { background-color: #e65c5c; }\
        .hidden { display: none; }\
        .circuit-image { max-width: 100%; height: auto; margin: 25px 0; text-align: center; border-radius: 12px; box-shadow: 6px 6px 16px #d1d9e6, -6px -6px 16px #ffffff; }\
        .result { margin-top: 30px; padding: 25px; background-color: #eaf2ff; border-radius: 12px; box-shadow: inset 2px 2px 8px #d1d9e6, inset -2px -2px 8px #ffffff; color: #2c3e50; font-size: 1.05em; }\
        .tabs { display: flex; justify-content: center; margin-bottom: 25px; gap: 10px; }\
        .tab { padding: 12px 20px; cursor: pointer; background: #dfe6fd; border-radius: 12px; font-weight: 600; color: #6c5ce7; box-shadow: 4px 4px 10px #d1d9e6, -4px -4px 10px #ffffff; transition: all 0.3s ease; }\
        .tab.active { background-color: #6c5ce7; color: white; box-shadow: inset 2px 2px 6px #594bcf, inset -2px -2px 6px #7d6cff; }\
        .tab-content { border-radius: 12px; background: #ffffff; box-shadow: inset 2px 2px 10px #d1d9e6, inset -2px -2px 10px #ffffff; padding: 25px; }\
        @media (max-width: 600px) {\
            h1 { font-size: 1.8em; padding: 20px 10px; }\
            .container { margin: 20px; padding: 20px; }\
            .tab { font-size: 0.9em; padding: 10px 14px; }\
            button { width: 100%; margin: 10px 0; }\
        }\
    </style>\
</head>\
<body>\
    <div class=\"container\">\
        <h1>555 con ESP32</h1>\
        <div class=\"tabs\">\
            <div class=\"tab active\" data-tab=\"config\">Configuración</div>\
            <div class=\"tab\" data-tab=\"docs\">Documentación</div>\
        </div>\
        <div class=\"tab-content\" id=\"config-tab\">\
            <div class=\"form-group\">\
                <label for=\"circuitType\">Circuito:</label>\
                <select id=\"circuitType\">\
                    <option value=\"astable\">Astable</option>\
                    <option value=\"monostable\">Monostable</option>\
                    <option value=\"pwm\">PWM</option>\
                </select>\
            </div>\
            <div id=\"astableOptions\">\
                <div class=\"form-group\">\
                    <label for=\"r1Astable\">R1:</label>\
                    <input type=\"number\" id=\"r1Astable\" min=\"100\" value=\"1000\">\
                </div>\
                <div class=\"form-group\">\
                    <label for=\"r2Astable\">R2:</label>\
                    <input type=\"number\" id=\"r2Astable\" min=\"100\" value=\"4700\">\
                </div>\
                <div class=\"form-group\">\
                    <label for=\"cAstable\">C:</label>\
                    <input type=\"number\" id=\"cAstable\" min=\"0.001\" step=\"0.001\" value=\"0.1\">\
                </div>\
            </div>\
            <div id=\"monostableOptions\" class=\"hidden\">\
                <div class=\"form-group\">\
                    <label for=\"rMonostable\">R:</label>\
                    <input type=\"number\" id=\"rMonostable\" min=\"100\" value=\"10000\">\
                </div>\
                <div class=\"form-group\">\
                    <label for=\"cMonostable\">C:</label>\
                    <input type=\"number\" id=\"cMonostable\" min=\"0.001\" step=\"0.001\" value=\"0.1\">\
                </div>\
                <div class=\"form-group\">\
                    <button id=\"pushBtn\">Botón Push</button>\
                </div>\
            </div>\
            <div id=\"pwmOptions\" class=\"hidden\">\
                <div class=\"form-group\">\
                    <label for=\"rPWMFixed\">Resistencia fija:</label>\
                    <input type=\"number\" id=\"rPWMFixed\" min=\"100\" value=\"1000\">\
                </div>\
                <div class=\"form-group\">\
                    <label for=\"cPWM\">C (μF):</label>\
                    <input type=\"number\" id=\"cPWM\" min=\"0.001\" step=\"0.001\" value=\"0.1\">\
                </div>\
            </div>\
            <div class=\"result\">\
                <h3>VALOR:</h3>\
                <div id=\"resultContent\"></div>\
            </div>\
            <div class=\"button-container\">\
                <button id=\"runBtn\">Ejecutar</button>\
                <button id=\"stopBtn\">Detener</button>\
            </div>\
        </div>\
        <div class=\"tab-content hidden\" id=\"docs-tab\">\
            <h2>Documentación del ESP32</h2>\
            <p>El ESP32 es un microcontrolador de bajo coste y bajo consumo con Wi-Fi y Bluetooth de modo dual integrado, fabricado por Espressif Systems.</p>\
            <h3>Características del ESP32:</h3>\
            <ul>\
                <li>Procesador dual-core o single-core Tensilica Xtensa LX6 de hasta 240MHz</li>\
                <li>520 KB SRAM</li>\
                <li>Wi-Fi integrado: 802.11 b/g/n</li>\
                <li>Bluetooth v4.2 BR/EDR y BLE</li>\
                <li>34 GPIO programables</li>\
                <li>12 entradas de sensor táctil capacitivo</li>\
                <li>2 convertidores DAC de 8 bits</li>\
                <li>18 canales ADC de 12 bits</li>\
            </ul>\
            </div>\
    </div>\
    <script>\
        document.addEventListener('DOMContentLoaded', function() {\
            const circuitType = document.getElementById('circuitType');\
            const astableOptions = document.getElementById('astableOptions');\
            const monostableOptions = document.getElementById('monostableOptions');\
            const pwmOptions = document.getElementById('pwmOptions');\
            const circuitImg = document.getElementById('circuitImg');\
            const resultContent = document.getElementById('resultContent');\
            const runBtn = document.getElementById('runBtn');\
            const stopBtn = document.getElementById('stopBtn');\
            const pushBtn = document.getElementById('pushBtn');\
            const tabs = document.querySelectorAll('.tab');\
            const tabContents = document.querySelectorAll('.tab-content');\
            \
            const circuitImages = {\
                astable: 'data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iNDAwIiBoZWlnaHQ9IjIwMCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cmVjdCB4PSIxNTAiIHk9IjUwIiB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgc3Ryb2tlPSJibGFjayIgZmlsbD0ibm9uZSIvPjx0ZXh0IHg9IjE4NSIgeT0iMTAwIiBmb250LWZhbWlseT0iQXJpYWwiIGZvbnQtc2l6ZT0iMTQiPjU1NTwvdGV4dD48bGluZSB4MT0iMTAwIiB5MT0iNzUiIHgyPSIxNTAiIHkyPSI3NSIgc3Ryb2tlPSJibGFjayIvPjxsaW5lIHgxPSIxMDAiIHkxPSIxMjUiIHgyPSIxNTAiIHkyPSIxMjUiIHN0cm9rZT0iYmxhY2siLz48bGluZSB4MT0iMjUwIiB5MT0iMTAwIiB4Mj0iMzAwIiB5Mj0iMTAwIiBzdHJva2U9ImJsYWNrIi8+PHRleHQgeD0iNzAiIHk9IjcwIiBmb250LWZhbWlseT0iQXJpYWwiIGZvbnQtc2l6ZT0iMTIiPlIxPC90ZXh0Pjx0ZXh0IHg9IjcwIiB5PSIxMjAiIGZvbnQtZmFtaWx5PSJBcmlhbCIgZm9udC1zaXplPSIxMiI+UjI8L3RleHQ+PHRleHQgeD0iMjcwIiB5PSI5MCIgZm9udC1mYW1pbHk9IkFyaWFsIiBmb250LXNpemU9IjEyIj5DPC90ZXh0Pjwvc3ZnPg==',\
                monostable: 'data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iNDAwIiBoZWlnaHQ9IjIwMCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cmVjdCB4PSIxNTAiIHk9IjUwIiB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgc3Ryb2tlPSJibGFjayIgZmlsbD0ibm9uZSIvPjx0ZXh0IHg9IjE4NSIgeT0iMTAwIiBmb250LWZhbWlseT0iQXJpYWwiIGZvbnQtc2l6ZT0iMTQiPjU1NTwvdGV4dD48bGluZSB4MT0iMTAwIiB5MT0iNzUiIHgyPSIxNTAiIHkyPSI3NSIgc3Ryb2tlPSJibGFjayIvPjxsaW5lIHgxPSIxMDAiIHkxPSIxMjUiIHgyPSIxNTAiIHkyPSIxMjUiIHN0cm9rZT0iYmxhY2siLz48bGluZSB4MT0iMjUwIiB5MT0iMTAwIiB4Mj0iMzAwIiB5Mj0iMTAwIiBzdHJva2U9ImJsYWNrIi8+PHRleHQgeD0iNzAiIHk9IjcwIiBmb250LWZhbWlseT0iQXJpYWwiIGZvbnQtc2l6ZT0iMTIiPlI8L3RleHQ+PHRleHQgeD0iNzAiIHk9IjEyMCIgZm9udC1mYW1pbHk9IkFyaWFsIiBmb250LXNpemU9IjEyIj5UcmlnZ2VyPC90ZXh0Pjx0ZXh0IHg9IjI3MCIgeT0iOTAiIGZvbnQtZmFtaWx5PSJBcmlhbCIgZm9udC1zaXplPSIxMiI+QzwvdGV4dD48cmVjdCB4PSI4MCIgeT0iMTMwIiB3aWR0aD0iMjAiIGhlaWdodD0iMTAiIHN0cm9rZT0iYmxhY2siIGZpbGw9Im5vbmUiLz48L3N2Zz4=',\
                pwm: 'data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iNDAwIiBoZWlnaHQ9IjIwMCIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cmVjdCB4PSIxNTAiIHk9IjUwIiB3aWR0aD0iMTAwIiBoZWlnaHQ9IjEwMCIgc3Ryb2tlPSJibGFjayIgZmlsbD0ibm9uZSIvPjx0ZXh0IHg9IjE4NSIgeT0iMTAwIiBmb250LWZhbWlseT0iQXJpYWwiIGZvbnQtc2l6ZT0iMTQiPjU1NTwvdGV4dD48bGluZSB4MT0iMTAwIiB5MT0iNzUiIHgyPSIxNTAiIHkyPSI3NSIgc3Ryb2tlPSJibGFjayIvPjxsaW5lIHgxPSIxMDAiIHkxPSIxMjUiIHgyPSIxNTAiIHkyPSIxMjUiIHN0cm9rZT0iYmxhY2siLz48bGluZSB4MT0iMjUwIiB5MT0iMTAwIiB4Mj0iMzAwIiB5Mj0iMTAwIiBzdHJva2U9ImJsYWNrIi8+<text x=\"NTAi\" y=\"NzAi\" font-family=\"Arial\" font-size=\"12\">Rfj</text><text x=\"NzAi\" y=\"MTIw\" font-family=\"Arial\" font-size=\"12\">Q</text><text x=\"Mjcw\" y=\"OTAi\" font-family=\"Arial\" font-size=\"12\">C</text></svg>'\
            };\
            \
            function updateCircuitOptions() {\
                const selectedType = circuitType.value;\
                \
                astableOptions.classList.add('hidden');\
                monostableOptions.classList.add('hidden');\
                pwmOptions.classList.add('hidden');\
                \
                if (selectedType === 'astable') {\
                    astableOptions.classList.remove('hidden');\
                } else if (selectedType === 'monostable') {\
                    monostableOptions.classList.remove('hidden');\
                } else if (selectedType === 'pwm') {\
                    pwmOptions.classList.remove('hidden');\
                }\
                \
                circuitImg.src = circuitImages[selectedType];\
            }\
            \
            function changeTab(tabId) {\
                tabs.forEach(tab => {\
                    tab.classList.remove('active');\
                });\
                tabContents.forEach(content => {\
                    content.classList.add('hidden');\
                });\
                \
                document.querySelector(`[data-tab=\"${tabId}\"]`).classList.add('active');\
                document.getElementById(`${tabId}-tab`).classList.remove('hidden');\
            }\
            \
            function runCircuit() {\
                const selectedType = circuitType.value;\
                \
                if (selectedType === 'astable') {\
                    const r1 = parseFloat(document.getElementById('r1Astable').value);\
                    const r2 = parseFloat(document.getElementById('r2Astable').value);\
                    const c = parseFloat(document.getElementById('cAstable').value);\
                    \
                    fetch('/api/astable', {\
                        method: 'POST',\
                        headers: {\
                            'Content-Type': 'application/json',\
                        },\
                        body: JSON.stringify({ r1, r2, c }),\
                    })\
                    .then(response => response.json())\
                    .then(data => {\
                        updateResults(data);\
                    });\
                } else if (selectedType === 'monostable') {\
                    const r = parseFloat(document.getElementById('rMonostable').value);\
                    const c = parseFloat(document.getElementById('cMonostable').value);\
                    \
                    fetch('/api/monostable', {\
                        method: 'POST',\
                        headers: {\
                            'Content-Type': 'application/json',\
                        },\
                        body: JSON.stringify({ r, c }),\
                    })\
                    .then(response => response.json())\
                    .then(data => {\
                        updateResults(data);\
                    });\
                } else if (selectedType === 'pwm') {\
                    const rFixed = parseFloat(document.getElementById('rPWMFixed').value);\
                    const c = parseFloat(document.getElementById('cPWM').value);\
                    \
                    fetch('/api/pwm', {\
                        method: 'POST',\
                        headers: {\
                            'Content-Type': 'application/json',\
                        },\
                        body: JSON.stringify({ rFixed, c }),\
                    })\
                    .then(response => response.json())\
                    .then(data => {\
                        updateResults(data);\
                    });\
                }\
            }\
            \
            function stopCircuit() {\
                fetch('/api/stop', {\
                    method: 'POST',\
                })\
                .then(response => response.json())\
                .then(data => {\
                    resultContent.innerHTML = 'Circuito detenido';\
                });\
            }\
            \
            function updateResults(data) {\
                let resultHtml = '';\
                \
                if (data.hasOwnProperty('frequency')) {\
                    resultHtml += `<p>Frecuencia: ${data.frequency.toFixed(2)} Hz</p>`;\
                    resultHtml += `<p>Ciclo de trabajo: ${data.dutyCycle.toFixed(2)}%</p>`;\
                }\
                \
                if (data.hasOwnProperty('pulseWidth')) {\
                    resultHtml += `<p>Ancho de pulso: ${(data.pulseWidth * 1000).toFixed(2)} ms</p>`;\
                }\
                \
                resultContent.innerHTML = resultHtml;\
            }\
            \
            circuitType.addEventListener('change', updateCircuitOptions);\
            \
            tabs.forEach(tab => {\
                tab.addEventListener('click', function() {\
                    changeTab(this.getAttribute('data-tab'));\
                });\
            });\
            \
            runBtn.addEventListener('click', runCircuit);\
            stopBtn.addEventListener('click', stopCircuit);\
            pushBtn.addEventListener('click', function() {\
                runCircuit();\
            });\
            \
            updateCircuitOptions();\
            \
            fetch('/api/parameters')\
                .then(response => response.json())\
                .then(data => {\
                    updateResults(data);\
                });\
        });\
    </script>\
</body>\
</html>";
// Tipos de circuitos
typedef enum {
    MODE_ASTABLE,
    MODE_MONOSTABLE,
    MODE_PWM,
    MODE_NONE  // Nuevo estado para cuando no hay ningún modo activo
} emulator_mode_t;

// Variables globales para ADC
static adc_oneshot_unit_handle_t adc_handle;

// Estado global del emulador - mejorado con valores iniciales
static struct {
    emulator_mode_t mode;
    bool running;
    esp_timer_handle_t timer;
    
    // Parámetros para astable
    float r1_astable;
    float r2_astable;
    float c_astable;
    
    // Parámetros para monostable
    float r_monostable;
    float c_monostable;
    
    // Parámetros para PWM
    float r_variable;   // Se actualizará leyendo el potenciómetro
    float r_fixed;
    float c_pwm;
    
    // Estado del pin y timing
    bool output_state;
    int64_t last_timer_start;  // Para cálculos más precisos
    
    // Estadísticas
    uint32_t cycle_count;
    float measured_frequency;
    float measured_duty_cycle;
} emulator_state = {
    .mode = MODE_NONE,
    .running = false,
    .timer = NULL,
    .r1_astable = 1000.0f,
    .r2_astable = 4700.0f,
    .c_astable = 0.0000001f,  // 0.1 μF
    .r_monostable = 10000.0f,
    .c_monostable = 0.0000001f,  // 0.1 μF
    .r_variable = 5000.0f,
    .r_fixed = 1000.0f,
    .c_pwm = 0.0000001f,  // 0.1 μF
    .output_state = false,
    .last_timer_start = 0,
    .cycle_count = 0,
    .measured_frequency = 0.0f,
    .measured_duty_cycle = 0.0f
};

// Mutex para proteger el acceso a emulator_state
SemaphoreHandle_t emulator_mutex;

// Prototipos de funciones (agregarlos al principio para evitar declaraciones implícitas)
void emulator_start_astable(float r1, float r2, float c);
void emulator_trigger_monostable(float r, float c);
void emulator_start_pwm(float r_fixed, float c);
void emulator_stop(void);
void emulator_get_parameters(emulator_mode_t *mode, float *frequency, float *duty_cycle, float *pulse_width);

// Función mejorada para leer el valor del potenciómetro con filtrado
static float read_potentiometer(void) {
    // Tomar múltiples muestras para reducir el ruido
    const int num_samples = 10;
    int adc_sum = 0;
    int raw = 0;
    
    for (int i = 0; i < num_samples; i++) {
        adc_oneshot_read(adc_handle, POT_ADC_CHANNEL, &raw);
        adc_sum += raw;
        vTaskDelay(1 / portTICK_PERIOD_MS);  // Pequeña pausa entre lecturas
    }
    
    int adc_val = adc_sum / num_samples;
    
    // Convertir el valor ADC a voltaje (aproximadamente en un rango de 0 a 3.3V)
    float voltage = adc_val * (3.3f / 4095.0f);
    
    // Mapear el voltaje a un rango de resistencia, por ejemplo de 100 Ω a 10000 Ω
    float resistance = 100.0f + ((voltage / 3.3f) * (10000.0f - 100.0f));
    
    ESP_LOGD(TAG, "Potentiometer ADC: %d, Voltage: %.2fV, Resistance: %.2f Ohms", 
             adc_val, voltage, resistance);
    
    return resistance;
}

// Función de inicialización del ADC para el potenciómetro - actualizada para nueva API
static void potentiometer_init(void) {
    // Configurar ADC con la nueva API
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = POT_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    // Configurar el canal
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, POT_ADC_CHANNEL, &config));
    
    ESP_LOGI(TAG, "Potenciómetro inicializado en pin GPIO34 (ADC1_CH6)");
    
    // Hacer una lectura inicial
    float initial_resistance = read_potentiometer();
    ESP_LOGI(TAG, "Lectura inicial del potenciómetro: %.2f Ohms", initial_resistance);
}

// Funciones de cálculo - mejoradas para mayor precisión
void calculate_astable(float r1, float r2, float c, float *frequency, float *duty_cycle) {
    float t1 = 0.693f * (r1 + r2) * c;
    float t2 = 0.693f * r2 * c;
    float total_period = t1 + t2;
    
    *frequency = 1.0f / total_period;
    *duty_cycle = (t1 / total_period) * 100.0f;
    
    ESP_LOGD(TAG, "Astable - t1: %.6fms, t2: %.6fms, frequency: %.2fHz, duty: %.2f%%", 
             t1*1000, t2*1000, *frequency, *duty_cycle);
}

float calculate_monostable(float r, float c) {
    float pulse_width = 1.1f * r * c;
    ESP_LOGD(TAG, "Monostable - pulse width: %.6fs (%.2fms)", pulse_width, pulse_width*1000);
    return pulse_width;
}

void calculate_pwm(float r_var, float r_fixed, float c, float *frequency, float *duty_cycle) {
    float t1 = 0.693f * (r_var + r_fixed) * c;
    float t2 = 0.693f * r_fixed * c;
    float total_period = t1 + t2;
    
    *frequency = 1.0f / total_period;
    *duty_cycle = (t1 / total_period) * 100.0f;
    
    ESP_LOGD(TAG, "PWM - t1: %.6fms, t2: %.6fms, frequency: %.2fHz, duty: %.2f%%", 
             t1*1000, t2*1000, *frequency, *duty_cycle);
}

// Función mejorada para actualizar la salida
static void update_output_state(bool new_state) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    emulator_state.output_state = new_state;
    
    // Establecer el nivel del pin de salida principal
    gpio_set_level(GPIO_OUTPUT, new_state);
    
    // También actualizar el LED incorporado para indicación visual
    gpio_set_level(LED_BUILTIN, new_state);
    
    // Si estamos cambiando de estado, actualizar las estadísticas
    int64_t current_time = esp_timer_get_time();
    if (emulator_state.last_timer_start > 0) {
        int64_t duration_us = current_time - emulator_state.last_timer_start;
        
        // Actualizar estadísticas solo si estamos en modo astable o PWM
        if ((emulator_state.mode == MODE_ASTABLE || emulator_state.mode == MODE_PWM) && 
            emulator_state.cycle_count > 0) {
            
            // Si pasamos de HIGH a LOW, actualizamos el ciclo de trabajo
            if (!new_state) {
                int64_t high_time_us = duration_us;
                // Total time es high_time + low_time de la iteración anterior
                if (emulator_state.cycle_count > 1) {
                    float cycle_time_s = 1.0f / emulator_state.measured_frequency;
                    emulator_state.measured_duty_cycle = (high_time_us / 1000000.0f) / cycle_time_s * 100.0f;
                }
            } 
            // Si pasamos de LOW a HIGH, actualizamos la frecuencia
            else if (emulator_state.cycle_count > 1) {
                // Calcular la frecuencia basada en el tiempo total del ciclo anterior
                emulator_state.measured_frequency = 1000000.0f / (float)duration_us;
            }
        }
    }
    
    emulator_state.last_timer_start = current_time;
    emulator_state.cycle_count++;
    
    xSemaphoreGive(emulator_mutex);
}

// Manejadores del timer - mejorados
void astable_timer_callback(void* arg) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    bool current_state = emulator_state.output_state;
    float r1 = emulator_state.r1_astable;
    float r2 = emulator_state.r2_astable;
    float c = emulator_state.c_astable;
    
    // Invertir el estado
    bool new_state = !current_state;
    
    // Calcular el próximo intervalo de tiempo
    uint64_t next_interval;
    if (new_state) {
        // Estado alto - duración = 0.693 * (R1 + R2) * C
        next_interval = (uint64_t)(0.693f * (r1 + r2) * c * 1000000); // En microsegundos
    } else {
        // Estado bajo - duración = 0.693 * R2 * C
        next_interval = (uint64_t)(0.693f * r2 * c * 1000000); // En microsegundos
    }
    
    xSemaphoreGive(emulator_mutex);
    
    // Actualizar la salida
    update_output_state(new_state);
    
    // Reprogramar el timer con el nuevo intervalo
    if (emulator_state.running) {
        esp_timer_restart(emulator_state.timer, next_interval);
    }
}

void monostable_timer_callback(void* arg) {
    // Cuando el tiempo del monoestable expira, volvemos a estado bajo
    update_output_state(false);
    
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    emulator_state.running = false;
    xSemaphoreGive(emulator_mutex);
    
    ESP_LOGI(TAG, "Monostable pulse completed");
}

void pwm_timer_callback(void* arg) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    // Actualizamos la resistencia variable leyendo el potenciómetro externo.
    emulator_state.r_variable = read_potentiometer();
    float r_var = emulator_state.r_variable;
    float r_fixed = emulator_state.r_fixed;
    float c = emulator_state.c_pwm;
    
    // Invertir el estado
    bool new_state = !emulator_state.output_state;
    
    // Calcular el próximo intervalo de tiempo
    uint64_t next_interval;
    if (new_state) {
        // Estado alto - duración = 0.693 * (r_variable + r_fixed) * C
        next_interval = (uint64_t)(0.693f * (r_var + r_fixed) * c * 1000000);
    } else {
        // Estado bajo - duración = 0.693 * r_fixed * C
        next_interval = (uint64_t)(0.693f * r_fixed * c * 1000000);
    }
    
    xSemaphoreGive(emulator_mutex);
    
    // Actualizar la salida
    update_output_state(new_state);
    
    // Reprogramar el timer con el nuevo intervalo
    if (emulator_state.running) {
        esp_timer_restart(emulator_state.timer, next_interval);
    }
}

// Función para manejar la interrupción del trigger del monoestable
static void IRAM_ATTR monostable_trigger_isr_handler(void* arg) {
    // Solo activamos el trigger si no estamos ya ejecutando un pulso
    if (!emulator_state.running && emulator_state.mode == MODE_MONOSTABLE) {
        // Debemos programar la activación real del monoestable desde una tarea
        // ya que no debemos llamar a ciertas funciones desde una ISR
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(arg, 0, eNoAction, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// Tarea para manejar el trigger del monoestable
static void monostable_trigger_task(void* arg) {
    while (1) {
        // Esperar notificación desde la ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Verificar si podemos activar el monoestable
        if (emulator_state.mode == MODE_MONOSTABLE && !emulator_state.running) {
            ESP_LOGI(TAG, "Monostable triggered by hardware button");
            
            // Activar el monoestable con los valores actuales
            float r = emulator_state.r_monostable;
            float c = emulator_state.c_monostable;
            emulator_trigger_monostable(r, c);
        }
    }
}

// Inicialización del emulador - mejorada
void emulator_init(void) {
    // Crear mutex para protección de estado
    emulator_mutex = xSemaphoreCreateMutex();
    
    // Configurar el pin de salida principal
    gpio_reset_pin(GPIO_OUTPUT);
    gpio_set_direction(GPIO_OUTPUT, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_OUTPUT, 0);
    
    // Configurar el LED integrado
    gpio_reset_pin(LED_BUILTIN);
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_BUILTIN, 0);
    
    // Configurar pin de trigger para monoestable con resistencia pull-up
    gpio_reset_pin(MONOSTABLE_TRIGGER_PIN);
    gpio_set_direction(MONOSTABLE_TRIGGER_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MONOSTABLE_TRIGGER_PIN, GPIO_PULLUP_ONLY);
    
    // Configurar interrupción para el trigger del monoestable (en flanco descendente)
    TaskHandle_t trigger_task_handle;
    xTaskCreate(monostable_trigger_task, "monostable_task", 2048, NULL, 10, &trigger_task_handle);
    gpio_set_intr_type(MONOSTABLE_TRIGGER_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MONOSTABLE_TRIGGER_PIN, monostable_trigger_isr_handler, (void*)trigger_task_handle);
    
    // Registrar una tarea periódica para actualizar las estadísticas
    ESP_LOGI(TAG, "Emulador 555 inicializado con salida en GPIO%d", GPIO_OUTPUT);
}

// Iniciar el modo astable - mejorado
void emulator_start_astable(float r1, float r2, float c) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    if (emulator_state.running) {
        esp_timer_stop(emulator_state.timer);
    }
    
    emulator_state.mode = MODE_ASTABLE;
    emulator_state.r1_astable = r1;
    emulator_state.r2_astable = r2;
    emulator_state.c_astable = c;
    emulator_state.cycle_count = 0;
    emulator_state.measured_frequency = 0.0f;
    emulator_state.measured_duty_cycle = 0.0f;
    
    // Recrear el timer para asegurarnos de que tiene el callback correcto
    esp_timer_create_args_t timer_args = {
        .callback = &astable_timer_callback,
        .name = "555_timer"
    };
    
    if (emulator_state.timer != NULL) {
        esp_timer_delete(emulator_state.timer);
    }
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &emulator_state.timer));
    
    // Calcular el primer intervalo (estado alto)
    uint64_t interval = (uint64_t)(0.693f * (r1 + r2) * c * 1000000);
    
    xSemaphoreGive(emulator_mutex);
    
    // Comenzar con estado alto
    update_output_state(true);
    
    // Iniciar el timer
    esp_timer_start_once(emulator_state.timer, interval);
    
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    emulator_state.running = true;
    xSemaphoreGive(emulator_mutex);
    
    ESP_LOGI(TAG, "Modo Astable iniciado - R1: %.2f Ohms, R2: %.2f Ohms, C: %.6f F", r1, r2, c);
}

// Iniciar el modo monostable - mejorado
void emulator_trigger_monostable(float r, float c) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    if (emulator_state.running) {
        esp_timer_stop(emulator_state.timer);
    }
    
    emulator_state.mode = MODE_MONOSTABLE;
    emulator_state.r_monostable = r;
    emulator_state.c_monostable = c;
    
    // Recrear el timer para asegurarnos de que tiene el callback correcto
    esp_timer_create_args_t timer_args = {
        .callback = &monostable_timer_callback,
        .name = "555_timer"
    };
    
    if (emulator_state.timer != NULL) {
        esp_timer_delete(emulator_state.timer);
    }
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &emulator_state.timer));
    
    // Calcular la duración del pulso
    uint64_t interval = (uint64_t)(1.1f * r * c * 1000000);
    
    xSemaphoreGive(emulator_mutex);
    
    // Activar la salida (estado alto)
    update_output_state(true);
    
    // Iniciar el timer para volver a estado bajo después del intervalo
    esp_timer_start_once(emulator_state.timer, interval);
    
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    emulator_state.running = true;
    xSemaphoreGive(emulator_mutex);
    
    ESP_LOGI(TAG, "Monoestable activado - R: %.2f Ohms, C: %.6f F, Duración: %.2f ms", 
             r, c, (1.1f * r * c * 1000));
}

// Iniciar el modo PWM - mejorado
void emulator_start_pwm(float r_fixed, float c) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    if (emulator_state.running) {
        esp_timer_stop(emulator_state.timer);
    }
    
    emulator_state.mode = MODE_PWM;
    emulator_state.r_fixed = r_fixed;
    emulator_state.c_pwm = c;
    emulator_state.cycle_count = 0;
    emulator_state.measured_frequency = 0.0f;
    emulator_state.measured_duty_cycle = 0.0f;
    
    // Obtener el valor actual del potenciómetro
    emulator_state.r_variable = read_potentiometer();
    
    // Recrear el timer con el callback correcto
    esp_timer_create_args_t timer_args = {
        .callback = &pwm_timer_callback,
        .name = "555_timer"
    };
    
    if (emulator_state.timer != NULL) {
        esp_timer_delete(emulator_state.timer);
    }
    
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &emulator_state.timer));
    
    // Calcular el primer intervalo (estado alto)
    uint64_t interval = (uint64_t)(0.693f * (emulator_state.r_variable + r_fixed) * c * 1000000);
    
    xSemaphoreGive(emulator_mutex);
    
    // Comenzar con estado alto
    update_output_state(true);
    
    // Iniciar el timer
    esp_timer_start_once(emulator_state.timer, interval);
    
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    emulator_state.running = true;
    xSemaphoreGive(emulator_mutex);
    
    ESP_LOGI(TAG, "Modo PWM iniciado - RFixed: %.2f Ohms, RVar: %.2f Ohms, C: %.6f F", 
             r_fixed, emulator_state.r_variable, c);
}

// Detener el emulador - mejorado
void emulator_stop(void) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    if (emulator_state.running) {
        esp_timer_stop(emulator_state.timer);
        emulator_state.running = false;
    }
    
    // También podríamos mantener el modo actual pero indicar que no está en ejecución
    
    xSemaphoreGive(emulator_mutex);
    
    // Apagar la salida
    update_output_state(false);
    
    ESP_LOGI(TAG, "Emulador detenido");
}

// Obtener parámetros actuales - mejorado
void emulator_get_parameters(emulator_mode_t *mode, float *frequency, float *duty_cycle, float *pulse_width) {
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    // Valores por defecto
    *frequency = 0.0f;
    *duty_cycle = 0.0f;
    *pulse_width = 0.0f;
    
    *mode = emulator_state.mode;
    
    switch (emulator_state.mode) {
        case MODE_ASTABLE:
            // Si tenemos suficientes ciclos para mediciones reales, usar valores medidos
            if (emulator_state.cycle_count > 2 && emulator_state.measured_frequency > 0) {
                *frequency = emulator_state.measured_frequency;
                *duty_cycle = emulator_state.measured_duty_cycle;
            } else {
                // De lo contrario, usar valores teóricos
                calculate_astable(emulator_state.r1_astable, emulator_state.r2_astable, 
                                 emulator_state.c_astable, frequency, duty_cycle);
            }
            break;
        case MODE_MONOSTABLE:
            *pulse_width = calculate_monostable(emulator_state.r_monostable, emulator_state.c_monostable);
            break;
        case MODE_PWM:
            // Similar al astable, usar valores medidos si están disponibles
            if (emulator_state.cycle_count > 2 && emulator_state.measured_frequency > 0) {
                *frequency = emulator_state.measured_frequency;
                *duty_cycle = emulator_state.measured_duty_cycle;
            } else {
                calculate_pwm(emulator_state.r_variable, emulator_state.r_fixed, 
                             emulator_state.c_pwm, frequency, duty_cycle);
            }
            break;
        default:
            break;
    }
    
    xSemaphoreGive(emulator_mutex);
}

// Inicializar NVS (almacenamiento no volátil) - sin cambios
static void initialize_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// Evento de WiFi - mejorado con más información
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" conectada, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación "MACSTR" desconectada, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Punto de acceso WiFi iniciado: %s", WIFI_SSID);
    }
}

// Inicialización del punto de acceso WiFi - mejorado
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP iniciado. SSID:%s Contraseña:%s Canal:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
    
 // Obtener y mostrar la dirección IP del AP
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
        ESP_LOGI(TAG, "Dirección IP del AP: " IPSTR, IP2STR(&ip_info.ip));
    }
}

// Manejador para la página principal - sin cambios
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

// Manejador para obtener los parámetros actuales - corregido formato de %u
static esp_err_t get_parameters_handler(httpd_req_t *req)
{
    emulator_mode_t mode;
    float frequency = 0;
    float duty_cycle = 0;
    float pulse_width = 0;
    
    emulator_get_parameters(&mode, &frequency, &duty_cycle, &pulse_width);
    
    char response[200];  // Buffer más grande para incluir más información
    
    if (mode == MODE_MONOSTABLE) {
        snprintf(response, sizeof(response), 
                 "{\"mode\":\"monostable\",\"pulseWidth\":%.6f,\"running\":%s,\"outputPin\":%d}", 
                 pulse_width, emulator_state.running ? "true" : "false", GPIO_OUTPUT);
    } else if (mode == MODE_ASTABLE || mode == MODE_PWM) {
        const char* mode_str = (mode == MODE_ASTABLE) ? "astable" : "pwm";
        snprintf(response, sizeof(response), 
                 "{\"mode\":\"%s\",\"frequency\":%.2f,\"dutyCycle\":%.2f,\"running\":%s,\"outputPin\":%d}", 
                 mode_str, frequency, duty_cycle, emulator_state.running ? "true" : "false", GPIO_OUTPUT);
    } else {
        // Modo NONE o cualquier otro modo no reconocido
        snprintf(response, sizeof(response),
                 "{\"mode\":\"none\",\"running\":false,\"outputPin\":%d}",
                 GPIO_OUTPUT);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Manejador para configurar el modo astable - mejorado con validación
static esp_err_t set_astable_handler(httpd_req_t *req)
{
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    float r1 = 0, r2 = 0, c = 0;
    if (sscanf(buf, "{\"r1\":%f,\"r2\":%f,\"c\":%f}", &r1, &r2, &c) != 3) {
        // Error en el formato JSON
        const char* error_response = "{\"error\":\"Formato JSON inválido\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Validar valores
    if (r1 < 100 || r2 < 100 || c <= 0) {
        const char* error_response = "{\"error\":\"Valores fuera de rango\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Convertir c de μF a F
    c = c * 0.000001;
    
    ESP_LOGI(TAG, "Configurando modo astable - R1: %.2f, R2: %.2f, C: %.8f", r1, r2, c);
    
    emulator_start_astable(r1, r2, c);
    
    float frequency, duty_cycle;
    calculate_astable(r1, r2, c, &frequency, &duty_cycle);
    
    char response[200];
    snprintf(response, sizeof(response), 
             "{\"frequency\":%.2f,\"dutyCycle\":%.2f,\"running\":true,\"outputPin\":%d}", 
             frequency, duty_cycle, GPIO_OUTPUT);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Manejador para activar el modo monostable - mejorado con validación
static esp_err_t trigger_monostable_handler(httpd_req_t *req)
{
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    float r = 0, c = 0;
    if (sscanf(buf, "{\"r\":%f,\"c\":%f}", &r, &c) != 2) {
        // Error en el formato JSON
        const char* error_response = "{\"error\":\"Formato JSON inválido\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Validar valores
    if (r < 100 || c <= 0) {
        const char* error_response = "{\"error\":\"Valores fuera de rango\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Convertir c de μF a F
    c = c * 0.000001;
    
    ESP_LOGI(TAG, "Activando monoestable - R: %.2f, C: %.8f", r, c);
    
    emulator_trigger_monostable(r, c);
    
    float pulse_width = calculate_monostable(r, c);
    
    char response[200];
    snprintf(response, sizeof(response), 
             "{\"pulseWidth\":%.6f,\"running\":true,\"outputPin\":%d}", 
             pulse_width, GPIO_OUTPUT);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Manejador para configurar el modo PWM - mejorado con validación
static esp_err_t set_pwm_handler(httpd_req_t *req)
{
    char buf[200];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    float r_fixed = 0, c = 0;
    if (sscanf(buf, "{\"rFixed\":%f,\"c\":%f}", &r_fixed, &c) != 2) {
        // Error en el formato JSON
        const char* error_response = "{\"error\":\"Formato JSON inválido\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Validar valores
    if (r_fixed < 100 || c <= 0) {
        const char* error_response = "{\"error\":\"Valores fuera de rango\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, error_response, strlen(error_response));
        return ESP_OK;
    }
    
    // Convertir c de μF a F
    c = c * 0.000001;
    
    ESP_LOGI(TAG, "Configurando modo PWM - RFixed: %.2f, C: %.8f", r_fixed, c);
    
    // Leer el valor actual del potenciómetro antes de iniciar
    float r_variable = read_potentiometer();
    ESP_LOGI(TAG, "Valor del potenciómetro: %.2f Ohms", r_variable);
    
    emulator_start_pwm(r_fixed, c);
    
    float frequency, duty_cycle;
    calculate_pwm(r_variable, r_fixed, c, &frequency, &duty_cycle);
    
    char response[200];
    snprintf(response, sizeof(response), 
             "{\"frequency\":%.2f,\"dutyCycle\":%.2f,\"rVariable\":%.2f,\"running\":true,\"outputPin\":%d}", 
             frequency, duty_cycle, r_variable, GPIO_OUTPUT);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Manejador para detener el emulador - mejorado con confirmación
static esp_err_t stop_emulator_handler(httpd_req_t *req)
{
    emulator_stop();
    
    char response[100];
    snprintf(response, sizeof(response),
             "{\"running\":false,\"outputPin\":%d,\"status\":\"stopped\"}",
             GPIO_OUTPUT);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Manejador para obtener el status del emulador (nuevo) - corregido formato de %u a %lu
static esp_err_t status_handler(httpd_req_t *req)
{
    xSemaphoreTake(emulator_mutex, portMAX_DELAY);
    
    char status_str[20];
    switch (emulator_state.mode) {
        case MODE_ASTABLE:
            strcpy(status_str, "astable");
            break;
        case MODE_MONOSTABLE:
            strcpy(status_str, "monostable");
            break;
        case MODE_PWM:
            strcpy(status_str, "pwm");
            break;
        default:
            strcpy(status_str, "none");
            break;
    }
    
    char response[200];
    snprintf(response, sizeof(response),
             "{\"mode\":\"%s\",\"running\":%s,\"outputPin\":%d,\"outputState\":%s,\"cycleCount\":%lu}",
             status_str,
             emulator_state.running ? "true" : "false",
             GPIO_OUTPUT,
             emulator_state.output_state ? "true" : "false",
             (unsigned long)emulator_state.cycle_count);
    
    xSemaphoreGive(emulator_mutex);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

// Inicializar el servidor web - mejorado con nuevos endpoints
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Iniciando servidor web en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Página principal
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        // API para obtener parámetros
        httpd_uri_t get_params_uri = {
            .uri       = "/api/parameters",
            .method    = HTTP_GET,
            .handler   = get_parameters_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &get_params_uri);

        // API para modo astable
        httpd_uri_t astable_uri = {
            .uri       = "/api/astable",
            .method    = HTTP_POST,
            .handler   = set_astable_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &astable_uri);

        // API para modo monostable
        httpd_uri_t monostable_uri = {
            .uri       = "/api/monostable",
            .method    = HTTP_POST,
            .handler   = trigger_monostable_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &monostable_uri);

        // API para modo PWM
        httpd_uri_t pwm_uri = {
            .uri       = "/api/pwm",
            .method    = HTTP_POST,
            .handler   = set_pwm_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &pwm_uri);

        // API para detener el emulador
        httpd_uri_t stop_uri = {
            .uri       = "/api/stop",
            .method    = HTTP_POST,
            .handler   = stop_emulator_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stop_uri);
        
        // API para obtener status (nuevo)
        httpd_uri_t status_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = status_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &status_uri);

        ESP_LOGI(TAG, "Servidor web iniciado correctamente");
        return server;
    }

    ESP_LOGE(TAG, "Error al iniciar el servidor web");
    return NULL;
}

// Tarea para monitoreo periódico del sistema - corregido formato de %u a %lu
void monitoring_task(void *pvParameters)
{
    while (1) {
        // Mostrar información del estado cada 5 segundos
        if (emulator_state.running) {
            emulator_mode_t mode;
            float frequency = 0, duty_cycle = 0, pulse_width = 0;
            
            emulator_get_parameters(&mode, &frequency, &duty_cycle, &pulse_width);
            
            if (mode == MODE_ASTABLE) {
                ESP_LOGI(TAG, "Estado: Astable, Frecuencia: %.2f Hz, Ciclo: %.2f%%", 
                         frequency, duty_cycle);
            } else if (mode == MODE_MONOSTABLE) {
                ESP_LOGI(TAG, "Estado: Monoestable, Ancho de pulso: %.2f ms", 
                         pulse_width * 1000.0f);
            } else if (mode == MODE_PWM) {
                ESP_LOGI(TAG, "Estado: PWM, Frecuencia: %.2f Hz, Ciclo: %.2f%%, RVar: %.2f Ω", 
                         frequency, duty_cycle, emulator_state.r_variable);
            }
        }
        
        // Verificar el nivel de memoria libre - corregido formato
        ESP_LOGD(TAG, "Heap libre: %lu bytes", (unsigned long)esp_get_free_heap_size());
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // Inicializar NVS
    initialize_nvs();
    
    // Inicializar WiFi como punto de acceso
    wifi_init_softap();
    
    // Inicializar el potenciómetro (ADC)
    potentiometer_init();
    
    // Inicializar el emulador 555
    emulator_init();
    
    // Iniciar servidor web
    start_webserver();
    
    // Crear tarea de monitoreo
    xTaskCreate(monitoring_task, "monitoring_task", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Emulador de 555 iniciado con éxito.");
    ESP_LOGI(TAG, "- Pin de salida: GPIO%d", GPIO_OUTPUT);
    ESP_LOGI(TAG, "- Pin de trigger para monoestable: GPIO%d", MONOSTABLE_TRIGGER_PIN);
    ESP_LOGI(TAG, "- Potenciómetro en GPIO34 (ADC1_CH6)");
    ESP_LOGI(TAG, "- Conectar a WiFi: %s, contraseña: %s", WIFI_SSID, WIFI_PASS);
}