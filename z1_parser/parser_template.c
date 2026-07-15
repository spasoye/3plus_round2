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
#define MAX_FRAME_DATA  64
#define ESCAPE_MARKER 0x7D
#define ESCAPE_XOR    0x20

/* Parser state machine states */
typedef enum {
    SM_SYNC0,           /* first preamble byte: 0xAA or 0x55 */
    SM_SYNC1,           /* second preamble byte */
    SM_LEN,             /* waiting for length byte (cmd+payload) */
    SM_DATA,            /* consuming/unescaping `len` bytes of cmd+payload */
    SM_DATA_ESC,        /* previous raw byte was the 0x7D escape marker, in DATA */
    SM_CRC,             /* consuming/unescaping the final CRC byte */
    SM_CRC_ESC,         /* previous raw byte was 0x7D, in CRC */
} parser_state_t;

/* parser context state */
typedef struct {
    parser_state_t state;                   /* current parser state */
    uint8_t        sync0;                   /* first preamble byte seen */
    uint8_t        len;                     /* decoded length announced by frame */
    uint8_t        data[MAX_FRAME_DATA];    /* cmd and payload */
    uint8_t        data_idx;                /* index into data[] */
    frame_cb_t     cb;                      /* callback function */
    void          *user;
} parser_ctx_t;

static parser_ctx_t ctx;

/**
 * Aborts the current frame and reports its status.
 * @param status The status to report.
 * @param b The byte that caused the abort (used to determine next state).
 */
static void abort_frame(frame_status_t status, uint8_t b) {
    ctx.cb(status, 0, NULL, 0, ctx.user);
    if (b == 0xAA || b == 0x55) {
        ctx.sync0 = b;
        ctx.state = SM_SYNC1;
    } else {
        ctx.state = SM_SYNC0;
    }
}

/**
 * Checks if a byte is a valid escaped byte.
 * @param b The byte to check.
 * @return true if the byte is valid, false otherwise.
 */
static bool is_valid_escaped_byte(uint8_t b) {
    /* Only these bytes may legally follow an escape marker. */
    return b == (uint8_t)(0xAA ^ ESCAPE_XOR)
        || b == (uint8_t)(0x55 ^ ESCAPE_XOR)
        || b == (uint8_t)(ESCAPE_MARKER ^ ESCAPE_XOR);
}

/**
 * Finishes the current frame and reports its status.
 * @param received_crc The CRC byte received with the frame.
 */
static void finish_frame(uint8_t received_crc) {
    uint8_t expected = crc8_maxim(ctx.data, (size_t)ctx.len + 1);
    if (received_crc == expected) {
        ctx.cb(FRAME_OK, ctx.data[1], &ctx.data[2], (size_t)ctx.len - 1, ctx.user);
    } else {
        ctx.cb(FRAME_BAD_CRC, 0, NULL, 0, ctx.user);
    }
    ctx.state = SM_SYNC0;
}

/**
 * Resets the parser state.
 * @param cb The callback function to use.
 * @param user The user data for the callback.
 */
void parser_reset(frame_cb_t cb, void *user) {
    ctx.state = SM_SYNC0;
    ctx.cb = cb;
    ctx.user = user;
}

void parser_feed(uint8_t b) {
    switch (ctx.state) {

    case SM_SYNC0:
        if (b == 0xAA || b == 0x55) {
            ctx.sync0 = b;
            // Next state
            ctx.state = SM_SYNC1;
        }
        break;

    case SM_SYNC1:
        /* 0xAA ^ 0x55 == 0xFF mindblown... */
        if ((uint8_t)(ctx.sync0 ^ b) == 0xFF) {
            // Next state
            ctx.state = SM_LEN;
        }
        else if (b == 0xAA || b == 0x55) {
            /* restart pairing from this byte */
            ctx.sync0 = b;
        }
        else {
            ctx.state = SM_SYNC0;
        }
        break;

    case SM_LEN:
        ctx.len = b;
        ctx.data_idx = 0;
        if (ctx.len == 0 || ctx.len >= MAX_FRAME_DATA) {
            abort_frame(FRAME_BAD_LENGTH, b);
            break;
        }

        ctx.data[ctx.data_idx] = b;
        ctx.data_idx++;

        ctx.state = SM_DATA;
        break;

    case SM_DATA:
        if (b == ESCAPE_MARKER) {
            ctx.state = SM_DATA_ESC;
        }
        else if(b == 0xAA || b == 0x55){
            abort_frame(FRAME_TRUNCATED, b);
        }
        else {
            ctx.data[ctx.data_idx++] = b;
            if (ctx.data_idx == (size_t)ctx.len + 1) ctx.state = SM_CRC;
        }
        break;

    case SM_DATA_ESC:
        if (!is_valid_escaped_byte(b)) {
            abort_frame(FRAME_BAD_ESCAPE, b);
            break;
        }
        ctx.data[ctx.data_idx++] = (uint8_t)(b ^ ESCAPE_XOR);
        ctx.state = (ctx.data_idx == (size_t)ctx.len + 1) ? SM_CRC : SM_DATA;
        break;

    case SM_CRC:
        if (b == ESCAPE_MARKER) {
            ctx.state = SM_CRC_ESC;
            break;
        }
        if (b == 0xAA || b == 0x55){
            abort_frame(FRAME_TRUNCATED, b);
            break;
        }
        finish_frame(b);
        break;

    case SM_CRC_ESC:
        if (!is_valid_escaped_byte(b)) {
            abort_frame(FRAME_BAD_ESCAPE, b);
            break;
        }
        finish_frame((uint8_t)(b ^ ESCAPE_XOR));
        break;
    }
}

uint8_t crc8_maxim(const uint8_t *data, size_t len) {
    /* NOTE: despite the function name, 
    data provided in capture file tells this should be CRC8/BLUETOOTH variant. */

    uint8_t crc = 0x00; 

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x01) {
                crc = (uint8_t)((crc >> 1) ^ 0xE5); 
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
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
