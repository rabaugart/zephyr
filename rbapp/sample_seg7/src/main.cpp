/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NOTE: If you are looking into an implementation of button events with
 * debouncing, check out `input` subsystem and `samples/subsys/input/input_dump`
 * example instead.
 */

#include <vector>
#include <array>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

#include "seg7.h"

#define SLEEP_TIME_MS	1

/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS_OKAY(SW0_NODE)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

#define SEGA_NODE DT_ALIAS(sega)
#if !DT_NODE_HAS_STATUS_OKAY(SEGA_NODE)
#error "sega missing"
#endif

static constexpr size_t STACKSIZE=1024;
static constexpr size_t PRIORITY=7;

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
						     {0});
static struct gpio_dt_spec sega = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sega), gpios,
						     {0});
static struct gpio_dt_spec segb = GPIO_DT_SPEC_GET_OR(DT_ALIAS(segb), gpios,
						     {0});
static struct gpio_dt_spec segc = GPIO_DT_SPEC_GET_OR(DT_ALIAS(segc), gpios,
						     {0});
static struct gpio_dt_spec segd = GPIO_DT_SPEC_GET_OR(DT_ALIAS(segd), gpios,
						     {0});
static struct gpio_dt_spec sege = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sege), gpios,
						     {0});
static struct gpio_dt_spec segf = GPIO_DT_SPEC_GET_OR(DT_ALIAS(segf), gpios,
						     {0});
static struct gpio_dt_spec segg = GPIO_DT_SPEC_GET_OR(DT_ALIAS(segg), gpios,
						     {0});

//////////////////////////////////////////////////
// WorkerCommand and Queue
enum class WorkerCommand {
    Start,
    Stop
};

K_MSGQ_DEFINE( worker_queue, sizeof(WorkerCommand), 5, 1);

//////////////////////////////////////////////////
// ButtonPressCommand and Queue
enum class ButtonPressCommand {
    B1,
};
K_MSGQ_DEFINE( button_press_queue, sizeof(WorkerCommand), 5, 1);

//////////////////////////////////////////////////
// Feedback ist das Flashen der Bestätigungs-LED auf dem Board
enum class FeedBack {
    LongFlash,
    ShortFlash
};
K_MSGQ_DEFINE( feedback_queue, sizeof(FeedBack), 5, 1);

void feedback_handler(void) {
    using item_t = std::pair<FeedBack,std::vector<size_t>>;
    static const std::array<item_t,2> zeiten = {
        item_t{FeedBack::LongFlash,{500}},
        item_t{FeedBack::ShortFlash,{200,100,200}}
    };
    while (true) {
        FeedBack f;
        int const ret = k_msgq_get(&feedback_queue, &f, K_FOREVER);
        if (ret==0) {
            for (auto const& i:zeiten) {
                if (i.first==f) {
                    int status = 1;
                    for (auto const zi:i.second) {
                        gpio_pin_set_dt(&led, status);
                        k_sleep(K_MSEC(zi));
                        status = 1-status;
                    }
                    gpio_pin_set_dt(&led, 0);
                }
            }

        }
    }
}

K_THREAD_DEFINE( feedback_handler_id, STACKSIZE, feedback_handler, NULL, NULL, NULL, PRIORITY, 0, 0);

//////////////////////////////////////////////////

bool running = false;

//////////////////////////////////////////////////
void button_pressed_isr(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
    printk("Button pressed at %lld ms\n", k_uptime_get());
    ButtonPressCommand bcmd = ButtonPressCommand::B1;
    k_msgq_put(&button_press_queue,&bcmd, K_NO_WAIT);
}

//////////////////////////////////////////////////
void button_handler(void) {
    while(1) {
        ButtonPressCommand bcmd;
        k_msgq_get(&button_press_queue, &bcmd, K_FOREVER);
        ButtonPressCommand bcmd2;
        int const result = k_msgq_get(&button_press_queue, &bcmd2, K_MSEC(500));
        if (result==0) {
            printk("Doppelt\n");
            FeedBack const f = FeedBack::LongFlash;
            k_msgq_put(&feedback_queue,&f, K_NO_WAIT);
        } else {
            printk("Einfach\n");
            WorkerCommand const wcmd = running ? WorkerCommand::Stop : WorkerCommand::Start;
            k_msgq_put(&worker_queue,&wcmd, K_NO_WAIT);
            FeedBack const f = FeedBack::ShortFlash;
            k_msgq_put(&feedback_queue,&f, K_NO_WAIT);
        }
    }
}

K_THREAD_DEFINE( button_handler_id, STACKSIZE, button_handler, NULL, NULL, NULL, PRIORITY, 0, 0);

//////////////////////////////////////////////////

void takter(void) {
    enum WorkerCommand cmd;
    k_msgq_get(&worker_queue, &cmd, K_FOREVER);
    while(1) {
        k_msgq_get(&worker_queue, &cmd, K_MSEC(500));
        switch (cmd) {
            case WorkerCommand::Start:
                running = true;
                break;
            case WorkerCommand::Stop:
                running = false;
                break;
        }
        if (running) {
            show_bits(next_char_bits());
        }
    }
}

K_THREAD_DEFINE( takter_id, STACKSIZE, takter, NULL, NULL, NULL, PRIORITY, 0, 0);

//////////////////////////////////////////////////
void main_thread(void)
{
	int ret;
	enum WorkerCommand wcmd;

	printk("Starting rbapp\n");

	if (!gpio_is_ready_dt(&button)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led.port && !gpio_is_ready_dt(&led)) {
		printk("Error %d: LED device %s is not ready; ignoring it\n",
		       ret, led.port->name);
		led.port = NULL;
	}
	if (led.port) {
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure LED device %s pin %d\n",
			       ret, led.port->name, led.pin);
			led.port = NULL;
		} else {
			printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}

	init_segments();
	init_balken( &sega, B_A);
	init_balken( &segb, B_B);
	init_balken( &segc, B_C);
	init_balken( &segd, B_D);
	init_balken( &sege, B_E);
	init_balken( &segf, B_F);
	init_balken( &segg, B_G);

	printk("Press the button\n");
	if (led.port) {
	    wcmd = WorkerCommand::Start;
        k_msgq_put(&worker_queue,&wcmd, K_NO_WAIT);
		while (1) {
			/* If we have an LED, match its state to the button's. */
			int val = gpio_pin_get_dt(&button);

			if (val >= 0) {
				//gpio_pin_set_dt(&led, val);
			}
			k_msleep(SLEEP_TIME_MS);
		}
	}
}

K_THREAD_DEFINE( main_thread_id, STACKSIZE, main_thread, NULL, NULL, NULL, PRIORITY, 0, 0);
