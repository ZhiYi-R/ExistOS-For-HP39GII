/**
 * @file Bootloader/drivers/input/stmp_gpio.cpp
 * @brief GPIO keyboard-matrix driver — pure-static @c Keyboard singleton.
 *
 * The input driver is the @c Keyboard pure-static singleton (declared in
 * stmp_gpio.hpp). Its four operations are ordinary out-of-line static methods,
 * called directly by their C++ consumers as @c Keyboard::gpioInit / @c scan /
 * @c isKeyDown / @c getChangedKey. The scan matrices and the pending-change
 * latch are @c private @c static state, grouped into one @c State aggregate so
 * -Os keeps the original shared-base @c .bss addressing.
 *
 * @c set_row_line / @c read_col_line stay @c private @c always_inline helpers
 * with a single (loop-body) call site, folded into the scan path exactly as the
 * original file-scope @c static helpers were.
 *
 * The keyboard is polled from @c key_task, so it owns no ISR and there is no
 * @c extern @c "C" seam: the legacy @c portKeyboard* / @c portKey* shims are gone.
 */


#include "stmp_gpio.hpp"

#include "reg_model.hpp"

#include "debug.h"


void Keyboard::gpioInit()
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

Keys_t Keyboard::getChangedKey()
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

bool Keyboard::isKeyDown(Keys_t key)
{
    return st.key_matrix[key % 8][key >> 3] != 0u;
}

void Keyboard::scan()
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
