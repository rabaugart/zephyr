# Sample-Binaries mit Zephyr

Stand: 4.9.2025 Zephy-Branch [rb-v4.2](
https://github.com/rabaugart/zephyr/tree/rb-v4.2)

Vorbereiten der Zephyr-Umgebung

    source zephyrproject/.venv/bin/activate

Bauen `samples/drivers/display` für *f249*:

    west build -p always -b stm32f429i_disc1 samples/drivers/display

Bauen `samples/drivers/blinky` für Nucleo_l432kc:

    west build -p always -b nucleo_l432kc samples/basic/blinky

