# CYD

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate
cd ~/zephyrproject
```

```bash
west build -p always \
"$HOME/VSCodeProjects/zephyr-lvgl/applications/cyd_demo" \
-b esp32_035/esp32/procpu \
-- -DBOARD_ROOT="$HOME/VSCodeProjects/zephyr-lvgl"
```

---
