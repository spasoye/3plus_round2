# Protocol — description

## 1. Frame structure

```
[Preamble: 2B][Length: 1B][CMD: 1B][Payload: N-1 B][CRC-8: 1B]
```

- **Preamble** — 2 bytes, differs by direction:
  - `0xAA 0x55` — MOSI (R1 → R2)
  - `0x55 0xAA` — MISO (R2 → R1)

  The two bytes are bitwise complements (`0xAA ^ 0x55 == 0xFF`), which is used for sync/re-sync.

- **Length** — the number of *decoded* bytes that follow in the CMD+Payload fields (so it includes CMD, but not itself or the CRC). Never seen escaped in the capture.

- **CMD** — the first decoded byte after Length; identifies the message type. Request/response pairs observed in `capture.txt`: `0x12`/`0x13`, `0x21`/`0x22`, `0x30`/`0x31`/`0x3F`.

- **Payload** — the remaining `Length - 1` decoded bytes.

- **CRC-8** — covers `Length + CMD + Payload` (decoded bytes), **does not cover the preamble**. Variant: **CRC-8/BLUETOOTH** (poly `0xA7`, init `0x00`, refin/refout `true`, xorout `0x00`) — see `DECISION.md` for how it was found. The skeleton's function name (`crc8_maxim`) is misleading; MAXIM (`0x31`) does not match the capture.

## 2. Escaping

- Escape marker: `0x7D`.
- Special values that must be escaped when they occur within the Length/CMD/Payload/CRC fields: `0xAA`, `0x55`, and the escape marker `0x7D` itself (it must escape itself so the decoder doesn't mistake a literal `0x7D` for the start of an escape sequence).
- Sequence: `0x7D <original_byte XOR 0x20>`. Decoding is the same operation: `decoded = next_byte XOR 0x20`.
- The preamble is never escaped.

## 3. Frame rejection

The parser invokes the callback for every frame attempt, including rejected ones, with a reason:
- `FRAME_BAD_CRC` — the computed CRC doesn't match the received one.
- `FRAME_BAD_LENGTH` — Length is `0` or exceeds the internal buffer's capacity.
- `FRAME_BAD_ESCAPE` — the byte after an escape marker isn't one of the legal escaped bytes.
- `FRAME_TRUNCATED` — while waiting for the rest of a frame, a new valid preamble is encountered (previous frame left unfinished).

Since `0xAA`/`0x55`/`0x7D` are always escaped when they're real data, an unescaped occurrence of any of them inside a frame is a reliable signal of desync/corruption, so it's also used to trigger re-sync (the same mechanism as the initial preamble sync).
