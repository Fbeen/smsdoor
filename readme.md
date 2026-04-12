# SMSDOOR – GSM Rolluik & Overhead Door Controller (RP2040 + A7670E)

SMSDOOR is a GSM/SMS controlled door and roller shutter controller based on a Raspberry Pi Pico (RP2040) and a SIMCom A7670E 4G modem.  
The system allows authorized users to control a roller shutter or overhead garage door via SMS commands. It also includes a phonebook with user/admin permissions, logging, and automatic closing schedules.

---

# Features
- Control roller shutter via SMS
- Control overhead garage door via SMS
- Phonebook with user and admin roles
- Automatic daily closing time
- Event logging
- SIM PIN storage in flash
- Modem AT command passthrough (console)
- Console interface via UART/USB
- Non-blocking firmware design
- Flash storage for configuration and phonebook

---

# Hardware
## Main components
- Raspberry Pi Pico (RP2040)
- SIMCom A7670E LTE modem
- Relay outputs for shutter/door control
- External 5V power supply (2 or 3 ampere recommanded)
- UART connection between Pico and modem

## UART connections
| Function | Pico Pin |
|----------|----------|
| Debug TX | GP0 |
| Debug RX | GP1 |
| Modem TX | GP4 |
| Modem RX | GP5 |

---

# Installation
## Build
Requires:
- Pico SDK
- CMake
- Ninja
- ARM GCC toolchain

Build steps:

    mkdir build
    cd build
    cmake ..
    ninja

UF2 file will be generated in the build folder.

Flash by holding BOOTSEL while plugging USB and copying the UF2 file.

**or make it simple and install Visual Studio Code**
---

# First Start / Initialization
When the system is started for the first time, the phonebook is empty.

Send SMS:

    INIT

The sender of this SMS becomes the first admin.

After initialization, only known phone numbers are allowed to control the system.

---

# User Roles

## User
Allowed commands:
- UP
- DOWN

## Admin
Admins can also:
- Close overhead door
- add and delete users
- list users
- promote and demote admins
- change automatic close time
- ask system information
- ask log

## Console Only
Only via serial console:
- set SIM pin code
- send modem commands directly to the A7670 modem

---

# Command Reference

## Door control

### UP
Open the roller shutter.

Aliases: UP, OPEN, OMHOOG, OP

### DOWN
Close the roller shutter.

Aliases: DOWN, DICHT, OMLAAG, CLOSE, NEER

---

## Overhead garage door

### OVERHEAD DOWN
Closes the overhead garage door.

Also accepted: GARAGE DOWN, GARAGE DICHT, GARAGE OMLAAG, GARAGE CLOSE, GARAGE NEER, OVERHEAD DICHT, OVERHEAD OMLAAG, OVERHEAD CLOSE, OVERHEAD NEER

---

# Automatic Closing

### Set automatic closing time

    CLOSEAT 21:30

### Disable automatic closing

    CLOSEAT OFF

The time is stored in flash and constantly monitored.

---

# Phonebook Commands

### Add user

    ADD +31612345678

The new user will receive a welcome SMS.

### Remove user

    DEL +31612345678

Restrictions:
- You cannot delete your own number via SMS
- You cannot delete the last admin

### List users

    LIST

Admins are marked with *

Example:

    +31611111111 *
    +31622222222
    Total numbers: 2

---

# Admin Management

### Promote user to admin

    PROMOTE +31612345678

The promoted user receives a notification SMS.

### Demote admin to user

    DEMOTE +31612345678

Restrictions:
- You cannot demote yourself
- You cannot demote the last admin

---

# System Commands

### INFO
Shows system information:
- Firmware version
- Uptime
- System time
- Number of users/admins
- Automatic close time

    INFO

Example output:

    SMSDOOR v1.0
    Uptime: 01:23:45
    System time: 28-03-2026 17:42
    Users 3 Admins 1
    Auto close time: 21:30

### LOG
Shows recent events.

Via SMS → last events  
Via console → full log

    LOG

---

# Console Commands

These commands only work via serial console.

### Store SIM PIN

    PIN 1234

Rules:
- Must be numeric
- Must be 4–6 digits

After storing the PIN:
- The modem is reset
- The Pico reboots

### Send AT command directly to modem

    AT AT+CSQ
    AT AT+CCLK?
    AT ATI

---

# Logging
The system logs:
- Initialization
- User added
- User removed
- User promoted
- User demoted
- Shutter up/down
- Overhead door close
- Auto close changes
- PIN changes

View log with:

    LOG

---

# Security Model
SMS commands are only accepted when:
1. Sender number exists in the phonebook
2. Sender has correct permission level (user vs admin)
3. Console-only commands are blocked via SMS

This acts as a simple firewall for SMS control.

---

# Example Workflow

Initialize system:

    INIT

Add users:

    ADD +31611111111
    ADD +31622222222

Promote a user:

    PROMOTE +31622222222

Set automatic closing:

    CLOSEAT 22:00

Daily use:

    UP
    DOWN
    OVERHEAD DOWN
    INFO
    LOG

---

# Project Structure

    .
    ├── src       (c code files)
    ├── include   (header files)
    └── hardware  (KiCad files)

---

# License
Open-source project. Use at your own risk.

---

# Author
Frank Beentjes  
https://github.com/Fbeen