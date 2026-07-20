# Decisions

## Task 1 - Serial protocol reverse-engineering

### 1 CRC identification
I used online CRC calculator https://crccalc.com/ that offered different algorithms, 
MAXIM (as crc8_maxim function name suggest) gave wrong check byte, ive tried few other 
variants before BLUETOOTH matched and was proven to return same CRC as in *capture.txt*, 
can't say there was "aha" moment... maybe "finally".

### 2 Escape mask 

0x7D is the documented escape byte, but the XOR mask was not given. Considered
assuming the PPP convention (0x20) directly, since 0x7D is the PPP escape and
the pairing is conventional. Rejected that as guessing. Instead derived the mask
from the length-field mismatch: a frame declaring 5 payload bytes contained 6 raw
bytes, so 0x7D 0x8A had to be one logical byte. Solving mask = 0x8A XOR 0xAA
gives 0x20, and the same mask holds for 0x7D 0x75 -> 0x55 and
0x7D 0x5D -> 0x7D. Three independent confirmations, so the mask is derived
rather than assumed.

0x8A -> 0xAA
0x75 -> 0x55
0x5D -> 0x7D

## Task 2 - Code review

### Volatile flag

The missing `volatile` (bug #1) has a standard textbook explanation: the compiler
caches the flag in a register and the loop never sees the ISR update it, so the
device hangs. I decided to actually check it. Installed the real ARM compiler and
compiled the file.

It does NOT break at plain -O2. The reason: the interrupt handler is visible to
the outside world (it's in the vector table), and the loop calls functions the
compiler can't see inside. So the compiler has to assume one of those calls might
touch the flag, and it reloads it every time to be safe. The caching only kicks in
once I turn on -flto (whole-program optimization), because then the compiler can
see everything at once and proves nothing changes the flag.

## Task 3 - design

### Topology rejection

Considered rejecting both A and B on EMI grounds, which would be the simpler story. 
Rejected that, because the two fail for different reasons and collapsing them loses the distinction. 
DS18B20 conversion time is fixed by the sensor's ADC: 750 ms at 12-bit, 93.75 ms at 9-bit. 
Even with a broadcast Skip ROM convert, the aggregate is ~1.27 Hz or ~7.5 Hz, 
against a 10 Hz per-sensor requirement. No wiring change touches that, so B is No. A is workable with mitigations, 
so A is Conditional. Flattening both to No would suggest the physical limit was not understood.

### Final decision

- A: I2C - Conditional (soft averaging, EMI mitigations)
- B: 1-Wire - No (fixed conversion time, cannot meet 10 Hz)
- C: RS-485 - Yes (local averaging, EMI robustness, Modbus RTU

In my opinion only RS-485 is suitable for this industrial application and flexible for future improvements.

## Task 4 - Debug 

### Split the firmware hypothesis in two (Task 4)

Initially had one hypothesis covering "firmware misbehaves." Split it into a blocking operation (D12a) and 
a missing data-ready flag (D12b) after working out their log signatures, which turn out to be opposites. 
A blocking operation produces a time gap with no burst and roughly 20 missing rows. A stale buffer produces 
no time gap at all, continuous 10 ms traffic with ~20 repeated identical values. Since the customer reports 
both "lost or repeated," keeping them merged would have discarded the one observation that separates them. 
However costs of both are similar, so the split is only for clarity, not to prioritize one over the other. 

### Dropped the timestamp question to the customer (Task 4)

Considered asking the customer whether the log carries a per-line timestamp, since the tests for three of the five hypotheses depend on it. Dropped it: the log is already available, so opening it answers the question in seconds. With only five question slots, spending one on something answerable without the customer wastes it, and asking it signals the log was not read. The dependency still matters, so it is stated as an assumption rather than asked.
