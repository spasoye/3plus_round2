# Topology analysis 

## Requirements

- Environment temperature: -10 to +70 °C
- Environment humidity: up to 90%
- EMI: 3kW motor/s nearby
- 8 sensors, 0.3-2m distance from MCU
- MCU is ESP32-S3 (not relevant)
- 10 Hz sampling rate per sensor

## Protocol comparison

Quick comparison of protocols used for communication between MCU and sensors in all three topologies:

| Feature            | **I2C**                   | **RS-485**                     | **1-Wire**                 |
| :----------------- | :------------------------ | :----------------------------- | :------------------------- |
| **Wires**          | 2 (SDA, SCL)              | 2 (Differential Pair)          | 1 (Data + Power)           |
| **Distance**       | Short (PCB/Enclosure)     | Long (Up to 1200m)             | Short to Medium            |
| **Signaling**      | Synchronous (Clock)       | Asynchronous (Differential)    | Asynchronous (Single Wire) |
| **Noise Immunity** | Low                       | Very High                      | Moderate                   |
| **Speed**          | Up to 3.4 Mbps            | Up to 10 Mbps                  | Low (Typically <100 kbps)  |
| **Primary Use**    | On-board IC communication | Industrial/Building automation | Simple sensors/ID tags     |

### Topology A: Branched I2C bus (TMP117 sensors)

#### Pros

1. Fewer components than topology C, which keeps BOM cost and part count low, no per-node MCU, transceiver, or separate firmware image to maintain across 8 boards.
2. TMP117, with resolution of 16 bits, offers significantly higher precision than the DS18B20, with an accuracy of ±0.1°C.
3. With a maximal conversion time of 17.5 ms which leaves plenty of spare overhead for additional procesing and I2C communication overhead to achieve 10 Hz sampling rate.

#### Cons

1. Only 4 addresses per bus (ADD0 pin - GND, VDD, SDA, SCL), needs 2 buses or a mux for 8 sensors which is not according to requirements for this use case.
2. Single-ended signaling, weak against the 3 kW motor's EMI
3. The I2C specification limits total bus capacitance to 400 pF for Standard Mode, branched topology with 8 branches up to 2 m in length quickly goes over that limit

#### Conclusion

This approach is workable, but not out of the box. Sensor addressing can be solved using a second I2C bus (ESP32-S3 has two hardware I2C controllers) or an I2C multiplexer. I2C wires should be kept short (under ~0.5 m per branch) to limit parasitic capacitance against the 400 pF bus budget. Given the weak noise immunity of single-ended signaling, I2C wiring should be routed away from motor power lines, with twisted pairs for SDA/GND and SCL/GND. 


### Topology B: 1-Wire (DS18B20 sensors)

#### Pros 

1. Simplest wiring, possible parasite power mode in which derives its operating power from the 1-Wire data bus (DQ pin) instead of a dedicated external VDD supply, this way wirinig can be done with only 2 wires (Dq and GND).
2. Unique device identification and scalability, no adressing problems like in I2C approach.
3. Lowest cost of all three

#### Cons

1. DS18B20 has a lower resolution of 12 bits and an accuracy of ±0.5°C, which is not as good as TMP117.
2. Due to it's relying on precise timing for communication, 1-Wire is more susceptible to EMI than I2C or RS-485, especially in a noisy environment with a 3 kW motor nearby.
3. Parallel wiring of 8 sensors causes significat voltage drop in parasite power mode so VDD supply is recommended, which makes main benfit of 1-Wire (2 wires) disappear, and adds complexity to the wiring.

#### Conclusion

I would not recommend this approach for this use case. The DS18B20's lower accuracy and susceptibility to EMI make it less suitable for the requirements, especially in an industrial environment with a nearby 3 kW motor. 

#### Topology C: RS-485 half-duplex bus with "smart" sensor nodes

#### Pros

1. RS-485 is designed for use cases like this, it is robust against EMI, supports long distances, and can handle multiple devices on the same bus.
2. Each sensor node can have its own brain, which allows for more complex processing.
3. Can achieve 10 Hz sampling rate per sensor with ease, as each node can handle its own timing and communication.

#### Cons

1. More complex and expensive than the other two topologies, as each sensor node requires its own MCU, RS-485 transceiver, and firmware.
2. No EMI drawbacks
3. No cable length drawbacks

#### Conclusion 

I would recommend this approach. The robustness of RS-485 makes it perfect for industrial environments. 🏆

### Firmware skeleton in pseudocode

I decided to go with topology C, this is the proof of concept firmware skeleton. Since
the main driver for this device is ESP32-S3, I will use ESP-IDF as the framework 
for this firmware. FreeRTOS being the default RTOS for ESP-IDF is used for task
management and synchronization.

**Note:** Since I decided to go with topology C this skeleton shows only the MCU side of the firmware, the sensor node firmware is not shown here. 

#### Task/thread


```
struct node_state {
    node_id;
    temperature;
    is_online;
    last_succ_timestamp;
    failure_count;
    //WIP...
}

// Array of 8 sensor node states, accessing this array should be done in a thread-safe manner, using mutexes or semaphores to avoid race conditions.
node_state nodes[8]

modbus_poll_task:
    purpose: Polls all 8 sensor nodes for temperature data via Modbus 
             RTU over RS-485
    priority: MEDIUM(5)
    period: 1000 ms
    note: all nodes are polled in one cycle, response is expected within 50 ms per node if not error_handler is called which increments failure_count for that node and marks it as offline if failure_count exceeds MAX_FAILURES.

wifi_pub_task:
    purpose: Handles Wi-Fi connectivity and publishes data to the cloud (MQTT/http/...)
    priority: LOW(3)
    period: 10000 ms 
    note: pending

watchdog_task:
    purpose: Monitors system health and resets MCU if necessary
    priority: HIGH(7)
    period: 5000 ms
    note: pending
```
 
#### Request response state machine

```
STATES define:
    IDLE
    WAIT_RESPONSE
    CHECK_CRC
    SUCCESS
    ERROR(reason)

STATE: IDLE
    on poll_tick(node_id):
        send_request(node_id)
        goto WAIT_RESPONSE

STATE: WAIT_RESPONSE
    start_timeout(50ms)
    on response_received:
        goto CHECK_CRC
    on timeout:
        goto ERROR(TIMEOUT)

STATE: CHECK_CRC
    if crc_valid(response):
        goto SUCCESS
    else:
        goto ERROR(CRC_FAIL)

STATE: SUCCESS
    store(node_id, parsed_value)
    goto IDLE

STATE: ERROR(reason)
    error_handler(node_id,reason)
    goto IDLE
```

#### Error handling 
```
void error_handler(node_id, reason) {
    nodes[node_id].failure_count += 1;

    if (nodes[node_id].failure_count > MAX_FAILURES) {
        nodes[node_id].is_online = false;
        log("Node %d marked as offline due to repeated failures", node_id);
    }

    // Additional error handling logic can be added here
}
```
#### Handling main loop blocking

All tasks are managed by FreeRTOS, which allows for non-blocking operations. Each task runs independently and can yield control back to the scheduler, ensuring that no single task can block the main loop.
RS-485 is not busy-waiting, it uses interrupts to handle incoming data, allowing the MCU to perform other tasks while waiting for responses from sensor nodes.
Critical sections of code that access shared resources (like the nodes array) are protected using FreeRTOS mutexes or semaphores to prevent race conditions and ensure data integrity.

### Bring-up and verification of communacation

Hardware just arrived from production, this is the first bring-up of the system. The goal is to verify that all 8 sensor nodes are online and responding correctly to Modbus RTU requests from the MCU.

#### 1. Visual inspection of the PCB and wiring to ensure there are no obvious issues

What: Before powering up the system, I visually inspect the PCB and wiring to ensure there are no obvious issues such as solder bridges, cold solder joints, loose connections or damaged components. Using a **multimeter**, I check for continuity on all critical traces and verify that the RS-485 differential pair is correctly terminated with 120-ohm resistors at both ends of the bus.

Success: No issues are found during the visual inspection. All connections appear solid, and the RS-485 bus is properly terminated.

#### 2. Powering up the system and checking for correct voltage levels

What: Power on one (arbitrary) node board and the master separately,
without connecting the RS-485 bus. Measure actual VCC rail voltage and
idle current draw. Bench power supply with current limit set conservatively
(e.g. 2-3x expected draw), multimeter.

With what: bench power supply, multimeter.

WARNING: Don't touch the PCB with bare hands, use ESD protection. Also, be careful not to short any pins or traces with the multimeter probes.

Success: The voltage levels were within the expected range, and the current draw was consistent with the specifications of the MCU and sensor nodes. No magic smoke appeared.

#### 3. Testing physical layer communication with a single sensor node

What: send a simple Modbus RTU request (Read Holding Register) to the
same node and confirm a correct response with valid CRC-16. Do this FIRST
with an off-the-shelf tool (USB-RS485 adapter + PC Modbus test software),
NOT your own unverified ESP32 firmware, so a failure can't be ambiguous
between node-side and master-side bugs. Only after this passes, repeat the
same request using your own ESP32 master firmware against the same node.

With what: USB-RS485 adapter + PC Modbus RTU tool, logic analyzer for
correlation.

Outcome that satisfies moving on: round trip completes, CRC-16 valid,
decoded value sane, with both the known tool and your own firmware.

#### 4. Basic Modbus RTU communication test with just one sensor node

What: assemble the full branched topology, all 8 nodes at actual cable
lengths (0.3-2 m), with 120 ohm termination at both physical ends and
fail-safe biasing. First, with no traffic, measure the bus idle state.
Then, using your own ESP32 firmware, individually address each of the 8
nodes one at a time (not the full polling cycle yet).

With what: multimeter/oscilloscope on A/B for idle state; logic
analyzer on the full bus plus ESP32 debug UART output for the addressing
pass.

Outcome that satisfies moving on: idle voltage stable and well
defined (biasing works); all 8 nodes respond correctly to their own
address with valid CRC, no collisions or stray traffic on a fully
populated bus.

#### 5. Testing communication with all 8 sensor nodes connected to the RS-485 bus
What: run the actual ModbusPollTask design (1000 ms period, all 8
nodes per cycle) continuously for 30-60 minutes in quiet lab conditions,
no EMI source.

With what: logic analyzer (or oscilloscope) to capture several cycles and compare
against calculated timing (~4-5 ms/node, ~35-40 ms full round), plus
logging of consecutive_failures / status per node.

Outcome that satisfies moving on: measured timing matches calculation
within reasonable tolerance, no node goes OFFLINE, error counters stay
near zero. This is the clean-conditions baseline before any stress.

#### 6. Stress testing the communication under noisy conditions
What: run some available noisy motor (drill, vacuum) near the bus 
while the step 5 polling loop keeps running, and watch whether 
errors/retries increase.

With what: same setup as in step 5

Outcome that satisfies moving on: not required to pass, but if errors increase, consider adding additional filtering, shielding, or twisted pair routing to mitigate EMI effects.