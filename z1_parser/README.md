# TASK 1 - Serial protocol reverse-engineering

Hera are the answers to first 4 questions, build procedure and final output.


## 1. Packet structure

```
[Preamble: 2 bytes][Payload length: 1 byte][Payload: N bytes(Command: 1 byte)(Data: N-1 bytes)][CRC-8: 1 byte]
```
## 2. Preamble 

MOSI: 0xAA 0x55 master to slave 
MISO: 0x55 0xAA slave to master

## 3. Special values 

Special values that required escape bytes are 0xAA, 0x55 and 0x7D.

Packets with escape bytes inside:
```
t=0.0409  MISO: 0x55 0xAA 0x05 0x22 0x00 0x7D 0x8A 0x00 0x00 0x2D
t=0.0439  MISO: 0x55 0xAA 0x05 0x22 0x01 0x7D 0x75 0x00 0x00 0x26
t=0.0458  MOSI: 0xAA 0x55 0x02 0x21 0x02 0x7D 0x5D      # escaped CRC value
t=0.0469  MISO: 0x55 0xAA 0x05 0x22 0x02 0x7D 0x75 0x01 0x00 0xB4
t=0.0580  MISO: 0x55 0xAA 0x05 0x22 0x06 0x7D 0x8A 0x00 0x00 0x56
```

0x7D 0x8A -> 0x8A ^ 0x20 = 0xAA
0x7D 0x75 -> 0x75 ^ 0x20 = 0x55
0x7D 0x5D -> 0x5D ^ 0x20 = 0x7D

**Note**: AI intensively used here to figure out XOR value of **0x20**

## 4. CRC-8 variant

### CRC-8/BLUETOOTH

| Check | Poly | Init | RefIn | RefOut | XorOut |
| --- | --- | --- | --- | --- | --- |
|0x26| 0xA7| 0x00| true| true| 0x00 |

## Build, run and output

```
$ make
$ parser
$ ./parser capture.txt 
[ OK ] cmd=0x12 len=2 payload=01 F8 
[ OK ] cmd=0x13 len=2 payload=01 F8 
[ OK ] cmd=0x12 len=2 payload=63 D8 
[ OK ] cmd=0x13 len=2 payload=63 D8 
[ OK ] cmd=0x12 len=2 payload=02 8F 
[ OK ] cmd=0x13 len=2 payload=02 8F 
[ OK ] cmd=0x12 len=2 payload=E0 BA 
[ OK ] cmd=0x13 len=2 payload=E0 BA 
[ OK ] cmd=0x12 len=2 payload=F8 8B 
[ OK ] cmd=0x13 len=2 payload=F8 8B 
[ OK ] cmd=0x12 len=2 payload=A1 DA 
[ OK ] cmd=0x13 len=2 payload=A1 DA 
[ OK ] cmd=0x12 len=2 payload=51 8B 
[ OK ] cmd=0x13 len=2 payload=51 8B 
[ OK ] cmd=0x12 len=2 payload=0F DA 
[ OK ] cmd=0x13 len=2 payload=0F DA 
[ OK ] cmd=0x12 len=2 payload=84 01 
[ OK ] cmd=0x13 len=2 payload=84 01 
[ OK ] cmd=0x12 len=2 payload=D9 BA 
[ OK ] cmd=0x13 len=2 payload=D9 BA 
[ OK ] cmd=0x12 len=2 payload=DD E7 
[ OK ] cmd=0x13 len=2 payload=DD E7 
[ OK ] cmd=0x12 len=2 payload=95 24 
[ OK ] cmd=0x13 len=2 payload=95 24 
[ OK ] cmd=0x12 len=2 payload=C0 08 
[ OK ] cmd=0x13 len=2 payload=C0 08 
[ OK ] cmd=0x12 len=2 payload=92 48 
[ OK ] cmd=0x13 len=2 payload=92 48 
[ OK ] cmd=0x12 len=2 payload=F0 B9 
[ OK ] cmd=0x13 len=2 payload=F0 B9 
[ OK ] cmd=0x21 len=1 payload=00 
[ OK ] cmd=0x22 len=4 payload=00 AA 00 00 
[ OK ] cmd=0x21 len=1 payload=01 
[ OK ] cmd=0x22 len=4 payload=01 55 00 00 
[ OK ] cmd=0x21 len=1 payload=02 
[ OK ] cmd=0x22 len=4 payload=02 55 01 00 
[ OK ] cmd=0x21 len=1 payload=03 
[ OK ] cmd=0x22 len=4 payload=03 52 03 00 
[ OK ] cmd=0x21 len=1 payload=04 
[ OK ] cmd=0x22 len=4 payload=04 FF 00 00 
[ OK ] cmd=0x21 len=1 payload=05 
[ OK ] cmd=0x22 len=4 payload=05 C8 00 00 
[ OK ] cmd=0x21 len=1 payload=06 
[ OK ] cmd=0x22 len=4 payload=06 AA 00 00 
[ OK ] cmd=0x21 len=1 payload=07 
[ OK ] cmd=0x22 len=4 payload=07 54 01 00 
[ OK ] cmd=0x21 len=1 payload=08 
[ OK ] cmd=0x22 len=4 payload=08 00 02 00 
[ OK ] cmd=0x21 len=1 payload=09 
[ OK ] cmd=0x22 len=4 payload=09 D2 04 00 
[ OK ] cmd=0x30 len=2 payload=00 00 
[ OK ] cmd=0x31 len=1 payload=30 
[ OK ] cmd=0x30 len=2 payload=01 01 
[ OK ] cmd=0x31 len=1 payload=30 
[ OK ] cmd=0x30 len=2 payload=02 00 
[ OK ] cmd=0x31 len=1 payload=30 
[ OK ] cmd=0x30 len=2 payload=99 01 
[ OK ] cmd=0x3F len=2 payload=30 02 
[BAD ] status=1
[BAD ] status=1
[ OK ] cmd=0x12 len=2 payload=26 91 
[ OK ] cmd=0x13 len=2 payload=26 91 
[ OK ] cmd=0x12 len=2 payload=53 2F 
[ OK ] cmd=0x13 len=2 payload=53 2F 
[ OK ] cmd=0x12 len=2 payload=72 7F 
[ OK ] cmd=0x13 len=2 payload=72 7F 
[ OK ] cmd=0x12 len=2 payload=37 4B 
[ OK ] cmd=0x13 len=2 payload=37 4B 
[ OK ] cmd=0x12 len=2 payload=A5 84 
[ OK ] cmd=0x13 len=2 payload=A5 84 
```
