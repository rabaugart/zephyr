
#if !defined(SEG7_HEADER)
#define SEG7_HEADER

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Balken {
    B_A,
    B_B,
    B_C,
    B_D,
    B_E,
    B_F,
    B_G
};

#define NUM_BALKEN 7

void init_segments();
void init_balken( struct gpio_dt_spec * s, enum Balken b );

void show_bits(unsigned short b);
void show_char( char c );

unsigned short next_char_bits();

void toggle_all_segments();

#ifdef __cplusplus
}
#endif

#endif
