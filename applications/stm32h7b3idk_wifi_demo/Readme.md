# STM32H7B3I-DK — Wi-Fi Demo

**Zephyr RTOS × LVGL × STM32H7 × Wi-Fi**

A demonstration of **Zephyr RTOS 4.5.0** and **LVGL 9.6.0** running on the **STM32H7B3I-DK** development board.

---

## Build

### Environment

Create and activate the Python virtual environment:

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

The environment must contain an initialized Zephyr workspace with the required dependencies.

### Compile

```bash
west build -p always \
-b stm32h7b3i_dk \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/stm32h7b3idk_wifi_demo"
```

### Flash

```bash
sudo "$(which python)" "$(which west)" flash -r openocd
```

The application uses the board's onboard ST-LINK debugger.

## Testing Wi-Fi module

### Scan

```bash
wifi scan
```

### Connect

```bash
wifi connect -s "ssid" -k 1 -p "pass"
```

### Check

```bash
wifi status
net iface
```

---
