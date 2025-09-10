# Arduino Timing Controller with Motor, Electromagnets, and 7-Segment Display

This project controls two DC motors (RE-280RA) via an L298 motor driver, two electromagnet modules (KEYESTUDIO), and a 4-digit 7-segment LED display (TM1637). It performs timed cycles with visual feedback and synchronized electromagnet activation.

## 🔧 Hardware Components

- Arduino Uno
- L298 Motor Driver
- RE-280RA DC Motors ×2
- KEYESTUDIO Electromagnet Modules ×2
- TM1637 4-digit 7-segment LED display
- External power supply (recommended for motors and electromagnets)

## ⚙️ Pin Configuration

| Component           | Arduino Pin |
|--------------------|-------------|
| Motor A IN1        | D4          |
| Motor A IN2        | D5          |
| Motor B IN3        | D6          |
| Motor B IN4        | D7          |
| TM1637 CLK         | D2          |
| TM1637 DIO         | D3          |
| LED Indicator      | D13         |
| Electromagnet 1    | D8          |
| Electromagnet 2    | D9          |

## ⏱️ Operation Overview

- The program begins with a 5-second "interval" phase (cycle 0).
- Then it alternates between:
  - **45 seconds of motor operation** (`state = true`)
  - **5 seconds of rest interval** (`state = false`)
- This loop continues for **10 full cycles**.
- After the 10th cycle, the display scrolls the word `FINISH` from right to left.

## 🔌 Electromagnet Behavior

- **Electromagnet 1** activates when the 5-second interval ends.
- **Electromagnet 2** activates when the 45-second motor phase ends.
- Each electromagnet stays ON for **5 seconds**, independently.
- While active, the **colon `:` on the 7-segment display lights up for 1 second**.

## 💡 Display Behavior

- The 7-segment display shows:
  - **Left 2 digits**: countdown seconds
  - **Right 2 digits**: current cycle number
- When an electromagnet is activated, the colon `:` appears for 1 second.
- After 10 cycles, the display scrolls `FINISH` from right to left.

## 🧠 Timing Logic

- All timing is handled using `millis()` for non-blocking countdowns.
- Countdown continues even while electromagnets are active.
- Speed can be adjusted using the `speedFactor` variable (e.g. `2.0` for double speed).

## 📄 Code Features

- Non-blocking countdown using `millis()`
- Independent control of two electromagnets
- Visual feedback via colon and scrolling text
- Modular functions for motor control, display, and magnet timing

## 🚀 Getting Started

1. Connect all components according to the pin configuration.
2. Upload the Arduino sketch.
3. Power the motors and electromagnets using an external supply.
4. Watch the countdown cycles and electromagnet activation.

## 🛠️ Customization Ideas

- Add buzzer or sound feedback during electromagnet activation
- Use buttons or sensors to trigger or reset cycles
- Display additional messages or animations after `FINISH`

## 📜 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.
