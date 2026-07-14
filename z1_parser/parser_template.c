/* -----------------------------------------------------------------------------
 * parser_template.c  —  Kostur parsera za Zadatak 1
 *
 * Dopunite označene TODO sekcije. NE mijenjajte javni API (deklaracije funkcija
 * i signature callback-a) — koristi ih evaluator za automatsko testiranje.
 *
 * Kompajlirati sa:  gcc -std=c11 -Wall -Wextra parser_template.c -o parser
 * -------------------------------------------------------------------------- */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
/* Trebamo memchr — u nekim MCU toolchainovima nije <string.h>. */
#include <string.h>

/* ------------------- JAVNI API (ne mijenjati) -------------------------------*/

/* Statusi koje callback prima. */
typedef enum {
    FRAME_OK          = 0,
    FRAME_BAD_CRC     = 1,
    FRAME_BAD_LENGTH  = 2,
    FRAME_BAD_ESCAPE  = 3,
    FRAME_TRUNCATED   = 4,
} frame_status_t;

/* Callback koji se poziva za SVAKI kompletan pokušaj okvira (valjan ili ne).
 *   status  -- FRAME_OK ili razlog odbacivanja
 *   cmd     -- CMD bajt (validan samo ako status == FRAME_OK)
 *   payload -- pointer na DEKODIRAN payload (nakon de-escape); NULL ako !OK
 *   length  -- broj bajtova u payload-u
 *   user    -- neprozirni pointer koji je predan parser_reset()
 */
typedef void (*frame_cb_t)(frame_status_t status,
                           uint8_t cmd,
                           const uint8_t *payload,
                           size_t length,
                           void *user);

/* Inicijalizacija/reset parsera. Poziva se jednom prije prvog byte-a. */
void parser_reset(frame_cb_t cb, void *user);

/* Feed-a jedan bajt. Zovi za svaki bajt s žice, redom kojim su stigli.
 * Parser interno održava stanje. Kad detektira kraj okvira (valjan ili ne),
 * poziva callback. */
void parser_feed(uint8_t b);

/* Pomoćna funkcija — implementirajte je i koristite iz svog parsera. */
uint8_t crc8_maxim(const uint8_t *data, size_t len);

/* ------------------- VAŠA IMPLEMENTACIJA (dopuniti) -------------------------*/

/* TODO: definirajte interno stanje parsera (state machine) ovdje. */

void parser_reset(frame_cb_t cb, void *user) {
    (void)cb; (void)user;
    /* TODO */
}

void parser_feed(uint8_t b) {
    (void)b;
    /* TODO: state machine
     *   1) Sinkronizacija na preamble (0xAA 0x55 ili 0x55 0xAA)
     *   2) ...
     */
}

uint8_t crc8_maxim(const uint8_t *data, size_t len) {
    (void)data; (void)len;
    /* TODO */
    return 0;
}

/* ------------------- MAIN (ne mijenjati bitno) ----------------------------- */

static void print_cb(frame_status_t st, uint8_t cmd, const uint8_t *p,
                     size_t n, void *user) {
    (void)user;
    if (st != FRAME_OK) {
        printf("[BAD ] status=%d\n", st);
        return;
    }
    printf("[ OK ] cmd=0x%02X len=%zu payload=", cmd, n);
    for (size_t i = 0; i < n; i++) printf("%02X ", p[i]);
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s capture.txt\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }
    parser_reset(print_cb, NULL);

    char line[1024];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* Očekujemo redak oblika "t=... MOSI: 0xAA 0x55 ..." ili slično.
         * Ekstraktiraj sve "0xXX" tokene i feedaj ih. */
        char *p = line;
        while ((p = (char *)memchr(p, '0', (line + sizeof line) - p))) {
            if (p[1] == 'x' || p[1] == 'X') {
                unsigned v; if (sscanf(p, "0x%2x", &v) == 1) parser_feed((uint8_t)v);
                p += 4;
            } else {
                p++;
            }
        }
    }
    fclose(f);
    return 0;
}


