#ifndef BUZZER_H
#define BUZZER_H

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define BUZZER_A 10
#define BUZZER_B 21

void buzzer_on(uint gpio, uint freq_hz) // Inicializa um buzzer com frequência em Hz
{
  gpio_set_function(gpio, GPIO_FUNC_PWM);
  uint slice = pwm_gpio_to_slice_num(gpio);

  uint32_t clock = 125000000; // Clock da PWM da Pico
  uint32_t wrap = clock / freq_hz;

  pwm_set_wrap(slice, wrap);
  pwm_set_chan_level(slice, PWM_CHAN_A, wrap / 2); // 50% duty
  pwm_set_enabled(slice, true);
}

void buzzer_off(uint gpio) // Desliga o buzzer (desativa PWM e limpa o pino)
{
  uint slice = pwm_gpio_to_slice_num(gpio);
  pwm_set_enabled(slice, false);
  gpio_set_function(gpio, GPIO_FUNC_SIO);
  gpio_set_dir(gpio, GPIO_OUT);
  gpio_put(gpio, 0);
}

int64_t desligar_buzzer_callback(alarm_id_t id, void *user_data) // Callback para desligar o buzzer após um tempo
{
  uint gpio_buzzer = (uint)(uintptr_t)user_data;
  buzzer_off(gpio_buzzer);
  return 0;
}

void buzzer_alarm()
{
  buzzer_on(BUZZER_A, 4000);
  buzzer_on(BUZZER_B, 4000);
}

void buzzer_alarm_off()
{
  buzzer_off(BUZZER_A);
  buzzer_off(BUZZER_B);
}

#endif