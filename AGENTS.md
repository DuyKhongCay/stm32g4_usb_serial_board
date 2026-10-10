# AGENTS.md — Coding Standards & Guidelines for AI Agents

> **Target Project:** `stm32g4_usb_serial_board`  
> **Role:** High-Speed Dual-Channel USB-to-UART Bridge for Zhongli (ASCII) & Waveshare ST3215 (STS Binary) Servos (1 Mbps)  
> **Audience:** AI Coding Assistants (Antigravity, Cursor, Aider, Claude Code, GitHub Copilot)

---

## 1. Tech Stack Snapshot

- **Target MCU:** STMicroelectronics STM32G431CBU6 (ARM Cortex-M4F @ 170 MHz, 128 KB Flash, 32 KB SRAM + CCM-RAM).
- **Core Board:** WeAct Studio STM32G431CxUx CoreBoard (UFQFPN48 V1.0).
- **Build System:** CMake (v3.22+) + Ninja generator via `CMakePresets.json`.
- **Toolchain:** `arm-none-eabi-gcc` / `arm-none-eabi-g++` (GCC 10+ recommended).
- **Languages:** Hybrid C11 (STM32CubeMX HAL/LL) + Modern C++17 (Application Layer in `App/`).
- **USB Interface:** USB 2.0 Full-Speed Native Composite Device with 2 independent CDC-ACM ports (Interface Association Descriptors - IAD):
  - CDC Port 0 (`/dev/zhongli_serial`): Mapped to USART1 (PA9).
  - CDC Port 1 (`/dev/lekiwi_serial`): Mapped to USART3 (PB10).
- **UART Peripherals:**
  - Channel 0 (Zhongli): USART1 Single-Wire Half-Duplex Open-Drain (PA9) @ 1,000,000 baud.
  - Channel 1 (Waveshare ST3215): USART3 Single-Wire Half-Duplex Open-Drain (PB10) @ 1,000,000 baud.

---

## 2. Essential Commands

### Build & Check
```bash
# Configure & Build Debug target
cmake --preset Debug && cmake --build --preset Debug

# Build Release target (optimized -Os)
cmake --preset Release && cmake --build --preset Release

# Clean build directory
rm -rf build/Debug build/Release

# Check firmware memory footprint
arm-none-eabi-size build/Release/stm32g4_usb_serial_board.elf
```

### Flash & Debug
```bash
# Flash via OpenOCD (ST-Link V2/V3)
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg -c "program build/Release/stm32g4_usb_serial_board.elf verify reset exit"

# Flash via STM32CubeProgrammer CLI (ST-Link)
STM32_Programmer_CLI -c port=SWD -w build/Release/stm32g4_usb_serial_board.elf -v -rst

# Flash via DFU over USB Type-C (Hold BOOT0 while resetting to enter DFU mode)
dfu-util -a 0 -s 0x08000000:leave -D build/Release/stm32g4_usb_serial_board.bin
```

---

## 3. Ironclad Architectural Rules (Do's & Don'ts)

### 3.1. Directory & Code Boundary Isolation
- **DO NOT** add custom business logic, C++ classes, or state machines into `Core/`, `Drivers/`, or `USB_Device/`. These folders are managed by STM32CubeMX and can be regenerated.
- **DO** place 100% of custom application code in `App/Inc/` and `App/Src/`.
- **DO** maintain the minimal C-linkage bridge pattern in `Core/` and `USB_Device/`:
  - `Core/Src/main.c`: Only calls `app_init()` and `app_loop()`.
  - `Core/Src/usart.c`: Routes `HAL_UART_TxCpltCallback()` to `app_on_uart_tx_complete(port_idx)`.
  - `Core/Src/stm32g4xx_it.c`: Handles `UART_FLAG_IDLE` on USART1 and USART3, routing to `app_on_uart_rx_idle(port_idx)`.
  - `USB_Device/App/usb_device.c`: Registers `&USBD_CDC_Dual` composite class.

### 3.2. Embedded C++17 Rules (Strict Zero-Allocation)
- **STRICTLY FORBIDDEN:** Dynamic heap memory allocation (`new`, `delete`, `malloc`, `free`, `realloc`).
- **STRICTLY FORBIDDEN:** Heap-based STL containers (`std::vector`, `std::string`, `std::list`, `std::map`).
- **STRICTLY FORBIDDEN:** C++ Exceptions (`throw`, `catch`, `try`) and RTTI (`typeid`, `dynamic_cast`). Build flags enforce `-fno-exceptions -fno-rtti`.
- **DO** use compile-time fixed-size containers (`std::array`, `SpscRingBuffer<uint8_t, 1024>`).
- **DO** qualify variables shared between ISR callbacks and the main loop with `volatile` or atomic access.
- **DO** use `constexpr` and templates for zero-overhead compile-time calculations.

### 3.3. Hardware & Peripherals Contract
- **PA9 (USART1_TX) & PB10 (USART3_TX):** Must ALWAYS remain configured as `GPIO_MODE_AF_OD` (Alternate Function Open-Drain) with external pull-up resistors ($2.2\text{k}\Omega$). Never change to Push-Pull!
- **Clock Tree Contract:** `SYSCLK` must remain locked at **170 MHz** (derived from HSE 8MHz via PLL) to maintain **$0.00\%$ baudrate error** for both USART1 and USART3 fractional generators at 1,000,000 baud.
- **Crystal-less USB:** USB 48 MHz must remain clocked by internal **HSI48 + CRS** (Clock Recovery System).
- **USB PMA Memory Contract:** PMA buffer allocations (536 bytes) must remain strictly non-overlapping for EP0 (Control), EP1/EP2 (CDC0), and EP3/EP4 (CDC1).
- **Power & Grounding:** Star Grounding (Common Ground) between STM32 GND, Zhongli PSU (7.4V), and ST3215 PSU (12V).
- **LED Pin:** `PC6` User LED (`LED_USER_Pin`, Active HIGH, 1 Hz Heartbeat).

---

## 4. Subsystem Extension Workflow

### Extending Channels or Protocol Modifiers:
1. Reusable channel abstraction is located in `App/Inc/half_duplex_uart_channel.hpp` and `App/Src/half_duplex_uart_channel.cpp`.
2. USB routing and coordination is managed by `App/Inc/dual_usb_bridge.hpp` and `App/Src/dual_usb_bridge.cpp`.
3. Do not block inside `process()`: all data pump operations must be non-blocking with zero-copy or minimal ring-buffer staging.
4. If adding new source files to `App/`, register them immediately in root `CMakeLists.txt` under `target_sources()`.
5. Run `cmake --preset Release && cmake --build --preset Release` and verify zero warnings before committing.
