# Driver for BMP180
![BMP180 Sensor](https://vn.szks-kuongshun.com/uploads/201810680/small/bmp180-digital-barometric-pressure-sensor-module14122058243.jpg)
![RASPBERRY_PI_3B+](https://bizweb.dktcdn.net/100/354/184/products/770a5614-1617x1080.jpg?v=1557587464217)
**Linux Kernel Driver for BMP180 Pressure Sensor on Raspberry Pi 3B+**
### Nhóm thực hiện
- **Huỳnh Võ Phúc Lộc** – 22146344  
- **Nguyễn Thiện Nhân** – 22146364  
- **Nguyễn Ngọc Hoàng** – 22146309  
- **Nguyễn Bùi Thảo Nguyên** – 22146360  

---

## Table of Contents
1. [Overview](#overview)
2. [Specifications](#specifications)
3. [Hardware & Software Setup](#hardware--software-setup)
4. [Driver Installation & Compilation](#driver-installation--compilation)
5. [Testing the Driver](#testing-the-driver)
6. [Troubleshooting](#troubleshooting)
7. [Author](#author)

---

## Overview

The Bosch **BMP180** is a digital barometric pressure and temperature sensor, commonly used in embedded systems, IoT applications, weather stations, and altimeters.

### Features
- Atmospheric pressure: **300–1100 hPa**, ±0.12 hPa accuracy  
- Temperature: **−40°C to +85°C**, ±0.5°C accuracy  
- Communication: **I2C only**  
- Low power: ~5µA (sleep mode)  
- Size: 3.6 × 3.8 × 0.93 mm  
- 16-bit ADC with onboard calibration  

---

## Specifications

### Electrical & Physical

| Parameter               | Value                           |
|------------------------|----------------------------------|
| Supply Voltage (VDD)   | 1.8V – 3.6V                      |
| Logic Level (VDDIO)    | 1.62V – 3.6V                     |
| Interface              | I2C (up to 3.4 MHz)              |
| Current Consumption    | ~12 µA (active)                  |
| Operating Temperature  | -40°C to +85°C                   |
| Pressure Range         | 300 – 1100 hPa (+9000 m to -500 m) |

### Resolution & Accuracy

| Metric                     | Value         |
|---------------------------|---------------|
| Pressure Resolution        | 0.01 hPa      |
| Absolute Pressure Accuracy| ±1.0 hPa      |
| Temperature Accuracy       | ±1.0 °C       |
| Temperature Resolution     | 0.1 °C        |

### Oversampling Settings (OSS)

| OSS Level               | Conversion Time | RMS Noise | Resolution |
|------------------------|-----------------|-----------|------------|
| 0 (Ultra Low Power)    | ~4.5 ms         | 0.06 hPa  | 16-bit     |
| 1 (Standard)           | ~7.5 ms         | 0.05 hPa  | 17-bit     |
| 2 (High Resolution)    | ~13.5 ms        | 0.04 hPa  | 18-bit     |
| 3 (Ultra High Res.)    | ~25.5 ms        | 0.03 hPa  | 19-bit     |

---

## Hardware & Software Setup

### 3.1 Wiring BMP180 to Raspberry Pi

| BMP180 Pin | RPi 4 Pin (GPIO)     | Description     |
|------------|----------------------|-----------------|
| VIN        | Pin 1 (3.3V)         | Power supply    |
| GND        | Pin 6 (GND)          | Ground          |
| SCL        | Pin 5 (GPIO3 / SCL)  | I2C Clock       |
| SDA        | Pin 3 (GPIO2 / SDA)  | I2C Data        |

### 3.2 Enable I2C Interface

```bash
sudo raspi-config
# Select: Interface Options → I2C → Enable
sudo reboot
```

Check connection:

```bash
i2cdetect -y 1
# Look for address 0x77
```

---

## Driver Installation & Compilation

### 4.1 Download Files

Clone or download the following files from the repository:

- `bmp180_driver.c`
- `bmp180.dts`
- `Makefile`
- `test.c`

GitHub: [Dong-quan/DriverBMP180](https://github.com/Dong-quan/DriverBMP180)

---

### 4.2 Compile Device Tree Overlay

```bash
make
sudo cp bmp180.dtbo /boot/overlays/
sudo nano /boot/config.txt
# Add the following line:
dtoverlay=bmp180
sudo reboot
```

---

### 4.3 Compile the Driver

```bash
make
```

---

### 4.4 Insert the Kernel Module

```bash
sudo insmod bmp180_driver.ko
```

To remove the driver:

```bash
sudo rmmod bmp180_driver.ko
```

---

## Testing the Driver

### 5.1 Compile the Test Program

```bash
gcc -o bmp180_test test.c
```

---

### 5.2 Run the Test

```bash
sudo ./bmp180_test
```

**Expected Output**:

```
Temperature: 33.6 °C (10🕥 AM, 12/5/2025, At Thu Duc City, Ho Chi Minh City)
Temperature level: Hot
Pressure: 1004–1010 hPa (At Thu Duc City, Ho Chi Minh City)
Altitude: 10–25 m (with some areas reaching up to 30 m)
```

---

## Troubleshooting

If the driver does not work, the default `bmp280_i2c` module might interfere.

### Steps:

1. Check if it’s loaded:

```bash
lsmod | grep bmp
```

If you see:

```
bmp280_i2c             16384  0
bmp280                 28672  1 bmp280_i2c
industrialio           90112  1 bmp280
regmap_i2c             16384  1 bmp280_i2c
```

2. Remove conflicting modules:

```bash
sudo rmmod bmp280_i2c
sudo rmmod bmp280
```

3. Reinstall your BMP180 driver:

```bash
sudo insmod bmp180_driver.ko
```

---

## Author

- **Huỳnh Võ Phúc Lộc**
- GitHub: [Dong-quan](https://github.com/Dong-quan)
