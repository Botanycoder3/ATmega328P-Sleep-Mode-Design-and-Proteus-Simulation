

Atmega328p vs stm32 lowpower report · MD
# Low-Power Operation: ATmega328P vs. STM32F103 — A Technical Comparison
 
---
 
## 1. Selected STM32 Device
 
**Device:** STM32F103C8T6 ("Blue Pill" family), part of the **STM32F10xxx (STM32F103xx) "Performance line"**, built around an **ARM Cortex-M3** core.
 
This device is chosen because it is the most common STM32 used in academic and hobbyist coursework as a direct architectural counterpart to the ATmega328P (Arduino Uno/Nano): both are low-cost, widely available, 3.3 V/5 V-class microcontrollers, but they represent different architecture an 8-bit AVR RISC core with a simple sleep-mode state machine vs. a 32-bit ARM Cortex-M3 core with a dedicated Power Control (PWR) peripheral and NVIC-integrated wake-up logic [1].
 
> Note: newer STM32 "ultra-low-power" families (STM32L0/L1/L4) add extra modes such as *Low-power Run*, *Low-power Sleep*, and *Shutdown*, which the classic STM32F1 line does not have. Where relevant this is flagged, but the register-level discussion below follows the STM32F103 PWR block as documented in **RM0008** [1].
 
---
 
## 2. Datasheet / Reference Manual Sources
 
| # | Source | Publisher | Use in this report |
|---|--------|-----------|---------------------|
| [1] | *RM0008 — Reference Manual: STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx advanced ARM‑based 32‑bit MCUs*, Rev. 21, Feb. 2021 | STMicroelectronics | PWR block, PWR_CR/PWR_CSR bits, Sleep/Stop/Standby behaviour |
| [2] | *ATmega48A/PA/88A/PA/168A/PA/328/P — Data Sheet Complete*, DS40002061A, 2018 | Microchip Technology (formerly Atmel) | SMCR, MCUCR, PRR, sleep-mode table, wake-up sources |
| [3] | J. Yiu, *The Definitive Guide to ARM Cortex‑M3 and Cortex‑M4 Processors*, 3rd ed., Newnes/Elsevier, 2014 — Ch. 9, "Low Power Features" | Elsevier | SCB->SCR (System Control Register), WFI/WFE semantics, Sleep‑on‑Exit, SEVONPEND |
| [4] | ARM Cortex‑M3 low-power architecture overview, *"The definitive guide to ARM Cortex‑M0/M0+: Low‑power states,"* Embedded.com | Embedded.com / ARM | WFI vs. WFE wake-up conditions, DSB usage |
| [5] | *AVR® Low Power Sleep Modes*, Microchip Developer Help portal | Microchip Technology | SMCR.SE / SMCR.SM[2:0] operational detail |
| [6] | *STM32 Low Power Modes: Sleep, Stop & Standby with HAL*, ControllersTech (practical HAL implementation reference, measured on STM32F103 Blue Pill) | ControllersTech | Practical application / HAL-level wake-up examples |
 
Full URLs are listed in the **References** section at the end of this report.
 
---
 
## 3. Low-Power-Mode Comparison (STM32F103)
 
RM0008 defines three low-power modes on top of normal **Run** mode [1]:
 
| Mode | How it's entered | What stays active | Wake-up latency |
|---|---|---|---|
| **Sleep** | `WFI`/`WFE` with `SCB->SCR.SLEEPDEEP = 0` | CPU clock stopped; all peripherals and clocks keep running | Immediate (a few cycles) |
| **Stop** | `WFI`/`WFE` with `SLEEPDEEP = 1` and `PWR_CR.PDDS = 0` | 1.8 V domain clocks all stopped; voltage regulator can be left on or put in low-power mode (`PWR_CR.LPDS`); SRAM and register contents are **retained** [1] | Fast (µs range — limited by regulator wake-up) |
| **Standby** | `WFI`/`WFE` with `SLEEPDEEP = 1` and `PWR_CR.PDDS = 1` | 1.8 V domain is **powered off** entirely; only the RTC, IWDG, and a few backup registers survive on VBAT; SRAM/register content is **lost** [1] | Slow — equivalent to a full reset; program restarts from the reset vector [1] |
 
The device exits Standby mode only via an external **NRST**, an **IWDG reset**, a rising edge on the **WKUP** pin, or an **RTC alarm** [1].
 
For comparison, the ATmega328P defines **six** sleep modes (Idle, ADC Noise Reduction, Power-down, Power-save, Standby, Extended Standby), selected by the `SM2:0` bits of `SMCR` [2] [5] — see Section 10 for the full mapping.
 
---
 
## 4. Power-Control Architecture
 
**STM32F103 / Cortex-M3:**
Power management is split across two cooperating layers [1][3]:
- **Core-level (ARM Cortex-M3):** the `SCB->SCR` (System Control Register) selects *Sleep* vs *Deep Sleep* at the processor level and is entered with the `WFI`/`WFE` instructions.
- **Chip-level (ST's PWR peripheral):** a dedicated **Power Control (PWR)** block sits between the core's "deep sleep" request and the actual voltage regulator/clock tree. It decides — via `PWR_CR` — whether "deep sleep" actually means Stop (regulator on/low-power, SRAM retained) or Standby (1.8 V domain fully powered down) [1].
This two-tier architecture (core sleep request + peripheral-level power domain control) is characteristic of ARM Cortex-M SoCs in general, since ARM defines only the core behaviour and leaves system-level power-domain gating to the silicon vendor [3][4].
 
**ATmega328P / AVR:**
Power control is a **single-tier, flat state machine** built into the AVR clock-control unit [2]. The `SLEEP` instruction, combined with `SMCR.SM[2:0]`, directly gates the clock signals (CPU clock, I/O clock, Flash clock, ADC clock, Asynchronous Timer2 clock) to different peripheral clock domains, and the separate **Power Reduction Register (PRR)** additionally lets the programmer disable individual peripheral clocks (ADC, USART, SPI, TWI, Timers) independent of sleep mode [2]. There is no separate "power domain controller" analogous to STM32's PWR block — voltage/regulator behaviour is fixed in hardware, not software-selectable.
 
---
 
## 5. Important Registers
 
### STM32F103 (PWR peripheral + Cortex-M3 core)
| Register | Location | Purpose |
|---|---|---|
| `PWR_CR` | PWR peripheral (APB1), offset 0x00 | Selects Stop vs Standby, regulator low-power mode, PVD config, flag clearing [1] |
| `PWR_CSR` | PWR peripheral, offset 0x04 | Status flags (wake-up/standby) and WKUP pin enable [1] |
| `SCB->SCR` | Cortex-M3 core (System Control Block), 0xE000ED10 | Selects Sleep vs Deep Sleep, Sleep-on-Exit, SEVONPEND [3] |
| `RCC_APB1ENR` etc. | RCC peripheral | Gates peripheral clocks (analogous role to AVR's PRR) |
| `EXTI_IMR` / `EXTI_EMR` / `EXTI_RTSR/FTSR` | EXTI controller | Enable external/wake-up interrupt lines used to exit Sleep/Stop |
 
### ATmega328P (AVR core)
| Register | Purpose |
|---|---|
| `SMCR` (Sleep Mode Control Register) | `SE` bit arms sleep; `SM2:0` select which of the 6 modes the next `SLEEP` instruction enters [2] |
| `MCUCR` | Contains `BODS`/`BODSE` bits for the timed sequence that disables the Brown-Out Detector during sleep (picoPower feature) [2] |
| `PRR` (Power Reduction Register) | Independently disables clocks to ADC, USART0, SPI, TWI, Timer0/1/2 [2] |
| `EIMSK` / `EICRA` | External Interrupt Mask/Control — configures INT0/INT1 edge or level sensitivity for wake-up [2] |
| `PCICR` / `PCMSK0-2` | Pin Change Interrupt Control/Mask — enables wake-up on any of 24 I/O pins |
 
---
 
## 6. Important Register Bits
 
### STM32 — `PWR_CR` [1]
| Bit | Name | Function |
|---|---|---|
| 0 | `LPDS` | Low-Power Deepsleep — puts the voltage regulator in low-power mode during Stop |
| 1 | `PDDS` | Power-Down Deepsleep — `0` = enter Stop, `1` = enter Standby on Deep Sleep entry |
| 2 | `CWUF` | Clear Wakeup Flag (software-write, clears `PWR_CSR.WUF`) |
| 3 | `CSBF` | Clear Standby Flag (clears `PWR_CSR.SBF`) |
| 4 | `PVDE` | Power Voltage Detector Enable |
| 5–7 | `PLS[2:0]` | PVD voltage threshold selection |
| 8 | `DBP` | Disable Backup domain write protection |
 
### STM32 — `PWR_CSR` [1]
| Bit | Name | Function |
|---|---|---|
| 0 | `WUF` | Wakeup Flag — set when a wake-up event occurred |
| 1 | `SBF` | Standby Flag — set when the device has been in Standby |
| 2 | `PVDO` | PVD Output (voltage-level status) |
| 8 | `EWUP` | Enable WKUP pin as a wake-up source |
 
### STM32 — Cortex-M3 `SCB->SCR` [3][4]
| Bit | Name | Function |
|---|---|---|
| 1 | `SLEEPONEXIT` | Automatically re-enters sleep on return from an ISR (idle-loop-free operation) |
| 2 | `SLEEPDEEP` | `0` = Sleep mode, `1` = Deep Sleep (mapped by PWR to Stop/Standby) |
| 4 | `SEVONPEND` | Any new pending interrupt generates a wake-up *event* for `WFE`, even if disabled |
 
### ATmega328P — `SMCR` [2][5]
| Bit(s) | Name | Function |
|---|---|---|
| 0 | `SE` | Sleep Enable — must be `1` immediately before executing `SLEEP` |
| 3:1 | `SM[2:0]` | Sleep mode select: `000`=Idle, `001`=ADC Noise Reduction, `010`=Power-down, `011`=Power-save, `110`=Standby, `111`=Extended Standby [2] |
 
---
 
## 7. WFI / WFE (Cortex-M3-specific — no AVR equivalent)
 
The STM32/Cortex-M3 core enters sleep with one of **two dedicated ARM instructions** [3][4]:
 
- **`WFI` (Wait For Interrupt):** halts the core until any enabled, pending interrupt (or a debug request) occurs. This is the instruction normally used for simple Sleep/Stop entry.
- **`WFE` (Wait For Event):** halts the core until an *event* occurs — which includes interrupts, a previously latched event in the internal single-bit event register, or (if `SEVONPEND` is set) any newly pended interrupt regardless of whether it is individually enabled in the NVIC [3][4]. `WFE` is typically paired with the `SEV` instruction for lightweight inter-core/inter-task signalling and low-power polling loops.
Both instructions place the processor into **Sleep** or **Deep Sleep** depending purely on the `SLEEPDEEP` bit in `SCB->SCR` — the instruction itself does not choose the depth, the register does [3]. ARM also recommends a `DSB` (Data Synchronisation Barrier) instruction immediately before `WFI`/`WFE` for portability, to ensure outstanding memory transactions complete before the clock is halted [4].
 
**The ATmega328P has no equivalent instruction pair.** The AVR ISA uses a single `SLEEP` instruction; *which* of the six sleep modes it enters is determined entirely by the `SM[2:0]` field pre-loaded into `SMCR`, not by two different opcodes [2]. This is a direct architectural consequence of AVR being a flat, non-hierarchical 8-bit core with no separate "core sleep controller" distinct from the system-level clock gating logic — unlike Cortex-M3, which formally separates *core* sleep-entry instructions (`WFI`/`WFE`) from *system* power-domain configuration (the vendor's PWR block).
 
---
 
## 8. Interrupt and Wake-Up Mechanism
 
### STM32F103
Wake-up is coordinated by the **NVIC** together with the **EXTI** (External Interrupt/Event) controller [1][6]:
- In **Sleep** mode, *any* enabled NVIC interrupt wakes the core (peripherals are still clocked, so timers, USART, ADC, etc. can all generate the wake-up interrupt).
- In **Stop** mode, only sources routed through **EXTI lines** can wake the device, since most peripheral clocks are stopped — typically EXTI pins, the RTC alarm, the USART in some families, or the PVD.
- In **Standby** mode, wake-up sources are reduced to the dedicated **WKUP pin** (`PWR_CSR.EWUP=1`), an **RTC alarm**, an **NRST**, or an **IWDG reset** [1]. Because the 1.8 V domain was powered off, the core actually performs a full reset-and-reboot rather than resuming code [1].
- The `PWR_CSR.WUF`/`SBF` flags let firmware determine, after reset, *why* the device woke up (software must clear them via `PWR_CR.CWUF`/`CSBF` before re-entering low power, or the mode will not be re-entered) [1].
### ATmega328P
Wake-up is interrupt-driven directly against the sleep-mode "active clock domain" table in the datasheet [2]:
- **Idle mode** — any enabled interrupt (timer overflow/compare, USART, SPI, TWI, external, pin-change, etc.) wakes the device, since only the CPU clock is stopped.
- **Power-down mode** — the most restrictive: only an **asynchronous** source can wake the part — external interrupts **INT0/INT1** (configurable as level or, on some pins, edge-triggered via `EICRA`), **pin-change interrupts (PCINT0-23)**, the **TWI address-match interrupt**, or the **Watchdog Timer interrupt/reset** [2][5].
- **Power-save mode** — same as Power-down, plus **Timer/Counter2** (if clocked asynchronously from an external 32.768 kHz crystal) can generate a wake-up (used for RTC-style timekeeping while asleep) [2].
- **Standby / Extended Standby** — identical wake-up sources to Power-down/Power-save respectively, but the crystal oscillator is kept running so the MCU resumes in only ~6 clock cycles instead of paying the full oscillator start-up delay [2].
A key architectural difference: STM32's EXTI lines and NVIC are decoupled from the sleep-mode selection (any of 16+ GPIO-mapped EXTI lines, plus internal lines like RTC/USB/Ethernet, can be configured as wake sources independent of which low-power mode is active), whereas the ATmega328P's available wake-up sources shrink in lockstep with how deep the sleep mode is, because peripheral clocks are physically gated off [2].
 
---
 
## 9. Practical Applications
 
| Scenario | Best-fit device | Why |
|---|---|---|
| Simple battery-powered sensor node waking every few seconds via Watchdog/Timer2 RTC | **ATmega328P** | Power-down current in the tens-of-nA to low-µA range with a simple, well-documented `SLEEP`+watchdog pattern; minimal toolchain overhead [2][5] |
| Wearable / IoT node needing RTC-timestamped logging while asleep, then a fast burst of ADC/DMA/USART processing on wake | **STM32F103 (Stop mode)** | SRAM and register state are retained in Stop mode, avoiding a full re-initialisation on every wake-up cycle, while still cutting current drastically versus Run [1][6] |
| Ultra-long shelf-life battery devices (e.g., asset trackers) that wake rarely and can tolerate a full reboot | **STM32F103 (Standby mode)** or **ATmega328P (Power-down)** | Both reach their lowest current draw by shutting down almost everything; STM32 Standby restarts via reset vector, ATmega Power-down resumes the next instruction after `SLEEP` [1][2] |
| Motor control / USB / multi-peripheral embedded systems needing fast wake latency with retained state | **STM32F103 (Sleep mode)** | Peripherals keep running and the CPU wakes on essentially any interrupt with minimal latency [1] |
| Low-cost educational or hobbyist projects (Arduino ecosystem) | **ATmega328P** | Simpler register model, single sleep instruction, large existing library base (`avr/sleep.h`) [2] |
 
---
 
## 10. Comparison Between ATmega328P and STM32 Low-Power Operation
 
At a systems level, the two devices differ in three fundamental ways:
 
1. **Granularity of modes.** ATmega328P exposes six discrete, peripheral-clock-gated sleep modes selected purely by `SMCR.SM[2:0]` [2]. STM32F103 exposes three coarse modes (Sleep/Stop/Standby), but each is further tunable (regulator on/low-power in Stop; WKUP/RTC selection in Standby) via the PWR block [1] — fewer named modes, but more configurable ones.
2. **Layered vs. flat architecture.** STM32/Cortex-M3 formally separates *core* sleep entry (`WFI`/`WFE` + `SCB->SCR`) from *system* power-domain control (ST's `PWR_CR`/`PWR_CSR`) [1][3]. The ATmega328P has one flat register (`SMCR`) directly driving the AVR clock-control unit, with `PRR` as an optional secondary layer for individual peripheral clocks [2] — there is no ARM-style "core vs. system" split because AVR is not built on a licensed core architecture with that separation.
3. **State retention on deepest sleep.** STM32's deepest mode (Standby) powers off the 1.8 V domain entirely, losing SRAM and requiring a reset-vector reboot [1]. ATmega328P's deepest modes (Power-down/Extended Standby) never remove power from the SRAM/register file at all — AVR sleep modes only gate *clocks*, not internal supply rails — so it always resumes execution at the instruction after `SLEEP` [2]. This means the ATmega328P's "deepest" mode is architecturally closer to STM32's *Stop* mode (clock-gated, state retained) than to STM32's *Standby* mode (power-gated, state lost).
---
 
## 11. Final Comparison Table
 
| Feature | ATmega328P | STM32F103 |
|---|---|---|
| **Sleep modes** | 6: Idle, ADC Noise Reduction, Power-down, Power-save, Standby, Extended Standby [2] | 3: Sleep, Stop, Standby [1] |
| **Deepest low-power mode** | Power-down (all clocks off except async wake sources; SRAM/registers retained) [2] | Standby (1.8 V core domain powered off; SRAM/registers **not** retained) [1] |
| **Sleep-control registers** | `SMCR` (mode select + enable), `MCUCR` (BOD disable sequence), `PRR` (per-peripheral clock gating) [2] | `PWR_CR` / `PWR_CSR` (system-level power domain) plus Cortex‑M3 `SCB->SCR` (core-level Sleep/Deep Sleep) [1][3] |
| **Wake-up sources** | External interrupts (INT0/INT1), pin-change interrupts (PCINT0-23), TWI address match, Watchdog Timer, Timer2 (async), any enabled interrupt in Idle [2] | Any NVIC interrupt (Sleep); EXTI lines/RTC/PVD (Stop); WKUP pin, RTC alarm, NRST, IWDG (Standby) [1] |
| **External interrupt wake-up** | `INT0`/`INT1` (edge or level, via `EICRA`) and 24 pin-change interrupts (`PCINT0-23`), available even in Power-down [2] | Up to 16 GPIO-mapped EXTI lines (edge-configurable via `EXTI_RTSR/FTSR`), available in Sleep and Stop; only the dedicated WKUP pin works in Standby [1] |
| **Timer operation during sleep** | Only Timer2 can run (asynchronously, from an external 32.768 kHz crystal) in Power-save/Extended Standby; all other timers stop below Idle mode [2] | In Sleep, all timers keep running (clocks not gated). In Stop, general-purpose timers stop unless independently clocked; only the RTC and IWDG keep running [1] |
| **RTC operation** | No dedicated RTC peripheral — timekeeping is emulated using Timer2 with an external 32.768 kHz crystal in Power-save mode [2] | Dedicated RTC peripheral clocked from LSE/LSI, continues running in Stop **and** Standby, and can itself generate the wake-up event (RTC alarm) [1] |
| **RAM retention** | Always retained in every sleep mode — AVR sleep modes gate clocks only, never SRAM supply [2] | Retained in Sleep and Stop; **lost** in Standby (1.8 V domain fully powered down, program restarts via reset vector) [1] |
| **Typical applications** | Low-cost Arduino-class projects, simple battery sensor nodes, education/hobbyist designs, watchdog-timed periodic wake-ups [2] | Battery IoT/wearable nodes needing fast wake with state retention (Stop), long-life asset trackers (Standby+RTC), or peripheral-rich real-time systems (Sleep) [1][6] |
 
---
 
## References
 
[1] STMicroelectronics, *RM0008 — Reference manual: STM32F101xx, STM32F102xx, STM32F103xx, STM32F105xx and STM32F107xx advanced ARM-based 32-bit MCUs*, Rev. 21, Feb. 2021. Available: https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
 
[2] Microchip Technology Inc. (formerly Atmel), *ATmega48A/PA/88A/PA/168A/PA/328/P — Data Sheet Complete*, Doc. DS40002061A, 2018. Available: https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061A.pdf
 
[3] J. Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*, 3rd ed., Ch. 9 "Low Power Features," Newnes/Elsevier, 2014 (System Control Register / SCB->SCR reference).
 
[4] "The definitive guide to ARM Cortex-M0/M0+: Low-power states," Embedded.com. Available: https://www.embedded.com/the-definitive-guide-to-arm-cortex-m0-m0-low-power-states/
 
[5] "AVR® Low Power Sleep Modes," Microchip Developer Help. Available: https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/8-bit-avr/structure/sleep/
 
[6] "STM32 Low Power Modes: Sleep, Stop & Standby with HAL," ControllersTech. Available: https://controllerstech.com/low-power-modes-in-stm32/
 
---
 
*Report compiled from official STMicroelectronics and Microchip primary documentation, cross-checked against ARM Cortex-M3 architecture references, for coursework on embedded low-power design.*
 

