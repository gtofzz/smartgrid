#ifndef GPIO_H
#define GPIO_H

/* ========== MAPEAMENTO (mantido como você enviou) ========== */
/* CARGAS */
#define LEDC1     19
#define LEDC2     26
#define BOTAOC1   27
#define BOTAOC2   22

/* PAINEL SOLAR */
#define LEDSOL     0
#define BOTAOSOL  17

/* ESTADO DA REDE */
#define LEDRED    13 // 13
#define LEDYLY     6 //6
#define LEDGRN     5 // 5

/* ========== API pública ========== */
void gpio_setup(void);
void set_led(int pin, int state);
int  read_button(int pin);
void set_network_led(int state); // 0=verde, 1=amarelo, 2=vermelho

#endif
