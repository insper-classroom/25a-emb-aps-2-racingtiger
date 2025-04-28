#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>

#include "hc06.c"

const int BTN_ACELERADOR = 15;
const int BTN_FREIO = 14;
const int BTN_RESET = 16;
const int BTN_START = 17;

const int LED_ACELERADOR = 13;
const int LED_FREIO = 12;
const int LED_CONEXAO = 11;

volatile int acelerando = 0;
volatile int freiando = 0;
volatile int mudanca_a = 0;
volatile int mudanca_f = 0;
volatile int reset = 0;
volatile int start = 0;

typedef struct adc {
    int axis;
    int val;
} adc_t;

QueueHandle_t xQueueADC;

// ---------- CALLBACK ----------

void btn_callback(uint gpio, uint32_t events) {
    if (gpio == BTN_ACELERADOR){
        if (events == GPIO_IRQ_EDGE_FALL) {
            acelerando = 1;
            mudanca_a = !mudanca_a;
        } else if (events == GPIO_IRQ_EDGE_RISE){
            acelerando = 0;
            mudanca_a = !mudanca_a;
        }
    } else if (gpio == BTN_FREIO){
        if (events == GPIO_IRQ_EDGE_FALL) {
            freiando = 1;
            mudanca_f = !mudanca_f;
        } else if (events == GPIO_IRQ_EDGE_RISE){
            freiando = 0;
            mudanca_f = !mudanca_f;
        }
    } else if (gpio == BTN_RESET){
        if (events == GPIO_IRQ_EDGE_FALL) {
            reset = 1;
        } else if (events == GPIO_IRQ_EDGE_RISE){
            reset = 0;
        }
    } else if (gpio == BTN_START){
        if (events == GPIO_IRQ_EDGE_FALL) {
            start = !start;
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
            gpio_put(LED_ACELERADOR, 1);
            valor_acelerador = 100;
        } else {
            gpio_put(LED_ACELERADOR, 0);
            valor_acelerador = 0;
        }

        if (mudanca_a && start) {
            adc_t dado = { .axis = 1, .val = valor_acelerador };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_a = !mudanca_a;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
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
            gpio_put(LED_FREIO, 1);
            valor_freio = -100;
        } else {
            gpio_put(LED_FREIO, 0);
            valor_freio = 0;
        }

        if (mudanca_f && start) {
            adc_t dado = { .axis = 1, .val = valor_freio };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_f = !mudanca_f;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void adc_task() {
    adc_init();
    adc_gpio_init(26); // GPIO 26 = ADC0
    adc_gpio_init(27); // GPIO 27 = ADC1
    
    int leitura_anterior = 0;
    const int zona_morta = 0;
    const int leitura_min = 300;
    const int leitura_max = 3800;
    
    int mudanca_b = 0;
    int boost = 0;
    int valor_anterior = 0;
    
    while (1) {
        
        // VOLANTE
        adc_select_input(1);
        int leitura = adc_read();

        if (leitura < leitura_min) leitura = leitura_min;
        if (leitura > leitura_max) leitura = leitura_max;

        float faixa = leitura_max - leitura_min;
        float pos = (leitura - leitura_min) / faixa; // 0.0 a 1.0
        int valor_normalizado = (int)((pos - 0.5f) * 200); // -100 a 100

        if (abs(valor_normalizado) < zona_morta) {
            valor_normalizado = 0;
        }

        if (abs(valor_normalizado - leitura_anterior) > 2 && start) {
            adc_t dado = { .axis = 2, .val = valor_normalizado };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
            leitura_anterior = valor_normalizado;
        }

        // BOOST
        adc_select_input(0);
        int leitura_boost = adc_read();
        
        if (leitura_boost < 29) {
            boost = 0;
            if (boost != valor_anterior) {
                mudanca_b = 1;
            }
        } else {
            boost = 100;
            if (boost != valor_anterior) {
                mudanca_b = 1;
            }
        }

        valor_anterior = boost;

        if (mudanca_b && start) {
            adc_t dado = { .axis = 3, .val = boost };
            xQueueSend(xQueueADC, &dado, 0);
            mudanca_b = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void reset_task(void *p) {
    gpio_init(BTN_RESET);
    gpio_set_dir(BTN_RESET, GPIO_IN);
    gpio_pull_up(BTN_RESET);
    gpio_set_irq_enabled_with_callback(BTN_RESET, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    while (1) {
        if (reset && start) {
            adc_t dado = { .axis = 4, .val = 100 };
            xQueueSend(xQueueADC, &dado, 0);
            reset = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void start_task(void *p) {
    gpio_init(BTN_START);
    gpio_init(LED_CONEXAO);
    gpio_set_dir(BTN_START, GPIO_IN);
    gpio_set_dir(LED_CONEXAO, GPIO_OUT);
    gpio_pull_up(BTN_START);
    gpio_set_irq_enabled_with_callback(BTN_START, GPIO_IRQ_EDGE_FALL, true, &btn_callback);

    while (1) {
        if (start) {
            gpio_put(LED_CONEXAO, 1);
        } else {
            gpio_put(LED_CONEXAO, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void hc06_task(void *p) {
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(HC06_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HC06_RX_PIN, GPIO_FUNC_UART);
    hc06_init("Liriri Larila", "2503");

    adc_t recebido;
    int led_state = 1;
    int inicio = 1;

    while (true) {
        if (xQueueReceive(xQueueADC, &recebido, portMAX_DELAY) && start) {
            int16_t valor = (int16_t)recebido.val;

            uint8_t axis = (uint8_t)recebido.axis;
            uint8_t val_0 = (uint8_t)(valor & 0xFF);
            uint8_t val_1 = (uint8_t)((valor >> 8) & 0xFF);

            uint8_t header = 0xAA;
            uint8_t msg_type = 0x01;
            uint8_t payload_size = 3;
            uint8_t payload[3] = { axis, val_1, val_0 };
            uint8_t checksum = msg_type ^ payload_size ^ payload[0] ^ payload[1] ^ payload[2];
            uint8_t eop = 0xFF;
            
            uint8_t pacote[8] = {
                header,
                msg_type,
                payload_size,
                payload[0],
                payload[1],
                payload[2],
                checksum,
                eop
            };
            
            uart_write_blocking(HC06_UART_ID, pacote, sizeof(pacote));
        }
    }
}

// ---------- MAIN ----------

int main() {
    stdio_init_all();
    sleep_ms(2000);

    xQueueADC = xQueueCreate(32, sizeof(adc_t));

    if (xQueueADC == NULL) {
        printf("Erro ao criar fila xQueueADC.\n");
        return 1;
    }

    xTaskCreate(acelerador_task, "Acelerador Task", 1024, NULL, 1, NULL);
    xTaskCreate(freio_task, "Freio Task", 1024, NULL, 1, NULL);
    xTaskCreate(adc_task, "ADC Task", 2048, NULL, 1, NULL);
    xTaskCreate(reset_task, "Reset Task", 1024, NULL, 1, NULL);
    xTaskCreate(start_task, "Start Task", 1024, NULL, 1, NULL);
    xTaskCreate(hc06_task, "HC06 Task", 2048, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        tight_loop_contents(); // não deve chegar aqui
}
