# Arduino Timing Controller with Relay and PWM Motor Control, Electromagnets, and 7-Segment Display

This project controls two DC motors and two electromagnet modules using an Arduino Uno. Motor1 is controlled via a relay and triggered by a momentary switch. Motor2 is controlled via PWM signals. A TM1637 4-digit 7-segment LED display provides countdown and cycle feedback. The system performs timed cycles with synchronized electromagnet activation and visual feedback, including a rotating '0' animation during Motor1 operation.

## 🔧 Hardware Components

- Arduino Uno  
- L298 Motor Driver (PWM-compatible pins required for Motor2)  
- DC Motors ×2 (e.g., RE-280RA)  
- Electromagnet Modules ×2 (e.g., KEYESTUDIO)  
- TM1637 4-digit 7-segment LED display  
- Push Button Switches ×2  
- External power supply (recommended for motors and electromagnets)  
- Relay modules for Motor1 and Motor2 status indication (optional)

## 📍 Pin Configuration

| Component                    | Arduino Pin |
|-----------------------------|-------------|
| Electromagnet 1             | D2          |
| Electromagnet 2             | D3          |
| TM1637 CLK                  | D4          |
| TM1637 DIO                  | D5          |
| Motor1 Switch Input         | D6          |
| Motor2 Switch Input         | D7          |
| Motor1 Status Output (Relay)| D8          |
| Motor2 Status Output (Relay)| D9          |
| Motor2 IN1 (PWM)            | D10         |
| Motor2 IN2 (PWM)            | D11         |
| LED Indicator               | D13         |

> ⚠️ Note: Motor2 IN1 and IN2 must be connected to PWM-capable pins on the Arduino Uno (3, 5, 6, 9, 10, 11).  
> D8 and D9 are digital output pins that go HIGH when Motor1 or Motor2 is running. These can be used to drive relay modules for external devices.  
> Both switches use INPUT_PULLUP configuration.

## ⏱️ Operation Overview

- The program begins with a 5-second **interval phase** (cycle 0).  
- Then it alternates between:  
  - **40 seconds of active phase** (`state = true`)  
  - **5 seconds of rest interval** (`state = false`)  
- This loop continues for **10 full cycles**.  
- After the 10th cycle, the display scrolls the word `FINISH` from right to left and shuts down.

### 🔹 Motor1 Switch Control

- Pressing the **momentary switch (D6)** triggers Motor1 and the LED (D13) for **10 seconds**.  
- If the switch is pressed again during the 10-second operation, the timer **resets to 10 seconds from that moment**.  
- **During the 5-second interval phase**, Motor1 is disabled and cannot be triggered.  
- While Motor1 is active, **D8 goes HIGH**, allowing relay-based control of external devices.

### 🔹 Motor2 Operation

- Motor2 runs automatically during each active phase.  
- While Motor2 is active (`state = true`), **D9 goes HIGH**, enabling relay-based control if needed.  
- Pressing the **momentary switch (D7)** increments a counter.  
- After **5 presses**, Motor2 stops for the remainder of the cycle.

### 🔹 Rotating '0' Animation on 7-Segment Display

- While Motor1 is running, the program animates **the '0' digit corresponding to the cycle number** by turning off its segments sequentially:  
  - **Cycle 0–9:** Ten's place '0' rotates  
  - **Cycle 10:** One's place '0' rotates  
- The animation updates every **200 ms**, giving a spinning effect.

## 🎚️ PWM Motor Control

- Motor2 is controlled using `analogWrite()` for smooth speed control.  
- The PWM power level is defined by `motorPower2` (default: 255).  
- You can adjust this value to fine-tune Motor2 speed.

## 🧲 Electromagnet Behavior

- **Electromagnet 1** activates at the start of the active phase.  
- **Electromagnet 2** activates at the end of the active phase.  
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

- PWM motor control for Motor2 via `analogWrite()`  
- Relay-based control for Motor1  
- Non-blocking countdown using `millis()`  
- Independent control of two electromagnets  
- Momentary switch control for Motor1 and Motor2  
- Rotating '0' animation during Motor1 timer  
- Visual feedback via colon and scrolling text  
- Relay-compatible status outputs for Motor1 and Motor2  
- Modular functions for motor control, display, and magnet timing

## 🚀 Getting Started

1. Connect all components according to the pin configuration.  
2. Upload the Arduino sketch.  
3. Power the motors and electromagnets using an external supply.  
4. Press the switch to test Motor1 and LED operation.  
5. Observe countdown cycles, Motor2 operation, electromagnet activation, and '0' rotation during Motor1 timer.  
6. Use D8 and D9 to control relays or monitor motor activity externally.

## 🛠️ Customization Ideas

- Adjust `motorPower2` for Motor2 speed tuning  
- Adjust `ROTATE_INTERVAL` for faster or slower '0' rotation  
- Add buzzer or sound feedback during electromagnet activation or Motor1 operation  
- Display additional messages or animations after `FINISH`  
- Use D8/D9 to trigger external devices like fans, lights, or alarms via relays

## 📜 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.
