# Low-Power Operation: ATmega328P vs. STM32F103 — Slide Prep Notes

---

## 1. Selected STM32 Device
- **STM32F103C8T6** ("Blue Pill"), STM32F10xxx Performance line, **ARM Cortex-M3** core
- Chosen because it's the standard STM32 counterpart to ATmega328P (Arduino) in coursework
- 8-bit AVR (simple sleep state machine) vs. 32-bit Cortex-M3 (dedicated PWR peripheral + NVIC wake-up)
- Note: newer STM32L0/L1/L4 add extra modes (Low-power Run, Shutdown) — not covered here; this report follows STM32F1's PWR block [1]

---

## 2. Datasheet / Reference Manual Sources
| # | Source | Publisher |
|---|--------|-----------|
| [1] | RM0008 Reference Manual (STM32F101/102/103/105/107), Rev. 21, 2021 | STMicroelectronics |
| [2] | ATmega48A/PA/88A/PA/168A/PA/328/P Datasheet Complete, DS40002061A, 2018 | Microchip (Atmel) |
| [3] | J. Yiu, *Definitive Guide to ARM Cortex-M3/M4*, 3rd ed., Ch. 9 | Elsevier |
| [4] | "Definitive guide to ARM Cortex-M0/M0+: Low-power states" | Embedded.com |
| [5] | "AVR Low Power Sleep Modes" | Microchip Developer Help |
| [6] | "STM32 Low Power Modes: Sleep, Stop & Standby with HAL" | ControllersTech |

(Full URLs in References, end of file)

---

## 3. Low-Power-Mode Comparison (STM32F103)
| Mode | Entry | Retained | Wake latency |
|---|---|---|---|
| **Sleep** | WFI/WFE, SLEEPDEEP=0 | CPU clock off only; peripherals run | Immediate |
| **Stop** | WFI/WFE, SLEEPDEEP=1, PDDS=0 | SRAM + registers retained | Fast (µs) |
| **Standby** | WFI/WFE, SLEEPDEEP=1, PDDS=1 | Only RTC/IWDG/backup regs survive | Slow (full reset) |

- Standby exit only via NRST, IWDG reset, WKUP pin, or RTC alarm [1]
- ATmega328P has **6** sleep modes instead of 3 (see Section 10) [2][5]

---

## 4. Power-Control Architecture
**STM32 — two-tier** [1][3]
- Core level: `SCB->SCR` picks Sleep vs Deep Sleep, entered via WFI/WFE
- Chip level: ST's `PWR_CR` decides if "Deep Sleep" = Stop or Standby

**ATmega328P — single-tier / flat** [2]
- `SLEEP` instruction + `SMCR.SM[2:0]` directly gate clock domains
- `PRR` adds optional per-peripheral clock disable
- No separate power-domain controller — regulator behavior is fixed in hardware

---

## 5. Important Registers
**STM32F103**
- `PWR_CR` — Stop/Standby select, regulator mode, PVD, flag clear [1]
- `PWR_CSR` — status flags, WKUP pin enable [1]
- `SCB->SCR` — Sleep/Deep Sleep, Sleep-on-Exit, SEVONPEND [3]
- `RCC_APB1ENR` etc. — peripheral clock gating
- `EXTI_IMR/EMR/RTSR/FTSR` — external wake-up lines

**ATmega328P**
- `SMCR` — sleep enable + mode select [2]
- `MCUCR` — BODS/BODSE (BOD disable during sleep) [2]
- `PRR` — per-peripheral clock gating [2]
- `EIMSK`/`EICRA` — INT0/INT1 config
- `PCICR`/`PCMSK0-2` — pin-change interrupt wake-up

---

## 6. Important Register Bits
**STM32 `PWR_CR`** [1]: LPDS(0) low-power regulator · PDDS(1) Stop/Standby select · CWUF(2) clear wake flag · CSBF(3) clear standby flag · PVDE(4) voltage detector enable · PLS[7:5] PVD threshold · DBP(8) backup write protect

**STM32 `PWR_CSR`** [1]: WUF(0) wake flag · SBF(1) standby flag · PVDO(2) PVD output · EWUP(8) enable WKUP pin

**STM32 `SCB->SCR`** [3][4]: SLEEPONEXIT(1) · SLEEPDEEP(2) · SEVONPEND(4)

**ATmega328P `SMCR`** [2][5]: SE(0) sleep enable · SM[3:1] mode select — 000 Idle, 001 ADC NR, 010 Power-down, 011 Power-save, 110 Standby, 111 Ext. Standby

---

## 7. WFI / WFE (Cortex-M3 only — no AVR equivalent)
- **WFI** — halt until any enabled interrupt or debug request [3][4]
- **WFE** — halt until an *event* (interrupt, latched event, or any newly-pended interrupt if SEVONPEND=1) [3][4]
- Depth (Sleep vs Deep Sleep) is set by `SLEEPDEEP`, not by which instruction is used
- ARM recommends `DSB` before WFI/WFE for portability [4]
- ATmega328P: single `SLEEP` opcode; mode chosen purely by `SMCR.SM[2:0]` beforehand — no core/system split because AVR has no separate core-level sleep controller [2]

---

## 8. Interrupt and Wake-Up Mechanism
**STM32F103** [1][6]
- Sleep → any NVIC interrupt wakes it
- Stop → only EXTI lines (pins, RTC alarm, PVD) wake it
- Standby → only WKUP pin, RTC alarm, NRST, IWDG
- `PWR_CSR.WUF/SBF` flags show wake cause; must clear via `CWUF/CSBF` before re-entering

**ATmega328P** [2][5]
- Idle → any enabled interrupt
- Power-down → only async sources: INT0/INT1, PCINT0-23, TWI address match, Watchdog
- Power-save → same + Timer2 (async 32.768 kHz crystal)
- Standby/Ext. Standby → same sources as above, oscillator stays running → ~6-cycle wake

**Key difference:** STM32 wake sources are largely mode-independent (EXTI/NVIC); ATmega328P wake sources shrink as sleep gets deeper, since clocks are physically gated [2]

---

## 9. Practical Applications
| Scenario | Best fit | Why |
|---|---|---|
| Battery sensor node, periodic wake | ATmega328P (Power-down) | nA–µA range, simple SLEEP+watchdog pattern [2][5] |
| IoT/wearable, fast burst processing | STM32 (Stop) | SRAM retained, no re-init needed [1][6] |
| Long-life asset tracker | STM32 (Standby) or ATmega (Power-down) | Lowest current draw either way [1][2] |
| Peripheral-heavy real-time system | STM32 (Sleep) | Peripherals stay live, fast wake [1] |
| Educational / hobbyist project | ATmega328P | Simple model, big library base [2] |

---

## 10. ATmega328P vs STM32 — Key Differences
1. **Mode granularity** — ATmega: 6 fixed modes [2]. STM32: 3 modes, each configurable [1]
2. **Architecture** — STM32 splits core sleep (WFI/WFE) from system power domain (PWR) [1][3]; ATmega is one flat register (`SMCR`) [2]
3. **State retention** — STM32 Standby loses SRAM (reset reboot) [1]; ATmega never loses SRAM even in deepest sleep (clock-gated only) [2] → ATmega's deepest mode behaves more like STM32 *Stop* than STM32 *Standby*

---

## 11. Final Comparison Table

| Feature | ATmega328P | STM32F103 |
|---|---|---|
| Sleep modes | 6: Idle, ADC NR, Power-down, Power-save, Standby, Ext. Standby [2] | 3: Sleep, Stop, Standby [1] |
| Deepest low-power mode | Power-down — SRAM/regs retained [2] | Standby — 1.8V domain off, SRAM **lost** [1] |
| Sleep-control registers | SMCR, MCUCR, PRR [2] | PWR_CR, PWR_CSR, SCB->SCR [1][3] |
| Wake-up sources | INT0/INT1, PCINT0-23, TWI, Watchdog, Timer2 (async) [2] | NVIC (Sleep); EXTI/RTC/PVD (Stop); WKUP/RTC/NRST/IWDG (Standby) [1] |
| External interrupt wake-up | INT0/INT1 + 24 PCINTs, works even in Power-down [2] | Up to 16 EXTI lines; only WKUP pin works in Standby [1] |
| Timer operation during sleep | Only Timer2 (async crystal) in Power-save/Ext. Standby [2] | All timers run in Sleep; only RTC/IWDG run in Stop [1] |
| RTC operation | No dedicated RTC — emulated via Timer2 [2] | Dedicated RTC, runs through Stop **and** Standby, can wake device [1] |
| RAM retention | Always retained (clock-gating only) [2] | Retained in Sleep/Stop; **lost** in Standby [1] |
| Typical applications | Arduino projects, simple sensor nodes, education [2] | IoT/wearables (Stop), asset trackers (Standby+RTC), real-time systems (Sleep) [1][6] |

---

## References
[1] STMicroelectronics, RM0008 Reference Manual, Rev. 21, 2021 — https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf

[2] Microchip Technology Inc., ATmega48A/PA/88A/PA/168A/PA/328/P Datasheet Complete, DS40002061A, 2018 — https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061A.pdf

[3] J. Yiu, *The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors*, 3rd ed., Ch. 9, Newnes/Elsevier, 2014

[4] "The definitive guide to ARM Cortex-M0/M0+: Low-power states," Embedded.com — https://www.embedded.com/the-definitive-guide-to-arm-cortex-m0-m0-low-power-states/

[5] "AVR Low Power Sleep Modes," Microchip Developer Help — https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/8-bit-avr/structure/sleep/

[6] "STM32 Low Power Modes: Sleep, Stop & Standby with HAL," ControllersTech — https://controllerstech.com/low-power-modes-in-stm32/
