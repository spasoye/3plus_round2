/* -----------------------------------------------------------------------------
 * sensor_loop.c  —  "Skoro radi" firmware kandidat za code review
 *
 * Ciljna platforma: MCU s ARM Cortex-M0+ (npr. RP2040 core0), bare-metal, GCC.
 * Toolchain:        arm-none-eabi-gcc, -std=c11, -Wall -Wextra
 * Standard:         ISO/IEC 9899:2011 (C11)
 *
 * Namjena:
 *   - Periodički (svakih 100 ms iz SysTick ISR-a) postavi zastavicu za uzorak
 *   - Glavna petlja čita registar 0x05 s MCP9808 (I2C, adresa 0x18) i sprema
 *     uzorak u kružni buffer
 *   - Svakih 500 ms uzima batch, filtrira, formatira i šalje preko UART-a
 *   - Uz to: nekoliko pomoćnih bit-operacija za kontrolni registar senzora
 *
 * Trenutno kompajlira (-Wall -Wextra), sporadično radi u simulaciji, u produkciji
 * pokazuje "čudno" ponašanje.
 *
 * VAŠ ZADATAK:
 *   Nađite sve smislene bugove. Za svaki dajte:
 *     (a) broj retka
 *     (b) klasu problema (npr. "integer promotion", "race condition")
 *     (c) kratko obrazloženje ZAŠTO je bug — po mogućnosti referencu na
 *         C11 standard ili konkretni izlaz kompajlera / opažanje na ciljnom MCU
 *     (d) minimalan fix (par redaka koda)
 *
 *   NE tražite "što bih ljepše napisao" — samo stvarne bugove i UB (undefined
 *   behavior) po ISO C-u, ili konkretne funkcionalne pogreške.
 *
 *   Broj bugova nije otkriven. Očekuje se između 5 i 15.
 * -------------------------------------------------------------------------- */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/* --- HAL prototipi (implementirano drugdje, nije tema code review-a) ------- */
extern bool     i2c_start(uint8_t addr, bool read);
extern bool     i2c_write_byte(uint8_t b);
extern uint8_t  i2c_read_byte(bool ack);
extern void     i2c_stop(void);
extern void     uart_send(const char *s, uint16_t len);
extern uint32_t millis(void);        /* wrap svakih ~49 dana */

/* --- Konfiguracija --------------------------------------------------------- */
#define MCP9808_ADDR   0x18
#define TEMP_REG       0x05
#define CFG_REG        0x01
#define RB_SIZE        8            /* mora biti potencija dvojke */
#define BATCH_SIZE     5

/* --- Bit maske za CFG registar (16-bit) ------------------------------------ */
#define CFG_SHDN_BIT      8         /* shutdown mode */
#define CFG_ALERT_BIT     3         /* alert output */
#define CFG_HYST_MASK     0x600     /* dva bita, poz. 9-10 */

/* --- Kružni buffer --------------------------------------------------------- */
typedef struct {
    int16_t data[RB_SIZE];
    uint8_t head;
    uint8_t tail;
} ring_buf_t;

static ring_buf_t rb;

static void rb_push(ring_buf_t *r, int16_t v) {
    r->data[r->head & (RB_SIZE - 1)] = v;
    r->head++;
    if (r->head - r->tail > RB_SIZE) {
        r->tail = r->head - RB_SIZE;   /* drop najstariji */
    }
}

static bool rb_pop(ring_buf_t *r, int16_t *out) {
    if (r->head == r->tail) return false;
    *out = r->data[r->tail & (RB_SIZE - 1)];
    r->tail++;
    return true;
}

/* --- ISR / dijeljeno stanje ------------------------------------------------ */
static uint32_t systick_count = 0;
static bool     sample_flag   = false;
static bool     send_flag     = false;

void SysTick_Handler(void) {
    systick_count++;
    if ((systick_count % 100) == 0) {
        sample_flag = true;
    }
    if ((systick_count % 500) == 0) {
        send_flag = true;
        /* Debug: log every 500 ms — helps in bring-up */
        float period_s = (float)systick_count / 1000.0f;
        if (period_s > 3600.0f) {
            /* preko sat vremena, resetiraj brojač */
            systick_count = 0;
        }
    }
}

/* --- Bitne pomoćne funkcije ------------------------------------------------ */

/* Vraća masku s N postavljenih donjih bitova. Npr. bitmask_lower(4) = 0x0F. */
static uint32_t bitmask_lower(uint8_t n) {
    return (1 << n) - 1;
}

/* Postavi bit b u vrijednosti v i vrati novu vrijednost. */
static uint16_t set_bit(uint16_t v, uint8_t b) {
    return v | (1 << b);
}

/* Vrati true ako je bit b postavljen. */
static bool test_bit(uint16_t v, uint8_t b) {
    return (v & (1 << b)) != 0;
}

/* Rotiraj 32-bitnu vrijednost udesno za n mjesta. */
static uint32_t rotr32(uint32_t x, uint8_t n) {
    return (x >> n) | (x << (32 - n));
}

/* Vrati true ako "sve zastavice" u masci nisu postavljene u v. */
static bool no_flags_set(uint8_t v, uint8_t mask) {
    return (~v & mask) == mask;
}

/* --- MCP9808 driver -------------------------------------------------------- */
static int16_t read_temperature_raw(void) {
    int16_t result = 0;
    uint8_t hi, lo;

    /* pisanje adrese registra */
    while (!i2c_start(MCP9808_ADDR, false)) { /* retry */ }
    i2c_write_byte(TEMP_REG);
    i2c_stop();

    /* čitanje 2 bajta */
    i2c_start(MCP9808_ADDR, true);
    hi = i2c_read_byte(true);
    lo = i2c_read_byte(false);
    i2c_stop();

    /* MCP9808: MSB prvi, gornja 3 bita su flagovi, ostalih 13 je temperatura
     * u komplementu 2 s rezolucijom 0.0625 °C. Skaliramo na *10 (int16). */
    result = ((hi & 0x1F) << 8) | lo;
    if (hi & 0x10) {                 /* sign bit */
        result -= 4096;
    }
    /* raw je u 1/16 °C; želimo *10, radimo *10 pa /16 */
    return (int16_t)((result * 10) / 16);
}

/* --- Formatiranje ---------------------------------------------------------- */
static void format_temp(int16_t v, char *buf) {
    /* Formatira temperaturu *10 kao "-XX.X\0" ili "XXX.X\0". */
    char tmp[8];
    int  n = 0;
    bool neg = v < 0;
    if (neg) v = -v;
    uint16_t whole = v / 10;
    uint16_t frac  = v % 10;
    tmp[n++] = '0' + (whole / 100) % 10;
    tmp[n++] = '0' + (whole / 10)  % 10;
    tmp[n++] = '0' + (whole)       % 10;
    tmp[n++] = '.';
    tmp[n++] = '0' + frac;
    tmp[n]   = '\0';
    if (neg) {
        buf[0] = '-';
        strcpy(buf + 1, tmp);
    } else {
        strcpy(buf, tmp);
    }
}

/* --- Rekurzivni "divide & conquer" prosjek --------------------------------- */
static int32_t recursive_avg(int16_t *samples, uint8_t n) {
    int16_t buffer[64];  /* dovoljno za bilo koji smisleni batch */
    (void)buffer;
    if (n == 0) return 0;
    if (n == 1) return samples[0];
    int32_t left  = recursive_avg(samples, n / 2);
    int32_t right = recursive_avg(samples + n / 2, n - n / 2);
    return (left + right) / 2;
}

/* --- Provjera zdravlja CFG registra ---------------------------------------- */
static bool cfg_is_healthy(uint16_t cfg) {
    /* Uređaj je "zdrav" ako shutdown nije postavljen i alert je aktivan. */
    if (test_bit(cfg, CFG_SHDN_BIT)) return false;
    if (!test_bit(cfg, CFG_ALERT_BIT)) return false;
    /* Također, high nibble mora imati barem jedan bit postavljen. */
    uint8_t high_nibble_mask = 0xF0;
    uint8_t low_bits = cfg & 0xFF;
    if (no_flags_set(low_bits, high_nibble_mask)) {
        /* prazno */
    }
    return true;
}

/* --- Glavna petlja --------------------------------------------------------- */
int main(void) {
    int16_t batch[BATCH_SIZE];
    uint8_t collected = 0;
    char    outbuf[8];

    /* SysTick config, UART/I2C init — pretpostavimo da je odrađeno */

    while (1) {
        if (sample_flag) {
            sample_flag = false;
            int16_t t = read_temperature_raw();
            rb_push(&rb, t);
        }

        if (send_flag) {
            send_flag = false;
            collected = 0;
            int16_t v;
            while (collected < BATCH_SIZE && rb_pop(&rb, &v)) {
                batch[collected++] = v;
            }
            if (collected == BATCH_SIZE) {
                int32_t avg = recursive_avg(batch, collected);
                format_temp((int16_t)avg, outbuf);
                uart_send(outbuf, strlen(outbuf));
                uart_send("\r\n", 2);
            }
        }

        /* Watchdog kick svakih ~10 ms */
        static uint32_t last_kick = 0;
        uint32_t now = millis();
        if (now - last_kick > 10) {
            /* watchdog_kick(); */
            last_kick = now;
        }
    }
}
