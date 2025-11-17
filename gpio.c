#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <bcm2835.h>
#include "gpio.h"

#define BTN_ACTIVE_LOW 1
static bool s_initialized = false;

static inline void cfg_output(uint8_t pin) {
    bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
}
static inline void cfg_input_pullup(uint8_t pin) {
    bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(pin, BCM2835_GPIO_PUD_UP);
    bcm2835_delay(1);
}
static inline void write_pin(uint8_t pin, int state) {
    bcm2835_gpio_write(pin, (state ? HIGH : LOW));
}

void gpio_setup(void)
{
    if (s_initialized) return;

    if (!bcm2835_init()) {
        fprintf(stderr, "[GPIO] Erro: bcm2835_init() falhou (execute como root/sudo?).\n");
        s_initialized = false;
        return;
    }

    /* Saídas (LEDs) */
    cfg_output(LEDRED);
    cfg_output(LEDYLY);
    cfg_output(LEDGRN);
    cfg_output(LEDSOL);
    cfg_output(LEDC1);
    cfg_output(LEDC2);

    /* Desliga todos na largada */
    write_pin(LEDRED, 0);
    write_pin(LEDYLY, 0);
    write_pin(LEDGRN, 0);
    write_pin(LEDSOL, 0);
    write_pin(LEDC1, 0);
    write_pin(LEDC2, 0);

    /* Entradas (botões) com pull-up */
    cfg_input_pullup(BOTAOC1);
    cfg_input_pullup(BOTAOC2);
    cfg_input_pullup(BOTAOSOL);

    s_initialized = true;
    fprintf(stdout, "[GPIO] Inicializado (bcm2835) com sucesso.\n");
}

void set_led(int pin, int state)
{
    if (!s_initialized) return;
    write_pin((uint8_t)pin, state ? 1 : 0);
}

int read_button(int pin)
{
    if (!s_initialized) return 0;
    uint8_t lev = bcm2835_gpio_lev((uint8_t)pin);
#if BTN_ACTIVE_LOW
    return (lev == LOW) ? 1 : 0;
#else
    return (lev == HIGH) ? 1 : 0;
#endif
}

void set_network_led(int state)
{
    if (!s_initialized) return;

    /* apaga todos */
    write_pin(LEDGRN, 0);
    write_pin(LEDYLY, 0);
    write_pin(LEDRED, 0);

    /* liga um */
    switch (state) {
        case 0: write_pin(LEDGRN, 1); break;   /* verde  */
        case 1: write_pin(LEDYLY, 1); break;   /* amarelo*/
        case 2: write_pin(LEDRED, 1); break;   /* vermelho*/
        default: break;
    }
}
