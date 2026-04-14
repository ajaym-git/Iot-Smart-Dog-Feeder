# Iot-Smart-Dog-Feeder
IoT based automatic dog feeder using ESP8266 with real time monitoring and scheduling
#  IoT Smart Dog Feeder

## 📌 Overview
This project is an IoT-based automatic dog feeder designed to feed pets on a scheduled time and allow manual control remotely using WiFi.

It uses ESP8266 for connectivity and enables real-time monitoring and control of the feeding process.

---

## * Features
-> Automatic scheduled feeding
-> Manual feeding via IoT (WiFi/MQTT)
-> LCD display for system status
-> WiFi connection status indication
-> Relay-based motor control

---

## * Hardware Components
-> ESP8266 (NodeMCU)
-> Relay Module
-> LCD Display (I2C)
-> Motor / Servo for dispensing food
-> Power Supply

---

## * Software & Technologies
- Embedded C (Arduino IDE)
- ESP8266 WiFi Module
- MQTT Protocol
- I2C Communication (LCD)

---

## ⚙️ Working Principle
The ESP8266 connects to a WiFi network and communicates using MQTT protocol.

- The system can operate in both **automatic mode** (scheduled feeding) and **manual mode** (remote trigger).
- When triggered, the relay activates the motor to dispense food.
- The LCD displays real-time information such as time, WiFi status, and feeding status.


##  Future Improvements
* Mobile app integration
*  Camera monitoring
*  AI-based feeding schedule
*  Battery backup system

---

## * Author
Ajay M
B.E Electronics and Communication Engineering
