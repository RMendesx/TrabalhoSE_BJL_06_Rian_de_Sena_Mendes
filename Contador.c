// Bibliotecas padrão da Raspberry Pi Pico
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"

// Bibliotecas de hardware da Pico
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

// Bibliotecas auxiliares personalizadas (display, LED, buzzer etc.)
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/led.h"
#include "lib/buzzer.h"

// FreeRTOS (sistema operacional em tempo real)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Definições dos pinos e constantes do projeto
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define BUTTON_A 5
#define BUTTON_B 6
#define BUTTON_J 22

#define LED_GREEN 11
#define LED_BLUE 12
#define LED_RED 13

#define BUZZER_A 10
#define BUZZER_B 21

#define MAX_USUARIOS 8

// Variáveis globais
ssd1306_t ssd;
SemaphoreHandle_t xSemaforoContagem;
SemaphoreHandle_t xSemaforoReset;
SemaphoreHandle_t xMutexDisplay;
SemaphoreHandle_t xMutexAcao;

volatile uint8_t usuarios_ativos = 0;
volatile int ultima_acao = 0;

// Tarefa responsável pela entrada de usuários
void vTaskEntrada(void *params)
{
    char buffer[32];
    int acao;

    while (true)
    {
        // Aguarda semáforo de contagem
        if (xSemaphoreTake(xSemaforoContagem, portMAX_DELAY) == pdTRUE)
        {
            // Garante acesso à variável de ação
            if (xSemaphoreTake(xMutexAcao, portMAX_DELAY) == pdTRUE)
            {
                acao = ultima_acao;
                xSemaphoreGive(xMutexAcao);
            }

            // Entrada permitida se houver vagas
            if (acao == 1 && usuarios_ativos < MAX_USUARIOS)
            {
                usuarios_ativos++;
                if (xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE)
                {
                    sprintf(buffer, "%d/%d", usuarios_ativos, MAX_USUARIOS);
                    ssd1306_draw_string(&ssd, buffer, 93, 24);
                    ssd1306_draw_string(&ssd, " ENTRADA ", 32, 44);
                    ssd1306_send_data(&ssd);

                    vTaskDelay(pdMS_TO_TICKS(600));
                    ssd1306_draw_string(&ssd, "         ", 32, 44);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xMutexDisplay);
                }
            }
            // Caso lotado, emite alarme e exibe mensagem
            else if (acao == 1 && usuarios_ativos >= MAX_USUARIOS)
            {
                buzzer_alarm();
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_alarm_off();

                if (xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE)
                {
                    sprintf(buffer, "%d/%d", usuarios_ativos, MAX_USUARIOS);
                    ssd1306_draw_string(&ssd, buffer, 93, 24);
                    ssd1306_draw_string(&ssd, " LOTADO! ", 32, 44);
                    ssd1306_send_data(&ssd);

                    vTaskDelay(pdMS_TO_TICKS(600));
                    ssd1306_draw_string(&ssd, "         ", 32, 44);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xMutexDisplay);
                }
            }
        }
    }
}

// Tarefa responsável pela saída de usuários
void vTaskSaida(void *params)
{
    char buffer[32];
    int acao;

    while (true)
    {
        // Aguarda semáforo de contagem
        if (xSemaphoreTake(xSemaforoContagem, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(xMutexAcao, portMAX_DELAY) == pdTRUE)
            {
                acao = ultima_acao;
                xSemaphoreGive(xMutexAcao);
            }

            // Saída permitida se houver usuários ativos
            if (acao == 2 && usuarios_ativos > 0)
            {
                usuarios_ativos--;
                if (xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE)
                {
                    sprintf(buffer, "%d/%d", usuarios_ativos, MAX_USUARIOS);
                    ssd1306_draw_string(&ssd, buffer, 93, 24);
                    ssd1306_draw_string(&ssd, "  SAIDA  ", 32, 44);
                    ssd1306_send_data(&ssd);

                    vTaskDelay(pdMS_TO_TICKS(600));
                    ssd1306_draw_string(&ssd, "         ", 32, 44);
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xMutexDisplay);
                }
            }
        }
    }
}

// Tarefa que realiza o reset do contador
void vTaskReset(void *params)
{
    char buffer[32];

    while (true)
    {
        if (xSemaphoreTake(xSemaforoReset, portMAX_DELAY) == pdTRUE)
        {
            usuarios_ativos = 0;

            if (xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE)
            {
                sprintf(buffer, "%d/%d", usuarios_ativos, MAX_USUARIOS);
                ssd1306_draw_string(&ssd, buffer, 93, 24);
                ssd1306_draw_string(&ssd, "RESETADO!", 32, 44);
                ssd1306_send_data(&ssd);

                // Alarme sonoro (duplo)
                buzzer_alarm();
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_alarm_off();
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_alarm();
                vTaskDelay(pdMS_TO_TICKS(200));
                buzzer_alarm_off();

                ssd1306_draw_string(&ssd, "         ", 32, 44);
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xMutexDisplay);

                gpio_set_irq_enabled(BUTTON_J, GPIO_IRQ_EDGE_FALL, true);
            }

            // Limpa semáforos acumulados para evitar ações antigas
            while (uxSemaphoreGetCount(xSemaforoContagem) > 0)
            {
                xSemaphoreTake(xSemaforoContagem, 0);
            }
        }
    }
}

// Tarefa que atualiza os LEDs conforme a ocupação
void vTaskPerifericos(void *params)
{
    while (1)
    {
        if (usuarios_ativos == 0)
        {
            led_azul();
        }
        else if (usuarios_ativos > 0 && usuarios_ativos <= (MAX_USUARIOS - 2))
        {
            led_verde();
        }
        else if (usuarios_ativos > 0 && usuarios_ativos <= (MAX_USUARIOS - 1))
        {
            led_amarelo();
        }
        else
        {
            led_vermelho();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
}

// Tarefa que faz leitura dos botões A e B com debounce
void vTaskLeituraBotoes(void *params)
{
    const uint32_t debounce_delay = 5;
    static absolute_time_t last_time_a = 0;
    static absolute_time_t last_time_b = 0;

    while (true)
    {
        // Botão A (entrada)
        if (!gpio_get(BUTTON_A))
        {
            if (absolute_time_diff_us(last_time_a, get_absolute_time()) > 50000)
            {
                last_time_a = get_absolute_time();

                if (xSemaphoreTake(xMutexAcao, portMAX_DELAY) == pdTRUE)
                {
                    ultima_acao = 1;
                    xSemaphoreGive(xMutexAcao);
                }
                xSemaphoreGive(xSemaforoContagem);
            }
        }

        // Botão B (saída)
        if (!gpio_get(BUTTON_B))
        {
            if (absolute_time_diff_us(last_time_b, get_absolute_time()) > 50000)
            {
                last_time_b = get_absolute_time();

                if (xSemaphoreTake(xMutexAcao, portMAX_DELAY) == pdTRUE)
                {
                    ultima_acao = 2;
                    xSemaphoreGive(xMutexAcao);
                }
                xSemaphoreGive(xSemaforoContagem);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(debounce_delay));
    }
}

// Handler de interrupção do botão J (reset)
void gpio_irq_handler(uint gpio, uint32_t events)
{
    static uint32_t last_time_j = 0;
    uint32_t now = time_us_32();
    const uint32_t debounce_delay = 50000;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == BUTTON_J && now - last_time_j > debounce_delay)
    {
        last_time_j = now;

        if (usuarios_ativos > 0)
        {
            gpio_set_irq_enabled(BUTTON_J, GPIO_IRQ_EDGE_FALL, false);
            xSemaphoreGiveFromISR(xSemaforoReset, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

// Função principal
int main()
{
    stdio_init_all();

    // Inicialização do display OLED via I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Inicialização dos botões com pull-up
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    gpio_init(BUTTON_J);
    gpio_set_dir(BUTTON_J, GPIO_IN);
    gpio_pull_up(BUTTON_J);

    gpio_set_irq_enabled_with_callback(BUTTON_J, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Criação de semáforos e mutexes
    xSemaforoContagem = xSemaphoreCreateCounting(MAX_USUARIOS, 0);
    xSemaforoReset = xSemaphoreCreateBinary();
    xMutexDisplay = xSemaphoreCreateMutex();
    xMutexAcao = xSemaphoreCreateMutex();

    // Exibição inicial no display
    char buffer[10];
    sprintf(buffer, "%d/%d", usuarios_ativos, MAX_USUARIOS);

    ssd1306_fill(&ssd, 0);
    ssd1306_rect(&ssd, 3, 3, 122, 58, 1, 0);
    ssd1306_line(&ssd, 3, 13, 122, 13, 1);

    ssd1306_draw_string(&ssd, "BIBLIOTECA", 24, 5);
    ssd1306_draw_string(&ssd, "Contador: ", 12, 24);
    ssd1306_draw_string(&ssd, buffer, 93, 24);
    ssd1306_send_data(&ssd);

    // Criação das tarefas
    xTaskCreate(vTaskEntrada, "EntradaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "SaidaTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskPerifericos, "PerifericosTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskLeituraBotoes, "LeituraBotoes", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    // Inicia o escalonador do FreeRTOS
    vTaskStartScheduler();
    panic_unsupported();
}