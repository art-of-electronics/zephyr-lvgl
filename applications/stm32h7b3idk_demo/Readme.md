# STM32 Demo

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

```bash
west build -p always \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/stm32h7b3idk_demo" \
-b stm32h7b3i_dk
```

```bash
sudo "$(which python)" "$(which west)" flash -r openocd
```

---
