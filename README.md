# Arduino Timing Controller with PWM Motor Control, Electromagnets, and 7-Segment Display

This project controls two DC motors via PWM signals, two electromagnets, and a 4-digit 7-segment LED display (TM1637). It performs timed cycles with countdown display, synchronized electromagnet activation, and a scrolling **FINISH** message at the end.

---

## 🔧 Hardware Components

- Arduino Uno  
- Motor driver (e.g. **L298N**, TB6612, or MOSFET-based)  
- DC Motors ×2  
- Electromagnet Modules ×2  
- TM1637 4-digit 7-segment LED display  
- External 12V power supply (recommended for motors and electromagnets)

---

## 📍 Pin Configuration

| Component           | Arduino Pin |
|--------------------|-------------|
| Motor A IN1 (PWM)  | D5          |
| Motor A IN2 (PWM)  | D6          |
| Motor B IN3 (PWM)  | D9          |
| Motor B IN4 (PWM)  | D10         |
| TM1637 CLK         | D2          |
| TM1637 DIO         | D3          |
| LED Indicator      | D13         |
| Electromagnet 1    | D8          |
| Electromagnet 2    | D11         |

> ⚠️ **Note:** IN1–IN4 must be connected to PWM-capable pins on the Arduino Uno (3, 5, 6, 9, 10, 11).  
> ⚡ Always drive motors and electromagnets through a proper driver circuit with **flyback diodes** — never directly from Arduino pins.

---

## ⏱️ Operation Overview

- The program alternates between two phases:  
  - **Motor ON phase** → motors run under PWM control, LED indicator ON  
  - **Interval phase** → motors stopped, electromagnet activation  
- **Electromagnet 1** activates at the start of a motor phase.  
- **Electromagnet 2** activates during the rest interval.  
- Each electromagnet stays ON for **5 seconds**.  
- After **10 cycles**, the display scrolls `"FINISH"` and all outputs are shut down.  

---

## ⏲️ Cycle Timing

- **Interval phase (state = false)**: `t = 5` seconds  
- **Motor ON phase (state = true)**: `t = 40` seconds  
- **One full cycle** = **5s interval** + **40s motor run** = **45 seconds total**  
- The system repeats for **10 cycles** → about **450 seconds (~7.5 minutes)** of operation before stopping.  

You can adjust these timings by modifying the `t` values in the code or by scaling with `speedFactor`.

---

## 🎚️ PWM Motor Control

- Motors are controlled using `analogWrite()` with adjustable power levels:  
  - `motorPower1` and `motorPower2` (default: 255 = full speed).  
- Direction is managed by setting one pin HIGH (PWM) and the other LOW for each motor.  
- Motors are stopped by setting both inputs LOW.

---

## 🧲 Electromagnet Behavior

- Controlled via digital pins (`D8` and `D11`).  
- Each magnet turns ON for a defined duration (`MAGNET_ON_DURATION`, default: 5000 ms).  
- Activation is independent and timed using `millis()` (non-blocking).  
- Automatically turns OFF after the duration expires.

---

## 📺 Display Behavior

- TM1637 4-digit display shows:  
  - **Left 2 digits** → countdown timer (seconds).  
  - **Right 2 digits** → cycle number.  
- When an electromagnet is activated, the display briefly updates with cycle/time values.  
- After 10 cycles, `"FINISH"` scrolls across the display, then clears after 30 seconds.

---

## ⌚ Timing Logic

- Countdown and scheduling use `millis()` (non-blocking).  
- `t` stores the countdown in seconds.  
- `speedFactor` can accelerate or slow down time (e.g., `2.0` = double speed).  
- Motors and electromagnets operate concurrently without blocking delays.

---

## 📄 Code Features

- PWM motor control via `analogWrite()`  
- Non-blocking timing with `millis()`  
- Independent dual electromagnet control with auto-off  
- Countdown timer + cycle display on TM1637  
- Scrolling `"FINISH"` message at completion  
- Modular functions for motors, display, and magnets  

---

## 🚀 Getting Started

1. Connect all components according to the pin configuration.  
2. Upload the provided Arduino sketch.  
3. Power the motors and electromagnets using an external supply.  
4. Observe countdown cycles, motor activity, and electromagnet activation.  

---

## 🛠️ Customization Ideas

- Adjust `motorPower1` and `motorPower2` for speed tuning.  
- Change `MAGNET_ON_DURATION` for longer/shorter magnet activation.  
- Add a buzzer or sound feedback during electromagnet ON/OFF events.  
- Use buttons or sensors to dynamically adjust timing or reset cycles.  
- Replace L298N with MOSFET-based drivers for higher efficiency.  

---

## 📜 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.  
