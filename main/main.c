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

SemaphoreHandle_t xSemaphore_acelerar;
SemaphoreHandle_t xSemaphore_frear;

volatile int acelerando = 0;
volatile int freiando = 0;

typedef struct adc {
    int axis;
    int val;
} adc_t;

QueueHandle_t xQueueADC;

// ---------- CALLBACK ----------

void btn_callback(uint gpio, uint32_t events) {
    if (gpio == BTN_ACELERADOR){
        if (events == GPIO_IRQ_EDGE_FALL) { // fall edge
            acelerando = 1;
            xSemaphoreGiveFromISR(xSemaphore_acelerar, 0);
        } else if (events == GPIO_IRQ_EDGE_RISE){
            acelerando = 0;
        }
    } else if (gpio == BTN_FREIO){
        if (events == GPIO_IRQ_EDGE_FALL) { // fall edge
            freiando = 1;
            xSemaphoreGiveFromISR(xSemaphore_frear, 0);
        } else if (events == GPIO_IRQ_EDGE_RISE){
            freiando = 0;
        }
    }
}

// ---------- TASKS ----------

void acelerador_task(void *p) {
    gpio_init(BTN_ACELERADOR);
    gpio_set_dir(BTN_ACELERADOR, GPIO_IN);
    gpio_pull_up(BTN_ACELERADOR);
    gpio_set_irq_enabled_with_callback(BTN_ACELERADOR, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    while (1) {
        // if (xSemaphoreTake(xSemaphore_acelerar, pdMS_TO_TICKS(500)) == pdTRUE) {
        //     while (acelerando) {
        //         printf(">> Acelerando...\n");
        //         vTaskDelay(pdMS_TO_TICKS(100));
        //     }
            
        //     vTaskDelay(pdMS_TO_TICKS(100));
        // }

        if (acelerando) {
            //printf(">> Acelerando...\n");
            adc_t dado = { .axis = 1, .val = 3 };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void freio_task(void *p) {
    gpio_init(BTN_FREIO);
    gpio_set_dir(BTN_FREIO, GPIO_IN);
    gpio_pull_up(BTN_FREIO);
    gpio_set_irq_enabled_with_callback(BTN_FREIO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &btn_callback);

    while (1) {
        //if (xSemaphoreTake(xSemaphore_frear, pdMS_TO_TICKS(500)) == pdTRUE) {
        //    while (freiando) {
        //        printf(">> Freando...\n");
        //        vTaskDelay(pdMS_TO_TICKS(100));
        //    }

        //    vTaskDelay(pdMS_TO_TICKS(100));
        //}
        if (freiando) {
            //printf(">> Freando...\n");
            adc_t dado = { .axis = 1, .val = 1 };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void potenciometro_task(void *p) {
    adc_init();
    adc_gpio_init(27); // GPIO 27 = ADC1
    adc_select_input(1);

    while (1) {
        int leitura = adc_read();
        //printf("POT: %d\n", leitura);
        if(leitura < 2200){
            adc_t dado = { .axis = 0, .val = 1 };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
        } else if (leitura > 2400){
            adc_t dado = { .axis = 0, .val = 3 };
            xQueueSend(xQueueADC, &dado, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // evita flood no terminal
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
            uint8_t eop = 0xFF;

            putchar_raw(axis & 0x01);
            putchar_raw(val_0);
            putchar_raw(val_1);
            putchar_raw(eop);
        }
    }
}

// ---------- MAIN ----------

int main() {
    stdio_init_all();
    sleep_ms(2000); // Aguarda inicialização da USB para evitar perda de prints

    // Semáforos
    xSemaphore_acelerar = xSemaphoreCreateBinary();
    xSemaphore_frear = xSemaphoreCreateBinary();

    if (xSemaphore_acelerar == NULL || xSemaphore_frear == NULL) {
        printf("Erro ao criar semáforos.\n");
        return 1;
    }

    // Criação de tasks
    xTaskCreate(acelerador_task, "Acelerador Task", 4096, NULL, 1, NULL);
    xTaskCreate(freio_task, "Freio Task", 4096, NULL, 1, NULL);
    xTaskCreate(potenciometro_task, "Potenciômetro Task", 4096, NULL, 1, NULL);
    xTaskCreate(uart_task, "UART Task", 2048, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        tight_loop_contents(); // não deve chegar aqui
}
