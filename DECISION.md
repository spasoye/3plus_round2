# Decisions

## Task 1 - Serial protocol reverse-engineering

I used online CRC calculator https://crccalc.com/ that offered different algorithms, MAXIM (as crc8_maxim function name suggest) gave wrong check byte, ive tried few other variants before BLUETOOTH matched and was proven to return same CRC as in *capture.txt*, can't say there was "aha" moment... maybe "finally".


