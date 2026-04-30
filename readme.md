# SMSDOOR – GSM + WiFi Door & Roller Shutter Controller  
(RP2040 + A7670E + Pico W)

SMSDOOR is a hybrid GSM (SMS) and via WiFi Access Point configurated door and roller shutter controller based on a Raspberry Pi Pico / Pico W (RP2040) and a SIMCom A7670E 4G modem.

The system allows authorized users to control a roller shutter or overhead garage door via:
- SMS
- Built-in WiFi web interface
- Serial console

---

# Features

## Control
- Roller shutter control (UP / DOWN)
- Overhead garage door control (DOWN)
- Control via SMS, Web UI, or Console

## Connectivity
- LTE modem (SMS control)
- WiFi Access Point (Pico W)
- Built-in webserver (mobile friendly UI)

---

# Hardware

## Main components
- Raspberry Pi Pico / Pico W
- SIMCom A7670E LTE modem
- Relay outputs
- External 5V power supply (2–3A recommended)

## ⚠️ Power note

WiFi + LTE modem can cause current spikes.  
Use a stable 5V / 2A–3A supply. PC USB is often not sufficient.

---

# UART connections

| Function | Pico Pin |
|----------|----------|
| Debug TX | GP0 |
| Debug RX | GP1 |
| Modem TX | GP20 |
| Modem RX | GP21 |

---

# Build

    mkdir build
    cd build
    cmake ..
    ninja

---

# First Start

Send SMS:

    INIT

Sender becomes admin.

---

# Commands

## Door

    UP                  Roller shutter up
    DOWN                Roller shutter down
    OVERHEAD DOWN       Close overhead door
    CLOSEAT <hh:mm>     Auto close roller shutter and  overhead door daily on a given time
    CLOSEAT OFF         auto close off

---

# Phonebook

    ADD +31612345678    adds a phonenumber to the phonebook
    DEL +31612345678    deletes a phonenumber from the phonebook
    LIST                lists all phonenumbers 

---

# Admin

    PROMOTE +31612345678    promote a number to an administrator
    DEMOTE +31612345678     demote a number to a normal user (only open/close)

---

# System

    PIN <code> *        Set a new PIN code to unlock the SIM card
    SIDD <name>         Set a new SSID for the WiFi AP
    PASS <passwd>       Set a new password for the WiFi AP
    AT <command> *      Send a command directly to the A7670E modem
    INFO                gives some system information
    LOG                 gives a log about most important events
    HELP                gives help information
    WIFI ON/OFF         switch on the WiFi AP for 15 minutes or switches off

---

# WiFi & Web Interface

- Access point (default 192.168.4.1)
- Mobile UI
- Door control and system information
- User management
- Settings configuration
- Console (live log)
- Help page

---

# Notes

- Non-blocking firmware
- Flash storage
- Designed for stability

---

# Author

Frank Beentjes  
https://github.com/Fbeen

