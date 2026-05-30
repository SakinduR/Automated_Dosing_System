# 🏥 IoT Automated Hospital Dosing System

An industrial-grade, distributed IoT automated dosing system designed for hospital environments. This project manages the sequential refilling of decentralized 750ml chemical dispensing tanks utilizing an MQTT publish/subscribe architecture over an isolated Wi-Fi network. It features non-blocking state machine queues and real-time algorithmic fail-safes to prevent mechanical and physical disasters.

## 🏗️ System Architecture & Component Map

The system is split into two distinct firmware environments running on separate ESP32 microcontrollers.

### 1. The Central Controller (The Hub)

The brains of the operation. It manages the central physical plant and handles the logic for the entire hospital network.

- **Core Logic:** First-In-First-Out (FIFO) sequential dispensing queue and non-blocking timers.
- **Physical Hardware Managed:** \* 80 GHz Radar Level Transmitter (Analog)
- Gems FT-330 Water Flow Meter (Analog)
- KROHNE BATCHFLUX Chemical Flow Meter (Analog)
- ProMinent Gamma/X Chemical Pump (PWM)
- Danfoss EV250B Main Water Valve (Relay)

- **Algorithmic Fail-Safes:** Cross-references flow meter data against valve commands. Automatically triggers emergency shutdown loops for Water Blockages (Jammed Rotors) or Chemical Pump/Line failures.
- **Network Role:** Listens to remote nodes, commands specific tank valves to open, and broadcasts real-time system status and critical alarms to the global MQTT broker.

### 2. The Dispenser Node (The Muscle)

A lightweight edge node attached to individual 750ml tanks distributed throughout the hospital.

- **Physical Hardware Managed:**
- Gems ELS-1100 Optical Level Sensor (Pushbutton/Digital Input)
- SMC VX2 Branch Valve (LED/Digital Output)

- **Network Role:** Monitors the optical sensor. When fluid drops, it publishes a refill request to the Central Hub. It blindly waits for the Hub's broadcast command to open or close its local valve.

---

## 📁 Repository Structure

```text
📁 Hospital_Dosing_System
│
├── 📁 Central_Controller       # Project 1: The Main Hub
│   ├── 📁 src
│   │   └── 📄 main.cpp         # Contains queue logic, math, and MQTT callbacks
│   ├── 📄 platformio.ini       # PlatformIO config (Includes LiquidCrystal_I2C & PubSubClient)
│   ├── 📄 diagram.json         # Wokwi hardware layout for the main plant
│   └── 📄 wokwi.toml           # Wokwi path configuration
│
└── 📁 Dispenser_Node           # Project 2: The Distributed Tank
    ├── 📁 src
    │   └── 📄 main.cpp         # Lightweight MQTT listener and hardware trigger
    ├── 📄 platformio.ini       # PlatformIO config (Includes PubSubClient)
    ├── 📄 diagram.json         # Wokwi hardware layout (ESP32, Sensor, Valve)
    └── 📄 wokwi.toml

```

---

## 🚀 How to Initiate the Simulation

This project is built using **PlatformIO** and simulated using **Wokwi** inside Visual Studio Code. Because the project utilizes multiple ESP32 microcontrollers running completely different codebases, you **must open the projects in separate VS Code windows**.

### Prerequisites

- [Visual Studio Code](https://code.visualstudio.com/)
- PlatformIO IDE Extension
- Wokwi Simulator Extension

### Step 1: Boot the Central Hub

1. Open a new VS Code window and open the `Central_Controller` folder.
2. Click the **PlatformIO Checkmark (✓)** in the bottom blue toolbar to build the project and download libraries.
3. Open the VS Code Command Palette (`Ctrl+Shift+P` or `Cmd+Shift+P`) and execute: `Wokwi: Start Simulator`.
4. Wait for the virtual LCD screen to display `WIFI CONNECTED!` and `SYSTEM IDLE`.

### Step 2: Boot the Dispenser Node(s)

1. Open a **second** VS Code window and open the `Dispenser_Node` folder.
2. Build the project using PlatformIO.
3. Start the Wokwi Simulator for this node.
4. Check the Wokwi serial terminal to confirm it says: `Connected to MQTT Broker.`

### Step 3: Run the Network Queue Test

1. Place both VS Code windows side-by-side.
2. On the **Dispenser Node** simulator, click the Green Pushbutton (Optical Sensor).
3. **Observe the Hub:** The Central Hub's LCD will change to `FILLING TANK`. The Red Relay and Blue Pump LED will activate.
4. **Observe the Node:** The Hub will send an MQTT command back to the Node, turning on its Green LED (SMC Valve).
5. **Cycle Complete:** After 15 seconds, the Hub will shut down the main plant and command the Node to close its valve.

---

## 🚨 Testing the Algorithmic Fail-Safes

You can test the Chapter 3 hardware fail-safes directly in the Central Controller simulator:

- **Yellow Warning (Empty Chemical Supply):** While the system is idle, flip the slide switch to the right. The Yellow LED will activate, warning of an empty 10L supply tank.
- **Red Critical Fault (Water Blockage):** Ensure the Water Flow rotary dial is turned up. Initiate a tank refill. While the system is actively filling, quickly turn the Water Flow dial to **zero**. After 2 seconds, the system will detect the physical mismatch, instantly close all valves, flash the Red LED, sound the buzzer, and lock the system to prevent catastrophic damage. Restart the simulator to clear the fault.

---

## 🌐 Web Dashboard Integration (HiveMQ)

Because the microcontrollers are connected to the live internet via Wokwi's virtual gateway, you can monitor and control the system from anywhere using a WebSocket client.

1. Navigate to: `http://www.hivemq.com/demos/websocket-client/`
2. Connect to the public broker.
3. Subscribe to the topic: `hospital/dosing/sakindu/#` to monitor real-time statuses and alarms.
4. Publish the message `REFILL_TANK1` to the topic `hospital/dosing/sakindu/command` to remotely trigger a dispensing cycle directly from the browser.
