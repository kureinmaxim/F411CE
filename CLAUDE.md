# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**primGPT_F411** — embedded firmware for STM32F411CEUx (BlackPill board) running FreeRTOS v10.3.1. Ported from STM32F103; key change was USART3 → USART6 for debug output. Documentation files are in Russian.

## Build System

The project uses **STM32CubeIDE** (Eclipse CDT) with auto-generated makefiles. Toolchain: `arm-none-eabi-gcc` (GNU Tools for STM32 13.3.rel1).

### Build Commands

```bash
# Build (from project root)
make -C Debug all

# Clean
make -C Debug clean

# Flash via OpenOCD + ST-Link
openocd -f Run.cfg -c "program Debug/primGPT_F411.elf verify reset exit"
```

Build output: `Debug/primGPT_F411.elf` (also .hex, .bin, .map, .list).

### Key Build Settings

- MCU: Cortex-M4F, FPU fpv4-sp-d16, hard float ABI
- Linker script: `STM32F411CEUX_FLASH.ld` (primary), `STM32F411CEUX_RAM.ld` (alternate)
- Defines: `USE_HAL_DRIVER`, `STM32F411xE`
- Debug: `-O0 -g3`; Release: `-Os -g0`

## Architecture

### System Clock

HSE 25 MHz → PLL → SYSCLK 96 MHz. APB1: 48 MHz, APB2: 96 MHz.

### FreeRTOS Tasks

| Task | Stack | Priority | Role |
|------|-------|----------|------|
| `defaultTask` | 1024 B | Normal | Heap/stack diagnostics every 5 sec |
| `LedTask` | 512 B | Normal | PC13 LED blink (1 sec) |
| `Uart1Task` | 2048 B | AboveNormal | UART command processing |

Heap: 10 KB (`heap_4`), ~41% used. CMSIS-RTOS v2 API. Tick rate: 1000 Hz.

### Peripherals

- **USART1** (38400 8N1): Command protocol. ISR-driven RX → FreeRTOS queue (40 chars) → `Uart1Task`. Frame end detected by 20 ms timeout (`UART_TIMEOUT_MS` in `DataFile.h`).
- **USART6** (115200 8N1): Debug logging via `log_printf()` / `printf_uart3()` (128-byte buffer limit).
- **SPI1** (38.4 kHz): FRAM driver — 32 KB, soft CS on PA4. Standard commands: read (0x03), write (0x02), WREN (0x06).
- **GPIO**: PC13 = LED, PC15 = UART1Task activity, PA4 = FRAM CS.

### Command Protocol

Frame: `[CMD_ID | CMD_DATA | CRC16_LSB | CRC16_MSB]` with CRC-16-MODBUS (poly 0xA001).

| Cmd | Sub | Action |
|-----|-----|--------|
| 0x70 | 0/1 | LED on/off |
| 0x02 | 0 | FRAM write/readback test |

### Source Layout

```
Core/Src/main.c        — Entry point, peripheral init, task creation
Core/Src/uart.c        — UART ISR callbacks, queue TX, logging
Core/Src/fram.c        — SPI FRAM read/write/erase
Core/Src/crc16.c       — CRC-16 calculation and frame validation
Core/Inc/DataFile.h    — Protocol constants (timeouts, buffer sizes, IDs)
Core/Inc/FreeRTOSConfig.h — RTOS tuning parameters
```

HAL drivers in `Drivers/`, FreeRTOS kernel in `Middlewares/Third_Party/FreeRTOS/`.

## Development Guidelines

- **CubeMX regeneration**: Peripheral config is in `primGPT_F411.ioc`. User code must stay within `/* USER CODE BEGIN */` / `/* USER CODE END */` markers or it will be overwritten.
- **ISR safety**: Always use `...FromISR()` FreeRTOS APIs in interrupt context. UART RX callback uses `xQueueSendFromISR` + `portYIELD_FROM_ISR`.
- **ISR priority ceiling**: Interrupts calling FreeRTOS APIs must have priority ≥ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (5). Lower numeric value = do not call RTOS from there.
- **Logging**: Use `log_printf()` for thread-safe debug output on USART6. Keep format strings short (128-byte buffer).
- **Memory budget**: FreeRTOS heap is only 10 KB. Monitor via `defaultTask` diagnostics (HeapB/HeapU/StkB/StkU metrics on USART6). ~85% of 128 KB RAM is free for growth.
- **CRC validation**: All incoming command frames must pass `process_crc()` check before processing. Use `calculate_crc_for_2_bytes()` for short responses.
