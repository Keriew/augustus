#include "core/string.h"

#include "core/calc.h"

#include <ctype.h>

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

int string_equals_until(const uint8_t *a, const uint8_t *b, unsigned int limit)
{
    if (!limit) {
        return 1;
    }
    unsigned int cursor = 0;
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

uint8_t *string_copy(const uint8_t *src, uint8_t *dst, int maxlength)
{
    int length = 0;
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

int string_compare(const uint8_t *a, const uint8_t *b)
{
    while (*a && *b && tolower(*a) == tolower(*b)) {
        ++a;
        ++b;
    }
    return tolower(*a) - tolower(*b);
}
