#include <tuple>
#include <type_traits>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include "seg7.h"

struct seg_data {
    struct gpio_dt_spec* spec;
    const char* name;
};

static struct seg_data segs[NUM_BALKEN];

void init_segments() {
    int i;
    for (i=0;i<NUM_BALKEN;i++) {
        segs[i].spec = NULL;
        segs[i].name = NULL;
    }
}

void init_balken( struct gpio_dt_spec* s, enum Balken b ) {
    const char* name = s->port->name;
    bool ret = false;
    printk("Init segment %d/%s\n", b, name);

    ret = gpio_is_ready_dt(s);
    if (s->port && !ret) {
		printk("Error %d: segment device %s is not ready; ignoring it\n",
		       ret, s->port->name);
		s->port = NULL;
	}
	if (s->port) {
		ret = gpio_pin_configure_dt(s, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure sega device %s pin %d\n",
			       ret, s->port->name, s->pin);
			s->port = NULL;
		} else {
			printk("Set up segment at %s pin %d\n", name, s->pin);
			gpio_pin_set_dt(s,0);
		}
		segs[b].spec = s;
		segs[b].name = name;
	}
}

void toggle_all_segments() {
    int i;
    for (i=0;i<NUM_BALKEN;i++) {
        if (segs[i].spec != NULL) {
            gpio_pin_toggle_dt(segs[i].spec);
        }
    }
}

void show_bits(unsigned short b) {
    int i;
    for (i=0;i<NUM_BALKEN;i++) {
        int const v = (b & (1<<i)) ? 1:0;
        gpio_pin_set_dt(segs[i].spec,v);
    }
}

template<Balken... B> constexpr unsigned short bits() {
    return ((1<<B)|...);
}

template<char C,Balken... B> struct SegChar {
    static constexpr char c = C;
    static constexpr unsigned short bi = bits<B...>();
    static void show() {
        show_bits(bi);
        printk("Showing %c, %d\n",c,bi);
    }
};

using SegChar0 = SegChar<'0',B_A,B_B,B_C,B_D,B_E,B_F>;
using SegChar1 = SegChar<'1',B_B,B_C>;
using SegChar2 = SegChar<'2',B_A,B_B,B_G,B_E,B_D>;
using SegChar3 = SegChar<'3',B_A,B_B,B_G,B_C,B_D>;
using SegChar4 = SegChar<'4',B_F,B_G,B_B,B_C>;
using SegChar5 = SegChar<'5',B_A,B_F,B_G,B_C,B_D>;
using SegChar6 = SegChar<'6',B_A,B_F,B_E,B_D,B_C,B_G>;
using SegChar7 = SegChar<'7',B_A,B_B,B_C>;
using SegChar8 = SegChar<'8',B_A,B_B,B_C,B_D,B_E,B_F,B_G>;
using SegChar9 = SegChar<'9',B_D,B_C,B_B,B_A,B_F,B_G>;
using SegCharA = SegChar<'A',B_E,B_F,B_A,B_B,B_C,B_G>;
using SegCharB = SegChar<'B',B_F,B_E,B_D,B_C,B_G>;
using SegCharC = SegChar<'C',B_A,B_F,B_E,B_D>;
using SegCharD = SegChar<'D',B_B,B_G,B_E,B_D,B_C>;
using SegCharE = SegChar<'E',B_A,B_F,B_G,B_E,B_D>;
using SegCharF = SegChar<'F',B_A,B_F,B_G,B_E>;
using AllChars = std::tuple<SegChar0,SegChar1,SegChar2,SegChar3,
    SegChar4,SegChar5,SegChar6,SegChar7,SegChar8,SegChar9,
    SegCharA,SegCharB,SegCharC,SegCharD,SegCharE,SegCharF>;
static constexpr size_t num_chars = std::tuple_size<AllChars>::value;

template<size_t N>
using seg_char_i = std::tuple_element_t<N,AllChars>;

struct show_op {
    template<typename C>
    struct op {
        static void call() {
            C::show();
        }
    };
};

template<typename OP, size_t N>
void call_char_1( char c ) {
    using char_i = typename std::tuple_element_t<N,AllChars>;
    using op = typename OP::op<char_i>;
    if (c==char_i::c) {
        op::call();
    } else {
        if constexpr (N>0) {
            call_char_1<OP,N-1>(c);
        } else {
            printk("Unknown char");
        }
    }
}

template<typename OP>
void call_char( char c ) {
    call_char_1<OP,num_chars-1>(c);
}

void show_char( char c ) {
    call_char<show_op>(c);
}

template<size_t N> unsigned short get_bits(size_t i) {
    if (N==i) {
        return seg_char_i<N>::bi;
    }
    if constexpr (N>0) {
        return get_bits<N-1>(i);
    }
    return 0;
}

unsigned short next_char_bits() {
    static size_t st = 0;
    auto const ret = get_bits<num_chars-1>(st);
    st = (st+1) % num_chars;
    return ret;
}
