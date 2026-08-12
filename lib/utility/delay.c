#include "utility/delay.h"

void util_delay_ms(uint32_t ms) {
    p_hal->delay_ms(ms);
}