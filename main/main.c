#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>

const int BTN_ACELERADOR = 15;
const int BTN_FREIO = 14;

const int LED_ACELERADOR = 13;
const int LED_FREIO = 12;

// const int LED_CONEXAO = 11;

volatile int acelerando = 0;
volatile int freiando = 0;
volatile int mudanca_a = 0;
volatile int mudanca_f = 0;

typedef struct adc {
    int axis;
    int val;
} adc_t;

QueueHandle_t xQueueADC;

int16_t melhora_resolucao(int16_t valor) {
    int x = valor - (4082/2);
    x = (x*255)/(4082/2);
    return x;
}

// ---------- CALLBACK ----------

void btn_callback(uint gpio, uint32_t events) {
    if (gpio == BTN_ACELERADOR){
        if (events == GPIO_IRQ_EDGE_FALL) { // fall edge
            acelerando = 1;
            mudanca_a = !mudanca_a;
        } else if (events == GPIO_IRQ_EDGE_RISE){
            acelerando = 0;
            mudanca_a = !mudanca_a;
        }
    } else if (gpio == BTN_FREIO){
        if (events == GPIO_IRQ_EDGE_FALL) { // fall edge
            freiando = 1;
            mudanca_f = !mudanca_f;
        } else if (events == GPIO_IRQ_EDGE_RISE){
            freiando = 0;
            mudanca_f = !mudanca_f;
        }
    }
}

// ---------- TASKS ----------

void acelerador_task(void *p) {
    gpio_init(BTN_ACELERADOR);
    gpio_init(LED_ACELERADOR);
    gpio_set_dir(BTN_ACELERADOR, GPIO_IN);
    gpio_set_dir(LED_ACELERADOR, GPIO_OUT);
    gpio_pull_up(BTN_ACELERADOR);
    gpio_set_irq_enabled_with_callback(BTN_ACELERADOR, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    while (1) {
        int valor_acelerador = 0;

        if (acelerando) {
            //printf(">> Acelerando...\n");
            gpio_put(LED_ACELERADOR, 1);
            valor_acelerador = 100;
        } else {
            gpio_put(LED_ACELERADOR, 0);
            valor_acelerador = 0;
        }

        if (mudanca_a) {
            adc_t dado = { .axis = 1, .val = valor_acelerador };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_a = !mudanca_a;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void freio_task(void *p) {
    gpio_init(BTN_FREIO);
    gpio_init(LED_FREIO);
    gpio_set_dir(BTN_FREIO, GPIO_IN);
    gpio_set_dir(LED_FREIO, GPIO_OUT);
    gpio_pull_up(BTN_FREIO);
    gpio_set_irq_enabled_with_callback(BTN_FREIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    while (1) {
        int valor_freio = 0;

        if (freiando) {
            //printf(">> Freando...\n");
            gpio_put(LED_FREIO, 1);
            valor_freio = -100;
        } else {
            gpio_put(LED_FREIO, 0);
            valor_freio = 0;
        }

        if (mudanca_f) {
            adc_t dado = { .axis = 1, .val = valor_freio };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_f = !mudanca_f;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// void volante_task(void *p) {
//     adc_init();
//     adc_gpio_init(27); // GPIO 27 = ADC1
//     adc_select_input(1);

//     int leitura_anterior = 0;
//     const int zona_morta = 0;
//     const int leitura_min = 300;  // Ajuste conforme seu teste real
//     const int leitura_max = 3800; // Ajuste conforme seu teste real

//     while (1) {
//         int leitura = adc_read();
//         printf("POT_antes: %d\n", leitura);

//         // Saturação dentro da faixa útil
//         if (leitura < leitura_min) leitura = leitura_min;
//         if (leitura > leitura_max) leitura = leitura_max;

//         // Normaliza para -100 a 100
//         float faixa = leitura_max - leitura_min;
//         float pos = (leitura - leitura_min) / faixa; // 0.0 a 1.0
//         int valor_normalizado = (int)((pos - 0.5f) * 200); // -100 a 100
//         printf("POT: %d\n", valor_normalizado);

//         if (abs(valor_normalizado) < zona_morta) {
//             valor_normalizado = 0;
//         }

//         if (abs(valor_normalizado - leitura_anterior) > 2) {
//             adc_t dado = { .axis = 2, .val = valor_normalizado };
//             xQueueSend(xQueueADC, &dado, portMAX_DELAY);
//             leitura_anterior = valor_normalizado;
//         }

//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

// void boost_task(void *p) {
//     adc_init();
//     adc_gpio_init(26); // GPIO 26 = ADC0
//     adc_select_input(0);

//     int leitura_anterior = 0;
//     int mudanca_b = 0;
//     int boost = 0;
//     int valor_anterior = 0;

//     while (1) {
//         int leitura = adc_read();
//         printf("BOOST_antes: %d\n", leitura);


//         if (leitura < 29) {
//             boost = 0;
//             if (boost != valor_anterior) {
//                 mudanca_b = 1;
//             }
//         } else {
//             boost = 100;
//             if (boost != valor_anterior) {
//                 mudanca_b = 1;
//             }
//         }

//         valor_anterior = boost;

//         if (mudanca_b) {
//             adc_t dado = { .axis = 3, .val = boost };
//             xQueueSend(xQueueADC, &dado, 0);
//             mudanca_b = 0;
//         }

//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

void adc_task() {
    adc_init();
    adc_gpio_init(26); // GPIO 26 = ADC0
    adc_gpio_init(27); // GPIO 27 = ADC1
    
    int leitura_anterior = 0;
    const int zona_morta = 0;
    const int leitura_min = 300;  // Ajuste conforme seu teste real
    const int leitura_max = 3800; // Ajuste conforme seu teste real
    
    int mudanca_b = 0;
    int boost = 0;
    int valor_anterior = 0;
    
    while (1) {
        
        // VOLANTE
        adc_select_input(1);
        int leitura = adc_read();
        // printf("POT_antes: %d\n", leitura);

        // Saturação dentro da faixa útil
        if (leitura < leitura_min) leitura = leitura_min;
        if (leitura > leitura_max) leitura = leitura_max;

        // Normaliza para -100 a 100
        float faixa = leitura_max - leitura_min;
        float pos = (leitura - leitura_min) / faixa; // 0.0 a 1.0
        int valor_normalizado = (int)((pos - 0.5f) * 200); // -100 a 100
        // printf("POT: %d\n", valor_normalizado);

        if (abs(valor_normalizado) < zona_morta) {
            valor_normalizado = 0;
        }

        if (abs(valor_normalizado - leitura_anterior) > 2) {
            adc_t dado = { .axis = 2, .val = valor_normalizado };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
            leitura_anterior = valor_normalizado;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        // BOOST
        adc_select_input(0);
        int leitura_boost = adc_read();
        printf("BOOST_antes: %d\n", leitura_boost);
        
        
        if (leitura_boost < 29) {
            boost = 0;
            printf("DESLIGADO\n");
            if (boost != valor_anterior) {
                mudanca_b = 1;
            }
        } else {
            boost = 100;
            printf("LIGADO\n");
            if (boost != valor_anterior) {
                mudanca_b = 1;
            }
        }

        valor_anterior = boost;

        if (mudanca_b) {
            adc_t dado = { .axis = 3, .val = boost };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_b = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void uart_task(void *p) {
    adc_t recebido;

    while (1) {
        if (xQueueReceive(xQueueADC, &recebido, portMAX_DELAY)) {
            int16_t valor = (int16_t)recebido.val;

            uint8_t axis = (uint8_t)recebido.axis;
            uint8_t val_0 = (uint8_t)(valor & 0xFF);
            uint8_t val_1 = (uint8_t)((valor >> 8) & 0xFF);

            uint8_t header = 0xAA;
            uint8_t msg_type = 0x01; // MSG_ANALOG
            uint8_t payload_size = 3;
            uint8_t payload[3] = { axis, val_1, val_0 }; // ATENÇÃO: MSB primeiro depois LSB
            uint8_t checksum = msg_type ^ payload_size ^ payload[0] ^ payload[1] ^ payload[2];
            uint8_t eop = 0xFF;

            // Envia os dados na ordem correta
            putchar_raw(header);
            putchar_raw(msg_type);
            putchar_raw(payload_size);
            putchar_raw(payload[0]);
            putchar_raw(payload[1]);
            putchar_raw(payload[2]);
            putchar_raw(checksum);
            putchar_raw(eop);
        }
    }
}

// ---------- MAIN ----------

int main() {
    stdio_init_all();
    sleep_ms(2000); // Aguarda inicialização da USB para evitar perda de prints

    xQueueADC = xQueueCreate(32, sizeof(adc_t));

    if (xQueueADC == NULL) {
        printf("Erro ao criar fila xQueueADC.\n");
        return 1;
    }

    // Criação de tasks
    xTaskCreate(acelerador_task, "Acelerador Task", 1024, NULL, 1, NULL);
    xTaskCreate(freio_task, "Freio Task", 1024, NULL, 1, NULL);
    // xTaskCreate(volante_task, "Volante Task", 1024, NULL, 1, NULL);
    // xTaskCreate(boost_task, "Boost Task", 1024, NULL, 1, NULL);
    xTaskCreate(adc_task, "ADC Task", 1024, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        tight_loop_contents(); // não deve chegar aqui
}
