# Arduino Timing Controller with Relay, PWM Motor, Electromagnet, and 7-Segment Display

This project uses an Arduino Uno to control two DC motors and one electromagnet module with precise timing logic. A TM1637 4-digit 7-segment LED display provides countdown and cycle feedback. The system performs timed cycles with synchronized motor and electromagnet activation, user input handling, and visual feedback.

## 🔧 Hardware Components

- Arduino Uno  
- L298 Motor Driver (PWM-compatible pins required for Motor2)  
- DC Motors ×2 (e.g., RE-280RA)  
- Electromagnet ×1 (e.g., KEYESTUDIO)  
- TM1637 4-digit 7-segment LED display  
- Push Button Switches ×2 (Start and Motor1)  
- Piezo sensor for Motor2 stop  
- External power supply (recommended for motors and electromagnet)  
- Relay modules for Motor1 and Motor2 status indication (optional)

## 📍 Pin Configuration

| Component                    | Arduino Pin |
|-----------------------------|-------------|
| Electromagnet               | D2          |
| TM1637 CLK                  | D4          |
| TM1637 DIO                  | D5          |
| Start Switch Input          | D6          |
| Motor1 Switch Input         | D7          |
| Motor2 Piezo Input          | D8          |
| Motor1 Status Output (Relay)| D9          |
| Motor2 Status Output (Relay)| D10         |
| Motor2 IN1 (PWM)            | D11         |
| Motor2 IN2 (PWM)            | D12         |
| LED Indicator               | D13         |

> ⚠️ Note: Motor2 IN1 and IN2 must be connected to PWM-capable pins on the Arduino Uno.  
> D9 and D10 are digital output pins that go HIGH when Motor1 or Motor2 is running.  
> Both switches use `INPUT_PULLUP` configuration.

## ⏱️ Operation Overview

- System starts in **READY_PHASE** with scrolling "READY" display.  
- Pressing the **start switch** transitions to **COOL_PHASE** (5-second rest interval).  
- After the rest interval, the system enters **ACTIVE_PHASE** (30-second active cycle).  
- The program repeats **10 full cycles**, alternating between active and cool phases.  
- After the 10th cycle, the display scrolls `FIN` and the system resets.

## 🔹 Motor1 Switch Control

- Pressing the **momentary switch (D7)** during ACTIVE_PHASE triggers Motor1 for **10 seconds**.  
- The 10-second timer starts immediately regardless of remaining phase time.  
- If the phase changes to COOL_PHASE or READY_PHASE, Motor1 stops immediately.  
- While Motor1 is active, **D9 goes HIGH**, allowing relay-based control of external devices.

## 🔹 Motor2 Operation

- Motor2 runs automatically during ACTIVE_PHASE.  
- Pressing the **piezo sensor (D8)** increments a counter.  
- After **5 piezo hits**, Motor2 stops for the remainder of the cycle.  
- While active, **D10 goes HIGH**, enabling relay-based control if needed.

## 🔹 7-Segment Display

- **Left 2 digits**: countdown seconds remaining in the phase.  
- **Right 2 digits**: current cycle number.  
- During Motor1's 10-second timer, the corresponding '0' digit rotates to indicate Motor1 activity.  
- READY_PHASE features a scrolling "READY" message.  
- After 10 cycles, the display scrolls `FIN`.

## 🔹 Active Phase Behavior

- **30-second active phase**:  
  - Motor2 runs automatically.  
  - Motor1 can be triggered by the switch for 10 seconds.  
  - Electromagnet activates at the start of each cycle for 10 seconds.  
- Motor1 respects phase boundaries: stops if phase changes.  
- Countdown updates every second using `millis()` (non-blocking).

## 🔹 Cool Phase Behavior

- **5-second rest interval**:  
  - Motors are stopped.  
  - READY scroll animation is not active to prevent 7-segment flicker.  
  - Electromagnet remains off.

## 🎚️ PWM Motor Control

- Motor2 is controlled using `analogWrite()` for smooth speed control.  
- PWM power level is set to 255 by default.

## 🧲 Electromagnet Control

- The electromagnet is activated at the start of each active cycle for 10 seconds.  
- Its timing is independent and does not block other operations.

## ⌚ Timing Logic

- All timing uses `millis()` to allow non-blocking countdowns.  
- Motor1’s 10-second timer is independent but cancels if phase changes.  
- Piezo input is debounced with 100 ms to prevent false triggers.

## 📄 Code Features

- Motor1: relay-controlled, 10-second timer during ACTIVE_PHASE  
- Motor2: PWM speed control, stops after 5 piezo hits  
- Non-blocking phase countdown using `millis()`  
- Electromagnet: independent 10-second activation  
- 7-segment display: countdown, cycle, and Motor1 rotation animation  
- Scrollable READY and FIN messages  
- Relay-compatible status outputs for Motor1 and Motor2  
- Modular functions for motor control, display, and magnet timing

## 🚀 Getting Started

1. Connect all components according to the pin configuration.  
2. Upload the Arduino sketch.  
3. Power motors and electromagnet with an external supply.  
4. Press the start switch to initiate the system.  
5. Observe countdown cycles, Motor2 operation, Motor1 rotation animation, piezo sensor detection, and electromagnet activation.  
6. Use D9 and D10 to control relays or monitor motor activity externally.

## 🛠️ Customization Ideas

- Adjust Motor2 PWM power for speed tuning.  
- Adjust display update intervals for smoother animations.  
- Add buzzer or visual feedback during electromagnet activation or Motor1 operation.  
- Display additional messages after `FIN`.  
- Use D9/D10 to trigger external devices like fans, lights, or alarms.

## 📜 License

This project is licensed under the MIT License.  
See the [LICENSE](LICENSE) file for details.
