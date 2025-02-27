/*

Projeto Final: Desenvolvimento de Um Jogo de Ping Pong como Recurso Tecnológico para Profissionais de Fonoaudiologia

Instituição: Hardware HBR

Estudante: Carlos Daniel da Silva 

*/

/* Bibliotecas para utilização */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include <ctype.h>
#include "math.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "inc/ssd1306.h"
#include <string.h>
#include "kissfft/kiss_fft.h"
#include "ws2818b.pio.h"     

/* Variáveis do microfone */
#define ADC_NUM 2
#define ADC_PIN (26 + ADC_NUM)
#define ADC_VREF 3.3
#define ADC_RANGE (1 << 12)
#define ADC_CONVERT (ADC_VREF / (ADC_RANGE - 1))
#define SAMPLE_TIME 3000
#define SAMPLE_RATE 5000
#define SAMPLES (SAMPLE_RATE * SAMPLE_TIME / 1000)

// Variáveis da FFT 
#define FFT_SIZE 1024
#define FILTER_CUTOFF 500
#define NUM_BLOCKS (SAMPLES / FFT_SIZE)

kiss_fft_cfg fft_cfg;
kiss_fft_cfg ifft_cfg;
kiss_fft_cpx fft_in[FFT_SIZE];
kiss_fft_cpx fft_out[FFT_SIZE];

/* Definição do da matriz de leds*/
#define LED_COUNT 25
#define LED_PIN_A 7 

/* Variáveis do Display OLED */
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

/* Estrutura de Definição da Raquete e bola */
#define RAQUETE_HEIGHT 20  // Comprimento da raquete
#define RAQUETE_WIDTH  2   // Largura da raquete 
#define BALL_RADIUS    1   // Raio da bola 

/* Definição dos pinos para o Joystick */
#define JOYSTICK_VRX  26   // Pino para o eixo x do joystick
#define JOYSTICK_VRY  27   // Pino para o eixo y do joystick
#define JOYSTICK_SW   22   // Pino para o botão do joystick

/* Definição do pino do botão e LED */
const uint LED_PIN = 13;
const uint BUTTON_PIN_B = 6; // Inicio do jogo e contagem regressiva 
#define BUTTON_PIN 5

/* Definição dos pinos para o buzzer */ 
#define BUZZER_PIN    21      // Pino para o botão do joystick 
#define BUZZER_FREQUENCY 2000  // Frequência do pino 

bool game_over_playing = false;
absolute_time_t game_over_start_time;

// Notas musicais para a música de Game Over 
const uint game_over_notes[] = {
    440, 440, 440, 349, 349, 349, 440, 440, 440, 349, 349, 349,
    440, 440, 880, 880, 659, 659, 523, 523, 440, 440, 349, 349,
    440, 440, 440, 349, 349, 349, 440, 440, 440, 349, 349, 349,
    440, 440, 880, 880, 659, 659, 523, 523, 440, 440, 349, 349,
};

// Duração das notas em milissegundos 
const uint note_duration[] = {
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 300, 300, 300, 300, 300, 300, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
    300, 300, 300, 300, 300, 300, 500, 500, 500, 500, 500, 500,
};

/* Funções de Controle do Display OLED */

// Função para inicializar o Jogo
void draw_initializa_game(uint8_t* ssd) {

    char *text[] = {       
        "    Press ",
        "   button A  ", 
    };

    int y = 30; // Variável para a posição vertical 
    
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
} 

// Função para inicializar para falar ao microfone
void draw_initializa_game_two(uint8_t* ssd) {

    char *text[] = {       
        "   Speak to ",
        "  Microphone ", 
    };

    int y = 30; // Variável para a posição vertical 
    
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
} 

// Função Seja bem-vindo
void draw_welcome(uint8_t* ssd) {
    char *text[] = {
        "     Press ",
        "    button B  ", 
    };

    int y = 36; // Variável para a posição vertical 
    
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
} 

// Função Coleta Finalizada
void draw_collection_done(uint8_t* ssd) {
    char *text[] = {
        "  Collection",
        "   finished ",
    };

    int y = 30; // Variável para a posição vertical 
    
    for (uint i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 2, y, text[i]);
        y += 8;
    }
}

// Função para desenhar uma raquete 
void draw_racket(uint8_t* ssd, int x, int y) {
    for (int i = 0; i < RAQUETE_HEIGHT; i++) {
        // Manipulação direta no buffer para desenhar a linha da raquete 
        ssd[(y + i) / 8 * ssd1306_width + x] |= (1 << (y + i) % 8); // Marca o pixel como aceso 
        ssd[(y + i) / 8 * ssd1306_width + x + RAQUETE_WIDTH - 1] |= (1 << (y + i) % 8); // Marca o pixel da raquete  
    }
}
// Função para desenhar a bola no display 
void draw_ball(uint8_t* ssd, int x, int y) {     
    for (int dx = -BALL_RADIUS; dx <= BALL_RADIUS; dx++) {
        for (int dy = -BALL_RADIUS; dy <= BALL_RADIUS; dy++) {
            if (dx * dx + dy * dy <= BALL_RADIUS * BALL_RADIUS) {
                if (x + dx >= 0 && x + dx < ssd1306_width && y + dy >= 0 && y + dy < ssd1306_height) {
                    ssd[(y + dy) / 8 * ssd1306_width + (x + dx)] |= (1 << (y + dy) % 8);
                }
            } 
        } 
    }
}

// Função para escrever o placar no display 
void draw_score(uint8_t *ssd, int score1, int score2) {
    char score_text[16];
    sprintf(score_text, "J1: %d  J2: %d", score1, score2);
    ssd1306_draw_string(ssd, 10, 0, score_text);  
}

/* Função para controle de LED matricial 5 x 5 */

// Definição de pixel GRB
struct pixel_t {
    uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; 

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Inicializa a máquina PIO para controle da matriz de LEDs.
 
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    // Limpa buffer de pixels.
    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}


  // Atribui uma cor RGB a um LED.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}


 // Limpa o buffer de pixels.
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs.
void npWrite() {
    // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

/**
Converte as coordenadas (x, y) na matriz 5x5 para o índice da fila linear.
 * 
 * @param x A coluna (0 a 4).
 * @param y A linha (0 a 4).
 * @return O índice correspondente na fila (0 a 24).
 */
int getIndex(int x, int y) {
    // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
    // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
    if (y % 2 == 0) {
        return y * 5 + x; // Linha par (esquerda para direita).
    } else {
        return y * 5 + (4 - x); // Linha ímpar (direita para esquerda).
    }
}

/**
 * Exibe um número na matriz de LEDs.
 * 
 * @param number O número a ser exibido (1, 2 ou 3).
 * @param r Valor de vermelho (0 a 255).
 * @param g Valor de verde (0 a 255).
 * @param b Valor de azul (0 a 255).
 */
void displayNumber(int number, uint8_t r, uint8_t g, uint8_t b) {
    npClear(); // Limpa a matriz antes de exibir o próximo número.
    
    switch (number) 
    {
    case 3:
        npSetLED(getIndex(1,0), r, g, b);
        npSetLED(getIndex(2,0), r, g, b);
        npSetLED(getIndex(3,0), r, g, b);
        npSetLED(getIndex(1,2), r, g, b);
        npSetLED(getIndex(2,2), r, g, b);
        npSetLED(getIndex(3,2), r, g, b);
        npSetLED(getIndex(1,4), r, g, b);
        npSetLED(getIndex(2,4), r, g, b);
        npSetLED(getIndex(3,4), r, g, b);
        break;
    case 2:
        npSetLED(getIndex(1,0), r, g, b);
        npSetLED(getIndex(2,0), r, g, b);
        npSetLED(getIndex(3,0), r, g, b);
        npSetLED(getIndex(3,1), r, g, b);
        npSetLED(getIndex(1,2), r, g, b);
        npSetLED(getIndex(2,2), r, g, b);
        npSetLED(getIndex(3,2), r, g, b);
        npSetLED(getIndex(1,3), r, g, b);
        npSetLED(getIndex(1,4), r, g, b);
        npSetLED(getIndex(2,4), r, g, b);
        npSetLED(getIndex(3,4), r, g, b);
    break;

    case 1:
        npSetLED(getIndex(2,0), r, g, b);
        npSetLED(getIndex(2,1), r, g, b);
        npSetLED(getIndex(2,2), r, g, b);
        npSetLED(getIndex(2,3), r, g, b);
        npSetLED(getIndex(2,4), r, g, b);
        break;
    }

    npWrite(); // Atualiza a tela com o numero exibido 
}
 // Função de contagem regressiva 

 void countdown() {
    // Contagem regressiva
    displayNumber(3,255,0,0);
    sleep_ms(1000);
    
    // Contagem regressiva 2 - Azul
    displayNumber(2,0,0,255);
    sleep_ms(1000);

    // Contagem regressiva 1 - verde
    displayNumber(1,0,255,0);
    sleep_ms(1000);

    // Apaga os LEDs
    npClear();
    npWrite();
}

/* Funções de controle do PWM para buzzer */

// Função para inicializar o PWM do pino do Buzzer 
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);

    // Obter o slice do PWM correspondente ao pino
    uint slice_num = pwm_gpio_to_slice_num(pin);
  
    // Configuração do PWM com a frequência desejada
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);  // Divisor de clock
    pwm_init(slice_num, &config, true); 

    // Iniciar o PWM no nível baixo 
    pwm_set_gpio_level(pin, 0);
}

// Toca uma nota com a frequência e duração especifica (raquete)
void play_sound_racket(uint pin, uint duration_ms) {
    // Obter o slice do PWM 
    uint slice_num = pwm_gpio_to_slice_num(pin);

    // Configura o duty cycle para 75% 
    pwm_set_gpio_level(pin, 3072);
    // Temporização 
    sleep_ms(duration_ms);
    // Desativa o sinal PWM (duty cycle)
    pwm_set_gpio_level(pin, 0);
    // Pausa entre os beeps
    sleep_ms(100);
}

// Função responsável pelo som da bola batendo na raquete
void sound_racket(uint pin) {
    play_sound_racket(pin, 100);
    sleep_ms(50);
    play_sound_racket(pin, 100);
}

// Toca uma nota com a frequência e duração especificada (música)
void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1; 

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); // 50% de duty cycle 

    sleep_ms(duration_ms);

    pwm_set_gpio_level(pin, 0); // Desliga o som após a duração 
    sleep_ms(50); // Pausa entre as notas
}

// Função principal para tocar a música Game Over 
void play_game_over(uint pin) {
    for (int i = 0; i < sizeof(game_over_notes) / sizeof(game_over_notes[0]); i++) {
        if (game_over_notes[i] == 0) {
            sleep_ms(note_duration[i]);
        } else {
            play_tone(pin, game_over_notes[i], note_duration[i]);
        }
    }
}

// Função para inicializar o botão
void init_button() {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN); // Habilita o resistor de pull-up interno
}

// Função para inicializar o botão de contagem regressiva para o jogo 
void init_button_B() {
    gpio_init(BUTTON_PIN_B);
    gpio_set_dir(BUTTON_PIN_B, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_B); // Habilita o resistor de pull-up interno
}

// Função para aplicar o filtro passa-baixa
void apply_low_pass_filter(kiss_fft_cpx* fft_data, int size, int cutoff) {
    int cutoff_index = (cutoff * size) / (SAMPLE_RATE / 2);
    for (int i = cutoff_index; i < size; i++) {
        fft_data[i].r = 0;
        fft_data[i].i = 0;
    }
}
// Função para calcular a magnitude de um número complexo
float magnitude(kiss_fft_cpx value) {
    return sqrt(value.r * value.r + value.i * value.i);
}

void collect_samples(uint8_t* ssd, struct render_area* frame_area, uint32_t duration_ms) {
    uint32_t start_time = time_us_32();
    uint32_t current_time;
    float samples[SAMPLES] = {0}; // Inicializa o array de amostras
    int sample_index = 0;

    printf("Iniciando coleta de amostras...\n");

    while (1) {
        current_time = time_us_32();
        if ((current_time - start_time) >= (duration_ms * 1000)) {
            break;
        }

        float voltage = (adc_read() * ADC_VREF) / ADC_RANGE;
        samples[sample_index] = voltage;

        // Verifica se o valor é válido
        if (isnan(voltage) || isinf(voltage)) {
            printf("Erro: Valor inválido na amostra %d\n", sample_index);
            samples[sample_index] = 0.0f; 
        }

        sample_index++;
        sleep_ms(10);
    }
    printf("Coleta de amostras finalizada.\n");

    // Aplicar a FFT
    fft_cfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);
    ifft_cfg = kiss_fft_alloc(FFT_SIZE, 1, NULL, NULL);

    if (!fft_cfg || !ifft_cfg) {
        printf("Erro: Falha ao alocar memória para a FFT\n");
        return;
    }

    for (int i = 0; i < FFT_SIZE; i++) {
        fft_in[i].r = samples[i];
        fft_in[i].i = 0;
    }

    kiss_fft(fft_cfg, fft_in, fft_out);

    // Imprimir as magnitudes das frequências
    printf("\nResultados da FFT:\n");
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        float freq = (i * SAMPLE_RATE) / FFT_SIZE;
        float mag = magnitude(fft_out[i]);
        printf("Frequência %.2f Hz: Magnitude %.2f\n", freq, mag);
    }

    // Aplicar o filtro passa-baixa
    apply_low_pass_filter(fft_out, FFT_SIZE, FILTER_CUTOFF);

    // Aplicar a inversa da FFT
    kiss_fft(ifft_cfg, fft_out, fft_in);

    // Imprimir as amostras filtradas no domínio do tempo
    printf("\nAmostras filtradas no domínio do tempo:\n");
    for (int i = 0; i < FFT_SIZE; i++) {
        printf("Amostra %d: %.2f\n", i, fft_in[i].r);
    }

    // Exibir a mensagem de coleta finalizada
    memset(ssd, 0, ssd1306_buffer_length);
    draw_collection_done(ssd);
    render_on_display(ssd, frame_area);

    // 1 segundo para exibição da mensagem bem-vindo
    sleep_ms(1000);

    // Aguarda 1 segundo para exibir a mensagem "Bem-vindo"
    memset(ssd, 0, ssd1306_buffer_length);
    draw_welcome(ssd);
    render_on_display(ssd, frame_area);

    // Liberar memória alocada para a FFT
    free(fft_cfg);
    free(ifft_cfg);
}

// Função para resetar 
void reset_game (uint8_t* ssd, struct render_area* frame_area) {
      
    // Limpa o display 
    memset(ssd, 0, ssd1306_buffer_length);

    // Exibi a mensgame inial 
    draw_initializa_game(ssd);
    render_on_display(ssd,frame_area);

    // Tempo um tempo para exibir a mensagem
    sleep_ms(2000);
}

// Função para iniciar o jogo
void start_game(uint8_t* ssd, struct render_area* frame_area) {

    // Posições iniciais das raquetes
    int raquete1_y = ssd1306_height / 2 - RAQUETE_HEIGHT / 2;
    int raquete2_y = ssd1306_height / 2 - RAQUETE_HEIGHT / 2;
    int raquete1_x = 5;
    int raquete2_x = ssd1306_width - 5 - RAQUETE_WIDTH;

    // Posição inicial da bola 
    int bola_x = ssd1306_width / 2;
    int bola_y = ssd1306_height / 2;
    int bola_dx = 2; // Direção horizontal da bola (1 para a direita, -1 para esquerda)  
    int bola_dy = 2; // Direção vertical da bola (1 para a baixo, -1 para cima) 

    // Placar
    int score1 = 0;
    int score2 = 0;

    // Espera o botão ser pressionado 
    while (gpio_get(BUTTON_PIN_B)) {
          sleep_ms(50);  
    }

    countdown(); // Inicia a contagem regressiva

    while (true) {

        // Limpa o Display OLED 
        memset(ssd, 0, ssd1306_buffer_length);
        
        // Atualiza a posição das raquetes
        draw_racket(ssd, raquete1_x, raquete1_y);
        draw_racket(ssd, raquete2_x, raquete2_y);
        
        // Desenha a bola 
        draw_ball(ssd, bola_x, bola_y);

        // Desenha o placar 
        draw_score(ssd, score1, score2);

        // Renderiza o conteúdo no Display
        render_on_display(ssd, frame_area);

        // Seleciona o canal 0 do ADC (pino GP26) para leitura.
        // Esse canal corresponde ao eixo X do joystick (JOYSTICK_VRX)
        adc_select_input(0);
        uint16_t vrx_value = adc_read(); // Lê o valor do eixo Y, de 0 a 4095.

        // Seleciona o canal 1 do ADC (pino GP27) para leitura.
        // Esse canal corresponde ao eixo Y do joystick (JOYSTICK_VRY)
        adc_select_input(1);
        uint16_t vry_value = adc_read(); // Lê o valor do eixo Y, de 0 a 4095.
    
        // Controla as raquetes com base no joystick
        if (vrx_value < 1000 && raquete1_y > 0) {
            raquete1_y -= 2;
        }
        
        if (vrx_value > 3000 && raquete1_y + RAQUETE_HEIGHT < ssd1306_height) {
            raquete1_y += 2;
        }

        if (vry_value < 1000 && raquete2_y > 0) {
            raquete2_y -= 2;
        }
        
        if (vry_value > 3000 && raquete2_y + RAQUETE_HEIGHT < ssd1306_height) {
            raquete2_y += 2;
        }

        // Movimento da bola 
        bola_x += bola_dx;
        bola_y += bola_dy;

        // Colisão com as paredes superior e inferior
        if (bola_y - BALL_RADIUS <= 0 || bola_y + BALL_RADIUS >= ssd1306_height) {
            bola_dy = -bola_dy;  // Inverte a direção vertical
        }

        // Colisão com as raquetes
        if ((bola_x - BALL_RADIUS <= raquete1_x + RAQUETE_WIDTH && bola_y >= raquete1_y && bola_y <= raquete1_y + RAQUETE_HEIGHT) ||
            (bola_x + BALL_RADIUS >= raquete2_x && bola_y >= raquete2_y && bola_y <= raquete2_y + RAQUETE_HEIGHT)) {
            bola_dx = -bola_dx;  // Inverte a direção horizontal
            // Som de raquet
            sound_racket(BUZZER_PIN);
        }

        // Pontuação do placar  
        if (bola_x - BALL_RADIUS <= 0) {
            score2++; 
            bola_x = ssd1306_width / 2;
            bola_y = ssd1306_height / 2;
            bola_dx = -bola_dx;
            bola_dy = (rand() % 2 == 0 ? 1 : -1);
            sleep_ms(500); // Pausa para atualizar o display 
        }

        if (bola_x + BALL_RADIUS >= ssd1306_width) {
            score1++; 
            bola_x = ssd1306_width / 2;  // Reposicionamento da bola no centro 
            bola_y = ssd1306_height / 2;
            bola_dx = -bola_dx;
            bola_dy = (rand() % 2 == 0 ? 1 : -1); 
            sleep_ms(500); // Pausa para atualizar o display 
        }
        
        if (score1 >= 5 || score2 >= 5) {

           // LED indicativo que o jogador perdeu
           gpio_put(LED_PIN, 1);
            
           // Toca a musica Game Over 
           play_game_over(BUZZER_PIN);

           // Reset do jogo
           reset_game(ssd, frame_area);

            // LED indicativo que o jogador perdeu
            gpio_put(LED_PIN, 0);
          
           // Saida do loop 
           break;

        }

        /** Função de Callback para o botão de interrupção **/
        if (!gpio_get(BUTTON_PIN)) {
            if (!game_over_playing || (absolute_time_diff_us(game_over_start_time, get_absolute_time()) > 200000)) {
                // Reseta o estado do jogo 
                score1 = 0;
                score2 = 0;
            }
        }
    }
}


// Função principal
int main() {

    // Inicializa as funções padrão 
    stdio_init_all();
    
    // Inicialização do i2c do Display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
   
    // Inicialização o pino do LED
    npInit(LED_PIN_A);

    // Inicialização do botão
    init_button_B();
    
    // Todos os LEDs apagados 
    npClear(); 

    // Inicializa o ADC para o joystick
    adc_init();
    adc_gpio_init(JOYSTICK_VRX); // Configura o pino para o eixo X
    adc_gpio_init(JOYSTICK_VRY); // Configura o pino para o eixo y 
    gpio_init(JOYSTICK_SW);      // Configura o botão do joystick
    gpio_set_dir(JOYSTICK_SW, GPIO_IN);
    gpio_pull_up(JOYSTICK_SW);

    // Inicialização do botão e LED 
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    
    // Inicializar o PWM no pino do buzzer 
    pwm_init_buzzer(BUZZER_PIN );

    // Inicialização do display OLED 
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column: 0,
        end_column: ssd1306_width - 1,
        start_page: 0,
        end_page: ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
  
    // Zera o display 
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    draw_initializa_game(ssd);
    render_on_display(ssd, &frame_area);

    printf("Aguardando acionamento do botão A...\n");

    // Tempo para exibir a segunda a mensagem
    sleep_ms(3000);
     
    // Exibir a mensagem de coleta finalizada
    memset(ssd, 0, ssd1306_buffer_length);
    draw_initializa_game_two(ssd);
    render_on_display(ssd, &frame_area);

    // Inicializa o ADC
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_NUM);

    // Inicializa o botão
    init_button();

    while (1) {
        // Verifica se o botão A foi pressionado
        if (!gpio_get(BUTTON_PIN)) {
            sleep_ms(50); // Debounce
    
            if (!gpio_get(BUTTON_PIN)) {
                collect_samples(ssd, &frame_area, SAMPLE_TIME); // Coleta amostras
                sleep_ms(500); // Evita múltiplas leituras rápidas
                // Inicia o jogo após a coleta de amostras 
                start_game(ssd, &frame_area);
            }
        }
    }  
}