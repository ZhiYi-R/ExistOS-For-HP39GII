/**
 * @file Bootloader/drivers/input/stmp_gpio.cpp
 * @brief GPIO keyboard-matrix driver — pure-static singleton class.
 *
 * Phase 2 of the HAL C++23 migration: the input driver becomes the @c Keyboard
 * pure-static singleton. The scan matrices and the pending-change latch were
 * file-scope globals with no cross-TU references, so they move into the class as
 * @c private @c static state.
 *
 * Two codegen-preserving details mirror what -Os did to the original globals:
 *  - @c key_matrix and @c key_matrix_last were two adjacent file-scope @c .bss
 *    arrays, so -Os addressed @c key_matrix_last as @c (key_matrix_base+55) off a
 *    single shared base. Two independent @c static @c inline members would become
 *    two separate COMDAT symbols and defeat that, so they are grouped into one
 *    @c State aggregate (still zero-init -> @c .bss, no global ctor).
 *  - @c set_row_line / @c read_col_line were file-scope @c static helpers with a
 *    single (loop-body) call site, which -Os folded into the scan path; they are
 *    @c always_inline methods now so that folding is preserved.
 *
 * The legacy @c portKeyboard* / @c portKey* entries survive as thin @c extern
 * @c "C" forwarding shims (keyboard_up.h declares them @c extern @c "C"; they are
 * reached by name from C translation units). Each entry has a single caller (its
 * shim), so always_inline folds the body back into the named entry bit-for-bit.
 * Caller migration onto @c Keyboard:: is deferred to the layer-merge phase.
 */


#include "keyboard_up.h"

#include "reg_model.hpp"

#include "debug.h"

class Keyboard {
public:
    // Single-caller entries (their extern "C" shims); always_inline folds each
    // body into the named entry so it is bit-for-bit the pre-class function.
    [[gnu::always_inline]] static void gpioInit();
    [[gnu::always_inline]] static void scan();
    [[gnu::always_inline]] static bool isKeyDown(Keys_t key);
    [[gnu::always_inline]] static Keys_t getChangedKey();

private:
    // key_matrix and key_matrix_last were two adjacent file-scope .bss arrays
    // that -Os addressed off one shared base; grouping them into a single
    // aggregate (zero-init -> .bss, no global ctor) keeps that shared-base
    // codegen, where two independent static-inline members would not.
    struct State {
        uint8_t key_matrix[5][11];
        uint8_t key_matrix_last[5][11];
    };
    static inline State st = {};
    static inline Keys_t ChangedKey = (Keys_t)255;

    // Single-call-site (loop-body) helpers that -Os folded into the scan path;
    // always_inline preserves that folding now that they are methods.
    [[gnu::always_inline]] static void set_row_line(int row_line);
    [[gnu::always_inline]] static unsigned int read_col_line(int col_line);
};


inline void Keyboard::gpioInit()
{
    unsigned int tmp_DOUT;
    unsigned int tmp_DOE;

    reg::PINCTRL_MUXSEL3::clr(reg::PINCTRL_MUXSEL3_::BANK1_PIN22::mask |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN23::mask |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN24::mask |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN25::mask |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN26::mask |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN27::mask);
    reg::PINCTRL_MUXSEL3::set(reg::PINCTRL_MUXSEL3_::BANK1_PIN22::val(3) |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN23::val(3) |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN24::val(3) |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN25::val(3) |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN26::val(3) |
                              reg::PINCTRL_MUXSEL3_::BANK1_PIN27::val(3));
    reg::PINCTRL_MUXSEL4::clr(reg::PINCTRL_MUXSEL4_::BANK2_PIN02::mask |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN03::mask |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN04::mask |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN05::mask |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN06::mask |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN07::mask);
    reg::PINCTRL_MUXSEL4::set(reg::PINCTRL_MUXSEL4_::BANK2_PIN02::val(3) |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN03::val(3) |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN04::val(3) |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN05::val(3) |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN06::val(3) |
                              reg::PINCTRL_MUXSEL4_::BANK2_PIN07::val(3));
    reg::PINCTRL_MUXSEL4::clr(reg::PINCTRL_MUXSEL4_::BANK2_PIN08::mask);
    reg::PINCTRL_MUXSEL4::set(reg::PINCTRL_MUXSEL4_::BANK2_PIN08::val(3));

    reg::PINCTRL_MUXSEL4::clr(reg::PINCTRL_MUXSEL4_::BANK2_PIN14::mask);
    reg::PINCTRL_MUXSEL4::set(reg::PINCTRL_MUXSEL4_::BANK2_PIN14::val(3));

    reg::PINCTRL_MUXSEL0::clr(reg::PINCTRL_MUXSEL0_::BANK0_PIN14::mask);
    reg::PINCTRL_MUXSEL0::set(reg::PINCTRL_MUXSEL0_::BANK0_PIN14::val(3));

    reg::PINCTRL_MUXSEL1::clr(reg::PINCTRL_MUXSEL1_::BANK0_PIN20::mask);
    reg::PINCTRL_MUXSEL1::set(reg::PINCTRL_MUXSEL1_::BANK0_PIN20::val(3));

    //col setting
    tmp_DOUT = reg::PINCTRL_DOUT1::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE1::B().DOE;
    tmp_DOUT |= ((1 << 22) | (1 << 23) | (1 << 25) | (1 << 26) | (1 << 27));
    tmp_DOE &= ~((1 << 22) | (1 << 23) | (1 << 25) | (1 << 26) | (1 << 27));
    reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE1::clr(reg::PINCTRL_DOE1_::DOE::mask); reg::PINCTRL_DOE1::set(reg::PINCTRL_DOE1_::DOE::val(tmp_DOE));

    //row setting
    tmp_DOUT = reg::PINCTRL_DOUT2::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE2::B().DOE;
    tmp_DOUT &= ~((1 << 14) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
    tmp_DOE |= ((1 << 14) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
    reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE2::clr(reg::PINCTRL_DOE2_::DOE::mask); reg::PINCTRL_DOE2::set(reg::PINCTRL_DOE2_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));

    tmp_DOUT = reg::PINCTRL_DOUT1::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE1::B().DOE;
    tmp_DOUT &= ~((1 << 24));
    tmp_DOE |= ((1 << 24));
    reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE1::clr(reg::PINCTRL_DOE1_::DOE::mask); reg::PINCTRL_DOE1::set(reg::PINCTRL_DOE1_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));

    tmp_DOUT = reg::PINCTRL_DOUT0::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE0::B().DOE;
    tmp_DOUT &= ~((1 << 20));
    tmp_DOE |= ((1 << 20));
    reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE0::clr(reg::PINCTRL_DOE0_::DOE::mask); reg::PINCTRL_DOE0::set(reg::PINCTRL_DOE0_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));

    //key ON
    tmp_DOUT = reg::PINCTRL_DOUT0::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE0::B().DOE;
    tmp_DOUT |= ((1 << 14));
    tmp_DOE &= ~((1 << 14));
    reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE0::clr(reg::PINCTRL_DOE0_::DOE::mask); reg::PINCTRL_DOE0::set(reg::PINCTRL_DOE0_::DOE::val(tmp_DOE));

}

inline void Keyboard::set_row_line(int row_line) {
    unsigned int tmp_DOUT;
    unsigned int tmp_DOE;

    tmp_DOUT = reg::PINCTRL_DOUT2::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE2::B().DOE;
    tmp_DOUT |= ((1 << 14) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
    tmp_DOE |= ((1 << 14) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
    reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE2::clr(reg::PINCTRL_DOE2_::DOE::mask); reg::PINCTRL_DOE2::set(reg::PINCTRL_DOE2_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));

    tmp_DOUT = reg::PINCTRL_DOUT1::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE1::B().DOE;
    tmp_DOUT |= ((1 << 24));
    tmp_DOE |= ((1 << 24));
    reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE1::clr(reg::PINCTRL_DOE1_::DOE::mask); reg::PINCTRL_DOE1::set(reg::PINCTRL_DOE1_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));

    tmp_DOUT = reg::PINCTRL_DOUT0::B().DOUT;
    tmp_DOE = reg::PINCTRL_DOE0::B().DOE;
    tmp_DOUT |= ((1 << 20));
    tmp_DOE |= ((1 << 20));
    reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));
    reg::PINCTRL_DOE0::clr(reg::PINCTRL_DOE0_::DOE::mask); reg::PINCTRL_DOE0::set(reg::PINCTRL_DOE0_::DOE::val(tmp_DOE));
    reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));

    switch (row_line) {
    case 5:
        tmp_DOUT = reg::PINCTRL_DOUT1::B().DOUT;
        tmp_DOE = reg::PINCTRL_DOE1::B().DOE;
        tmp_DOUT &= ~((1 << 24));
        tmp_DOE |= ((1 << 24));
        reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));
        reg::PINCTRL_DOE1::clr(reg::PINCTRL_DOE1_::DOE::mask); reg::PINCTRL_DOE1::set(reg::PINCTRL_DOE1_::DOE::val(tmp_DOE));
        reg::PINCTRL_DOUT1::clr(reg::PINCTRL_DOUT1_::DOUT::mask); reg::PINCTRL_DOUT1::set(reg::PINCTRL_DOUT1_::DOUT::val(tmp_DOUT));
        break;
    case 8:
        tmp_DOUT = reg::PINCTRL_DOUT0::B().DOUT;
        tmp_DOE = reg::PINCTRL_DOE0::B().DOE;
        tmp_DOUT &= ~((1 << 20));
        tmp_DOE |= ((1 << 20));
        reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));
        reg::PINCTRL_DOE0::clr(reg::PINCTRL_DOE0_::DOE::mask); reg::PINCTRL_DOE0::set(reg::PINCTRL_DOE0_::DOE::val(tmp_DOE));
        reg::PINCTRL_DOUT0::clr(reg::PINCTRL_DOUT0_::DOUT::mask); reg::PINCTRL_DOUT0::set(reg::PINCTRL_DOUT0_::DOUT::val(tmp_DOUT));
        break;
    default:
        tmp_DOUT = reg::PINCTRL_DOUT2::B().DOUT;
        tmp_DOE = reg::PINCTRL_DOE2::B().DOE;
        switch (row_line) {
        case 0:
            tmp_DOUT &= ~((1 << 6));
            break;
        case 1:
            tmp_DOUT &= ~((1 << 5));
            break;
        case 2:
            tmp_DOUT &= ~((1 << 4));
            break;
        case 3:
            tmp_DOUT &= ~((1 << 2));
            break;
        case 4:
            tmp_DOUT &= ~((1 << 3));
            break;
        case 6:
            tmp_DOUT &= ~((1 << 8));
            break;
        case 7:
            tmp_DOUT &= ~((1 << 7));
            break;
        case 9:
            tmp_DOUT &= ~((1 << 14));
            break;
        }
        tmp_DOE |= ((1 << 14) | (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 2));
        reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));
        reg::PINCTRL_DOE2::clr(reg::PINCTRL_DOE2_::DOE::mask); reg::PINCTRL_DOE2::set(reg::PINCTRL_DOE2_::DOE::val(tmp_DOE));
        reg::PINCTRL_DOUT2::clr(reg::PINCTRL_DOUT2_::DOUT::mask); reg::PINCTRL_DOUT2::set(reg::PINCTRL_DOUT2_::DOUT::val(tmp_DOUT));
        break;
    }
}


inline unsigned int Keyboard::read_col_line(int col_line) {
    switch (col_line) {
    case 0:
        return (reg::PINCTRL_DIN1::B().DIN >> 23) & 1;
    case 1:
        return (reg::PINCTRL_DIN1::B().DIN >> 25) & 1;
    case 2:
        return (reg::PINCTRL_DIN1::B().DIN >> 27) & 1;
    case 3:
        return (reg::PINCTRL_DIN1::B().DIN >> 26) & 1;
    case 4:
        return (reg::PINCTRL_DIN1::B().DIN >> 22) & 1;
    default:
        break;
    }
    return 0;
}

inline Keys_t Keyboard::getChangedKey()
{
    Keys_t ret;
    if(ChangedKey == 255){
        return (Keys_t)255;
    }

    st.key_matrix_last[ChangedKey % 8][ChangedKey >> 3] = st.key_matrix[ChangedKey % 8][ChangedKey >> 3];

    ret = ChangedKey;
    ChangedKey = (Keys_t)255;
    return ret;
}

inline bool Keyboard::isKeyDown(Keys_t key)
{
    return st.key_matrix[key % 8][key >> 3] != 0u;
}

inline void Keyboard::scan()
{

    for (int y = 0; y < 10; y++) {
        set_row_line(y);
        for (int x = 0; x < 5; x++) {
            st.key_matrix[x][y] = !read_col_line(x);
            if(st.key_matrix_last[x][y] != st.key_matrix[x][y])
            {
                if(ChangedKey == 255)
                    ChangedKey = (Keys_t)((y << 3) + x);
            }
        }
    }

    st.key_matrix[0][10] = ((reg::PINCTRL_DIN0::B().DIN >> 14) & 1);

    if(st.key_matrix_last[0][10] != st.key_matrix[0][10])
    {
        if(ChangedKey == 255)
            ChangedKey = (Keys_t)((10 << 3) + 0);
    }

}

// ---------------------------------------------------------------------------
// extern "C" seams (keyboard_up.h declares the interface extern "C").
// Caller migration onto Keyboard:: is deferred to the layer-merge phase.
// ---------------------------------------------------------------------------
extern "C" void portKeyboardGPIOInit()        { Keyboard::gpioInit(); }
extern "C" void portKeyScan()                 { Keyboard::scan(); }
extern "C" bool portIsKeyDown(Keys_t key)     { return Keyboard::isKeyDown(key); }
extern "C" Keys_t portGetChangedKey()         { return Keyboard::getChangedKey(); }
