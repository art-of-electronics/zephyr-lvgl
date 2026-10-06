# Demo for Viewe Board

## Run venv

```bash
python3 -m venv ~/zephyrproject/.venv
source ~/zephyrproject/.venv/bin/activate

cd ~/zephyrproject
```

## UEDX80480050E-WB-A v1.2

```bash
west build -p always \
$HOME/VSCodeProjects/zephyr-lvgl/applications/viewe_demo \
-b uedx80480050e_wb_a@1.2.0/esp32s3/procpu \
-- -DBOARD_ROOT=$HOME/VSCodeProjects/zephyr-lvgl

west flash
```

---
