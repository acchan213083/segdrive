# Arduino Timing Controller with PWM Motor Control, Electromagnets, and 7-Segment Display

This project controls two DC motors (RE-280RA) via PWM signals and an L298 motor driver, two electromagnet modules (KEYESTUDIO), and a 4-digit 7-segment LED display (TM1637). It performs timed cycles with visual feedback and synchronized electromagnet activation. Additionally, it supports a **momentary switch to trigger Motor1 and LED for 10 seconds**, including a **rotating '0' animation** on the display during the timer.

## 🔧 Hardware Components

- Arduino Uno  
- L298 Motor Driver (PWM-compatible pins required)  
- RE-280RA DC Motors ×2  
- KEYESTUDIO Electromagnet Modules ×2  
- TM1637 4-digit 7-segment LED display  
- Push Button Switch  
- External power supply (recommended for motors and electromagnets)

## 📍 Pin Configuration

| Component           | Arduino Pin |
|--------------------|-------------|
| Motor1 IN1 (PWM)   | D5          |
| Motor1 IN2 (PWM)   | D6          |
| Motor2 IN3 (PWM)   | D9          |
| Motor2 IN4 (PWM)   | D10         |
| TM1637 CLK         | D2          |
| TM1637 DIO         | D3          |
| LED Indicator      | D13         |
| Electromagnet 1    | D8          |
| Electromagnet 2    | D11         |
| Momentary Switch   | D7          |

> ⚠️ Note: IN1–IN4 must be connected to PWM-capable pins on the Arduino Uno (3, 5, 6, 9, 10, 11). The switch uses INPUT_PULLUP configuration.

## ⏱️ Operation Overview

- The program begins with a 5-second **interval phase** (cycle 0).  
- Then it alternates between:  
  - **45 seconds of PWM motor operation** (Motor2, `state = true`)  
  - **5 seconds of rest interval** (`state = false`)  
- This loop continues for **10 full cycles**.  
- After the 10th cycle, the display scrolls the word `FINISH` from right to left.  

### 🔹 Motor1 Switch Control

- Pressing the **momentary switch (D7)** triggers Motor1 (IN1/IN2) and the 13-pin LED for **10 seconds**.  
- If the switch is pressed again during the 10-second operation, the timer **resets to 10 seconds from that moment**.  
- **During the 5-second interval phase**, the Motor1 timer is **cancelled** and cannot run. After the interval, the switch can be pressed again for a valid 10-second operation.

### 🔹 Rotating '0' Animation on 7-Segment Display

- While Motor1 is running, the program animates **the '0' digit corresponding to the cycle number** by turning off its segments sequentially:  
  - **Cycle 0:** Ten's place '0' rotates  
  - **Cycle 1–9:** Ten's place '0' rotates  
  - **Cycle 10:** One's place '0' rotates  
- The animation updates every **200 ms**, giving a spinning effect.

## 🎚️ PWM Motor Control

- Motors are controlled using `analogWrite()` for smooth speed control.  
- Each motor has an independent PWM power level:  
  - `motorPower1` and `motorPower2` (default: 255)  
- You can adjust these values to fine-tune motor speed.

## 🧲 Electromagnet Behavior

- **Electromagnet 1** activates at the start of the 45-second motor phase.  
- **Electromagnet 2** activates at the end of the 45-second motor phase.  
- Each electromagnet stays ON for **5 seconds**, independently.  
- While active, the **colon `:` on the 7-segment display lights up for 1 second**.

## 📺 Display Behavior

- The 7-segment display shows:  
  - **Left 2 digits**: countdown seconds  
  - **Right 2 digits**: current cycle number  
- During Motor1's 10-second timer, the appropriate '0' digit rotates while Motor1 is active.  
- After 10 cycles, the display scrolls `FINISH` from right to left.

## ⌚ Timing Logic

- All timing is handled using `millis()` for non-blocking countdowns.  
- Countdown continues even while electromagnets are active.  
- Speed can be adjusted using the `speedFactor` variable (e.g., `2.0` for double speed).  
- Motor1's 10-second timer respects the interval phase and cancels itself if the interval starts.

## 📄 Code Features

- PWM motor control via `analogWrite()`  
- Non-blocking countdown using `millis()`  
- Independent control of two electromagnets  
- Momentary switch control for Motor1/LED  
- Rotating '0' animation during Motor1 timer  
- Visual feedback via colon and scrolling text  
- Modular functions for motor control, display, and magnet timing

## 🚀 Getting Started

1. Connect all components according to the pin configuration.  
2. Upload the Arduino sketch.  
3. Power the motors and electromagnets using an external supply.  
4. Press the switch to test Motor1 and LED operation.  
5. Observe countdown cycles, Motor2 operation, electromagnet activation, and '0' rotation during Motor1 timer.

## 🛠️ Customization Ideas

- Adjust `motorPower1` and `motorPower2` for speed tuning.  
- Adjust `ROTATE_INTERVAL` for faster or slower '0' rotation.  
- Add buzzer or sound feedback during electromagnet activation or Motor1 operation.  
- Display additional messages or animations after `FINISH`.

## 📜 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.
