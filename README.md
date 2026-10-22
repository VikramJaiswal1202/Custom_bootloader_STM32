# Custom Bare-Metal Bootloader for STM32F446RE

A custom firmware-update bootloader for the STM32F446RE microcontroller, implemented using bare-metal C and ARM assembly. The project explores low-level firmware transfer, Flash memory programming, CRC-32 integrity verification, and transferring execution from a bootloader to an application.

The system uses a Python-based host utility to transmit application firmware over UART. The bootloader receives the firmware in chunks, programs it into Flash memory, verifies the complete image, and launches the application after a successful update.

## Features

- **Bare-metal implementation:** Custom startup assembly, linker scripts, and Makefiles.
- **UART communication:** Firmware transfer between a host computer and the STM32 using USART2 at 115200 baud.
- **Flash memory management:** Separate memory regions for the bootloader and application firmware.
- **Chunk-based transfer:** Firmware is transmitted in smaller chunks, with acknowledgements from the target.
- **CRC-32 verification:** Compares the calculated CRC-32 of the programmed application with the expected checksum provided by the host.
- **Flash verification:** Checks programmed data through readback verification.
- **Application validation:** Checks the application's initial stack pointer and reset-handler address before transferring execution.
- **Application handover:** Configures the vector table and stack pointer before branching to the application's reset handler.
- **Python host utility:** Automates firmware transmission and reports update progress and verification results.

## Hardware and Software

### Hardware

- STM32 Nucleo-F446RE development board
- USB connection for ST-LINK programming and the virtual COM port
- Host computer running Linux

### Software

- ARM GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`)
- Python 3
- PySerial
- STM32CubeProgrammer
- GNU Make

The bootloader's core implementation uses direct register-level programming and custom startup code rather than STM32 HAL or CubeMX-generated peripheral initialization.

## System Architecture

The project consists of three main components:

1. **Bootloader:** Runs from the beginning of Flash memory, handles update commands, receives firmware, programs Flash, and validates the application.
2. **Application firmware:** A separately linked program stored in a dedicated Flash region. The current demonstration application initializes the GPIO and blinks the user LED.
3. **Host utility:** A Python script that sends the application binary over UART and verifies the update response.

```text
                Host Computer
              ┌─────────────────┐
              │ Application BIN │
              │ Python Sender   │
              └────────┬────────┘
                       │
                 UART / USART2
                 115200 baud
                       │
              ┌────────▼────────┐
              │   Bootloader    │
              │                 │
              │ Command Parsing │
              │ Flash Erase     │
              │ Flash Program   │
              │ CRC-32 Check    │
              └────────┬────────┘
                       │
                Successful Update
                       │
              ┌────────▼────────┐
              │   Application   │
              │                 │
              │ Startup Code    │
              │ GPIO Init       │
              │ LED Blink       │
              └─────────────────┘
```

## Flash Memory Layout

The bootloader and application are linked to separate Flash memory regions.

| Region | Start Address | Size |
|---|---|---:|
| Bootloader | `0x08000000` | 32 KiB |
| Application | `0x08008000` | Up to 480 KiB |

The bootloader occupies the first 32 KiB of Flash memory, corresponding to sectors 0 and 1 on the STM32F446RE. The application starts at `0x08008000`, the beginning of sector 2.

The application region extends to the end of the 512 KiB Flash address range at `0x08080000`.

The SRAM stack is initialized using the top of the available SRAM at `0x20020000`.

Keeping the two programs in separate regions allows the bootloader to remain in place while the application firmware is updated.

## Firmware Update Workflow

The firmware update follows these steps:

1. The host compiles the application into a binary image and calculates its CRC-32 checksum.
2. The host opens the STM32's UART serial connection and sends a firmware update command containing the image size and expected CRC-32.
3. The bootloader validates the requested image size and prepares the application Flash region for programming.
4. The host transmits the firmware in chunks over UART.
5. The bootloader receives each chunk, programs it into Flash, verifies the programmed data, and acknowledges the chunk.
6. Once the complete image is received, the bootloader calculates the CRC-32 of the programmed application.
7. The calculated CRC-32 is compared with the expected value supplied by the host.
8. If the checksum matches, the bootloader validates the application's initial stack pointer and reset-handler address.
9. The bootloader transfers execution to the application, which initializes its peripherals and runs.

If the integrity check or application validation fails, the bootloader reports an error instead of intentionally launching the invalid image.

## UART Communication

The bootloader communicates with the host through USART2.

| Parameter | Configuration |
|---|---|
| Peripheral | USART2 |
| Baud rate | 115200 |
| TX pin | PA2 |
| RX pin | PA3 |
| Firmware transfer | Chunk-based |
| Chunk size | 128 bytes |
| Command termination | Newline |

The host utility and bootloader exchange commands and status messages to coordinate the update process.

### Supported Commands

**`GET_INFO`**

Requests bootloader information and confirms that the bootloader is responding.

**`BEGIN_UPDATE <size> <crc32>`**

Starts a firmware update. The command includes the binary image size and expected CRC-32 checksum.

The bootloader validates the size before preparing the application Flash region and beginning the transfer.

**`BOOT`**

Requests a handover to the application if the application passes the bootloader's validation checks.

### Example Update Output

```text
Host command: BEGIN_UPDATE 1048 DA363B90
STM32: PREPARING
STM32: READY

... firmware chunks transmitted and acknowledged ...

STM32: EXPECTED_CRC: DA363B90
STM32: ACTUAL_CRC: DA363B90
STM32: UPDATE_OK
```

Matching expected and actual CRC-32 values indicate that the programmed image passed the implemented integrity check.

## CRC-32 Verification

CRC-32 is used to detect accidental corruption of the firmware image during transfer or programming.

The host calculates the checksum of the application binary before transmission. After receiving and programming the complete image, the bootloader calculates the checksum of the programmed application and compares the result with the host-provided value.

The update is considered successful when the values match.

CRC-32 is an integrity check, not a cryptographic authentication mechanism. It does not establish the identity or trustworthiness of the firmware source.

## Application Handover

A microcontroller does not automatically begin executing a separately linked application simply because its binary has been written into Flash.

The bootloader must explicitly transfer execution to the application.

The handover involves:

1. Reading the application's initial stack pointer from its vector table.
2. Reading the reset-handler address.
3. Checking that the stack pointer and reset-handler address fall within the expected memory regions and satisfy the implemented validation rules.
4. Disabling interrupts and clearing relevant pending interrupt state.
5. Updating the Vector Table Offset Register (VTOR) to the application's vector table at `0x08008000`.
6. Loading the application's initial stack pointer.
7. Branching to the application's reset handler.

The application then executes its startup code, initializes its runtime state, and enters `main()`.

This demonstrates the distinction between programming a firmware image into Flash and actually transferring CPU execution to it.

## Project Structure

```text
Custom_Bootloader/
├── bootloader/
│   ├── src/
│   │   ├── main.c
│   │   ├── boot.c
│   │   └── startup.s
│   ├── linker.ld
│   ├── Makefile
│   ├── bootloader.elf
│   └── bootloader.bin
│
├── application/
│   ├── src/
│   │   ├── main.c
│   │   └── startup.s
│   ├── linker.ld
│   ├── Makefile
│   ├── application.elf
│   └── application.bin
│
├── tools/
│   └── uart_transmit.py
│
└── README.md
```

The `.elf` files contain executable and debugging information, while the `.bin` files contain the binary images used for programming and firmware transfer.

## Building the Project

### 1. Install the prerequisites

Install the ARM GNU Toolchain, GNU Make, Python 3, PySerial, and STM32CubeProgrammer using the appropriate installation method for your operating system.

Install PySerial with:

```bash
python -m pip install pyserial
```

### 2. Build the bootloader

```bash
cd bootloader
make
```

### 3. Build the application

```bash
cd ../application
make
```

Confirm that both builds produce their respective `.elf` and `.bin` files.

### 4. Flash the bootloader

Connect the Nucleo board through ST-LINK and use STM32CubeProgrammer to program the bootloader at `0x08000000`.

Example command:

```bash
STM32_Programmer_CLI \
  -c port=SWD \
  -w bootloader/bootloader.bin 0x08000000 \
  -v -rst
```

Run this command from the repository root, using an accessible STM32CubeProgrammer executable.

**Important:** Flash the bootloader at `0x08000000`, not at the application address. Do not perform a full-chip erase after programming the bootloader if you intend to preserve it while updating the application.

### 5. Transfer the application

Connect the board's virtual COM port and run:

```bash
python tools/uart_transmit.py
```

The script reads `application/application.bin`, calculates its CRC-32, transmits the update request and firmware chunks, and reports the target's verification result.

If the serial device name differs from the one expected by the script, update the configured serial port accordingly.

## Current Limitations and Future Improvements

This project is a learning-oriented bootloader implementation. Potential improvements include:

- Adding recovery behavior for interrupted firmware transfers and power loss.
- Implementing a more robust update state machine and explicit transfer timeouts.
- Adding a persistent application-validity record so the bootloader can make a deliberate boot decision after every reset.
- Supporting version metadata and firmware compatibility checks.
- Adding cryptographic signature verification to authenticate firmware before execution.
- Improving diagnostic messages and error reporting.

CRC-32 alone does not protect against deliberately modified firmware. A production bootloader would require stronger security controls and a carefully designed recovery strategy.

## Learning Outcomes

Working through this project provided practical exposure to:

- Bare-metal C and ARM assembly.
- ARM Cortex-M4 startup and exception handling.
- Linker scripts and memory mapping.
- Flash erase, programming, and readback verification.
- UART communication and host-target protocols.
- CRC-32 integrity verification.
- Vector-table relocation and application handover.
- Building and debugging embedded firmware on Linux.

## Acknowledgements

AI tools were used as a supporting resource for understanding unfamiliar concepts, troubleshooting implementation issues, and exploring possible solutions during development.

The project served as a hands-on learning exercise in understanding how a firmware update mechanism operates at the microcontroller level.
