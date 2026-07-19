#include "core/string.h"

#include "core/calc.h"

#include <ctype.h>
#include <stddef.h>

int string_equals(const uint8_t *a, const uint8_t *b)
{
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
    }
    if (*a == 0 && *b == 0) {
        return 1;
    } else {
        return 0;
    }
}

int string_equals_until(const uint8_t *a, const uint8_t *b, size_t limit)
{
    if (!limit) {
        return 1;
    }
    size_t cursor = 0;
    while (*a && *b && *a == *b) {
        ++a;
        ++b;
        cursor++;
        if (cursor == limit) {
            return 1;
        }
    }
    if (*a == 0 && *b == 0) {
        return 1;
    } else {
        return 0;
    }
}

uint8_t *string_copy(const uint8_t *src, uint8_t *dst, size_t maxlength)
{
    size_t length = 0;
    while (length < maxlength && *src) {
        *dst = *src;
        src++;
        dst++;
        length++;
    }
    if (length == maxlength) {
        dst--;
    }
    *dst = 0;
    return dst;
}

int string_length(const uint8_t *str)
{
    if (!str) {
        return 0;
    }

    int length = 0;
    while (*str) {
        length++;
        str++;
    }
    return length;
}

const uint8_t *string_from_ascii(const char *str)
{
    const char *s = str;
    while (*s) {
        if (*s & 0x80) {
            return 0;
        }
        s++;
    }
    return (const uint8_t *) str;
}

int string_to_int(const uint8_t *str)
{
    int negative = 0;
    if (*str == '-') {
        negative = 1;
        ++str;
    }

    int result = 0;
    while (*str >= '0' && *str <= '9') {
        if (result > 0) {
            result *= 10;
        }
        result += *(str++) - '0';
    }

    if (negative) {
        return -result;
    }
    return result;
}

int string_from_int(uint8_t *dst, int value, int force_plus_sign)
{
    int total_chars = 0;

    if (value < 0) {
        dst[total_chars++] = '-';
        value = -value;
    } else if (force_plus_sign) {
        dst[total_chars++] = '+';
    }

    if (value == 0) {
        dst[total_chars++] = '0';
        dst[total_chars] = 0;
        return total_chars;
    }

    uint8_t *nums = &dst[total_chars];
    int total_nums = 0;

    while (value > 0) {
        nums[total_nums++] = (uint8_t) (value % 10 + '0');
        value /= 10;
        total_chars++;
    }

    // Reverse string
    total_nums >>= 1;
    while (total_nums > 0) {
        char tmp = nums[total_nums - 1];
        nums[total_nums - 1] = dst[total_chars - total_nums];
        dst[total_chars - total_nums] = tmp;
        total_nums--;
    }

    dst[total_chars] = 0;

    return total_chars;
}

int string_from_float(uint8_t *dst, float value, int decimal_places, int force_plus_sign)
{
    int total_chars = 0;

    if (decimal_places < 0) {
        decimal_places = 0;
    } else if (decimal_places > 5) {
        decimal_places = 5; // why would we ever need more than 5 decimal places? This ain't nuclear physics
    }

    if (value < 0.0f) {
        dst[total_chars++] = '-';
        value = -value;
    } else if (force_plus_sign) {
        dst[total_chars++] = '+';
    }

    int scale = 1;
    for (int i = 0; i < decimal_places; i++) {
        scale *= 10;
    }

    // Round once at full precision so carry into whole part is handled.
    int64_t scaled = (int64_t) (value * (float) scale + 0.5f);
    int int_part = (int) (scaled / scale);
    int fractional_digits = (int) (scaled % scale);

    total_chars += string_from_int(&dst[total_chars], int_part, 0);

    if (decimal_places > 0) {
        dst[total_chars++] = '.';

        // Emit exactly decimal_places digits, including leading zeros.
        int divisor = scale / 10;
        while (divisor > 0) {
            dst[total_chars++] = (uint8_t) ('0' + ((fractional_digits / divisor) % 10));
            divisor /= 10;
        }
    }

    dst[total_chars] = 0;
    return total_chars;
}

int string_compare(const uint8_t *a, const uint8_t *b)
{
    while (*a && *b && tolower(*a) == tolower(*b)) {
        ++a;
        ++b;
    }
    return tolower(*a) - tolower(*b);
}
