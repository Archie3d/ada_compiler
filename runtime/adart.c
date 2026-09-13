/* Minimal run time support for programs compiled by adac. */

#include "adart.h"

#include "adaio.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_BUFFERS 8
#define IMAGE_BUFFER_SIZE 64

/* Indexed by AdaExceptionId; the compiler writes the same spellings when it
   raises an exception itself. */
static const char* const exceptionNames[] = {
    "EXCEPTION",     "CONSTRAINT_ERROR", "PROGRAM_ERROR", "STORAGE_ERROR", "NUMERIC_ERROR",
    "TASKING_ERROR", "STATUS_ERROR",     "MODE_ERROR",    "NAME_ERROR",    "USE_ERROR",
    "DEVICE_ERROR",  "END_ERROR",        "DATA_ERROR",    "LAYOUT_ERROR"
};

void __ada_raise(int id)
{
    int count = (int)(sizeof exceptionNames / sizeof exceptionNames[0]);

    if (id < 1 || id >= count) {
        id = ADA_PROGRAM_ERROR;
    }
    __ada_exception = id;
    __ada_exception_name = exceptionNames[id];
}

void* __ada_allocate(long size)
{
    void* address;

    if (size <= 0) {
        size = 1;
    }
    address = calloc(1, (size_t)size);
    if (address == NULL) {
        __ada_raise(ADA_STORAGE_ERROR);
    }
    return address;
}

void __ada_deallocate(void* address)
{
    free(address);
}

/* Integer'Image renders a leading space in front of non-negative values.  The
   buffers rotate so that a few images can be combined in one expression. */
const char* __ada_image_integer(int value)
{
    static char buffers[IMAGE_BUFFERS][IMAGE_BUFFER_SIZE];
    static int next = 0;

    char* buffer = buffers[next];
    next = (next + 1) % IMAGE_BUFFERS;

    if (value < 0) {
        snprintf(buffer, IMAGE_BUFFER_SIZE, "%d", value);
    } else {
        snprintf(buffer, IMAGE_BUFFER_SIZE, " %d", value);
    }
    return buffer;
}

/* The image of an enumeration value is its literal in upper case, and the image
   of a character is the literal between its quotes. */
const char* __ada_image_enum(int value, const char** names, int count)
{
    static char buffers[IMAGE_BUFFERS][IMAGE_BUFFER_SIZE];
    static int next = 0;

    char* buffer = buffers[next];
    const char* name;
    int i;

    next = (next + 1) % IMAGE_BUFFERS;

    if (value < 0 || value >= count) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        buffer[0] = '\0';
        return buffer;
    }

    name = names[value];
    for (i = 0; name[i] != '\0' && i < IMAGE_BUFFER_SIZE - 1; ++i) {
        buffer[i] = (char)toupper((unsigned char)name[i]);
    }
    buffer[i] = '\0';
    return buffer;
}

const char* __ada_image_character(int value)
{
    static char buffers[IMAGE_BUFFERS][IMAGE_BUFFER_SIZE];
    static int next = 0;

    char* buffer = buffers[next];
    next = (next + 1) % IMAGE_BUFFERS;

    snprintf(buffer, IMAGE_BUFFER_SIZE, "'%c'", value);
    return buffer;
}

/* 'Value ignores the blanks around what it is given, so both ends are trimmed
   before anything is made of the rest. */
static void trim(const char* text, int length, int* from, int* to)
{
    int start = 0;
    int stop = length;

    while (start < stop && isspace((unsigned char)text[start])) {
        ++start;
    }
    while (stop > start && isspace((unsigned char)text[stop - 1])) {
        --stop;
    }
    *from = start;
    *to = stop;
}

long long __ada_value_long_integer(const char* text, int length, long long low, long long high)
{
    int start;
    int stop;
    int negative = 0;
    unsigned long long value = 0;
    unsigned long long limit;
    long long result;
    int i;

    trim(text, length, &start, &stop);
    if (start < stop && (text[start] == '+' || text[start] == '-')) {
        negative = text[start] == '-';
        ++start;
    }
    limit = (unsigned long long)LLONG_MAX + negative;
    if (start == stop) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return low;
    }
    for (i = start; i < stop; ++i) {
        unsigned digit = (unsigned)(text[i] - '0');
        if (digit > 9 || value > (limit - digit) / 10) {
            __ada_raise(ADA_CONSTRAINT_ERROR);
            return low;
        }
        value = value * 10 + digit;
    }
    result = negative ? (value == (unsigned long long)LLONG_MAX + 1 ? LLONG_MIN : -(long long)value)
                      : (long long)value;
    if (result < low || result > high) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return low;
    }
    return result;
}

int __ada_value_integer(const char* text, int length, int low, int high)
{
    return (int)__ada_value_long_integer(text, length, low, high);
}

const char* __ada_image_long_integer(long long value)
{
    static char buffers[IMAGE_BUFFERS][IMAGE_BUFFER_SIZE];
    static int next = 0;
    char* buffer = buffers[next];
    next = (next + 1) % IMAGE_BUFFERS;
    snprintf(buffer, IMAGE_BUFFER_SIZE, value < 0 ? "%lld" : " %lld", value);
    return buffer;
}

long long __ada_integer_operation(int operation, int bits, long long left, long long right)
{
    long long result = 0;
    long long low = bits == 32 ? INT_MIN : LLONG_MIN;
    long long high = bits == 32 ? INT_MAX : LLONG_MAX;
    int overflow = 0;

    switch (operation) {
    case 0:
        overflow = __builtin_add_overflow(left, right, &result);
        break;
    case 1:
        overflow = __builtin_sub_overflow(left, right, &result);
        break;
    case 2:
        overflow = __builtin_mul_overflow(left, right, &result);
        break;
    case 3:
    case 4:
    case 5:
        if (right == 0) {
            overflow = 1;
        } else if (left == LLONG_MIN && right == -1) {
            overflow = operation == 3;
        } else if (operation == 3) {
            result = left / right;
        } else {
            result = left % right;
            if (operation == 5 && result != 0 && (result < 0) != (right < 0)) {
                result += right;
            }
        }
        break;
    case 6:
        if (right < 0) {
            overflow = 1;
            break;
        }
        result = 1;
        while (right != 0) {
            if ((right & 1) && (__builtin_mul_overflow(result, left, &result)
                               || result < low || result > high)) {
                overflow = 1;
                break;
            }
            right >>= 1;
            if (right != 0 && (__builtin_mul_overflow(left, left, &left)
                              || left < low || left > high)) {
                overflow = 1;
                break;
            }
        }
        break;
    default:
        overflow = 1;
        break;
    }
    if (overflow || result < low || result > high) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return 0;
    }
    return result;
}

/* An enumeration literal is named without regard to case, which is why the
   comparison here folds both sides rather than the text alone. */
int __ada_value_enum(const char* text, int length, const char** names, int count)
{
    int start;
    int stop;
    int i;

    trim(text, length, &start, &stop);
    for (i = 0; i < count; ++i) {
        const char* name = names[i];
        int j;

        for (j = 0; j < stop - start && name[j] != '\0'; ++j) {
            if (tolower((unsigned char)text[start + j]) != tolower((unsigned char)name[j])) {
                break;
            }
        }
        if (j == stop - start && name[j] == '\0') {
            return i;
        }
    }

    __ada_raise(ADA_CONSTRAINT_ERROR);
    return 0;
}

/* A character is named by its spelling, so its value is written between quotes
   the way __ada_image_character writes it. */
int __ada_value_character(const char* text, int length)
{
    int start;
    int stop;

    trim(text, length, &start, &stop);
    if (stop - start != 3 || text[start] != '\'' || text[stop - 1] != '\'') {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return 0;
    }
    return (unsigned char)text[start + 1];
}

/* Arrays compare element by element; a prefix is smaller than what extends it. */
int __ada_string_compare(const char* left, int leftLength, const char* right, int rightLength)
{
    int shorter = leftLength < rightLength ? leftLength : rightLength;
    int i;

    for (i = 0; i < shorter; ++i) {
        unsigned char leftCharacter = (unsigned char)left[i];
        unsigned char rightCharacter = (unsigned char)right[i];
        if (leftCharacter != rightCharacter) {
            return leftCharacter < rightCharacter ? -1 : 1;
        }
    }
    if (leftLength == rightLength) {
        return 0;
    }
    return leftLength < rightLength ? -1 : 1;
}

/* Renders a real value the way Ada.Text_IO does.  Fore is the least number of
   characters before the point including the sign, Aft the number after it, and
   Exp the width of the exponent field counting its sign.  An Exp of zero asks
   for plain decimal notation. */
void __ada_format_float(char* buffer, int size, double value, int fore, int aft, int exponent)
{
    char digits[64];
    char sign;
    int power;
    int written;
    int i;

    if (fore < 1) {
        fore = 1;
    }
    if (aft < 1) {
        aft = 1;
    }

    if (exponent <= 0) {
        snprintf(buffer, size, "%*.*f", fore + 1 + aft, aft, value);
        return;
    }

    sign = value < 0.0 ? '-' : ' ';
    snprintf(digits, sizeof digits, "%.*e", aft, value < 0.0 ? -value : value);

    /* Split the mantissa from the exponent that printf produced. */
    {
        char* marker = strchr(digits, 'e');
        power = marker != 0 ? (int)strtol(marker + 1, 0, 10) : 0;
        if (marker != 0) {
            *marker = '\0';
        }
    }

    /* Ada counts the sign as part of the exponent field. */
    written = snprintf(buffer, size, "%*c%sE%c", fore - 1, sign, digits, power < 0 ? '-' : '+');
    if (written < 0 || written >= size) {
        return;
    }
    power = power < 0 ? -power : power;
    snprintf(buffer + written, size - written, "%0*d", exponent - 1, power);
}

void __ada_put_float(double value, int fore, int aft, int exponent)
{
    char buffer[128];

    __ada_format_float(buffer, (int)sizeof buffer, value, fore, aft, exponent);
    fputs(buffer, __ada_output_stream());
}

const char* __ada_image_float(double value, int aft, int exponent)
{
    static char buffers[IMAGE_BUFFERS][IMAGE_BUFFER_SIZE];
    static int next = 0;

    char* buffer = buffers[next];
    next = (next + 1) % IMAGE_BUFFERS;

    __ada_format_float(buffer, IMAGE_BUFFER_SIZE, value, 2, aft, exponent);
    return buffer;
}

/* Ada rounds away from zero when a real value becomes an integer, while a cast
   would drop the fraction. */
long long __ada_round_to_integer(double value)
{
    long long result;
    double fraction;
    /* The upper endpoint is exclusive: LLONG_MAX rounds to 2**63 as a double.
       This comparison also rejects NaNs before the C floating-to-integer cast. */
    if (!(value >= -9223372036854775808.0 && value < 9223372036854775808.0)) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return 0;
    }
    result = (long long)value;
    fraction = value - (double)result;
    if (fraction >= 0.5) {
        ++result;
    } else if (fraction <= -0.5) {
        --result;
    }
    return result;
}

void __ada_unhandled(const char* name)
{
    fflush(stdout);
    fprintf(stderr, "\nraised %s\n", name == 0 ? "EXCEPTION" : name);
}

/* Internal Ada ABI: an unconstrained result carries a transfer buffer, its
   first dimension's bounds, and the buffer size. The emitter appends any
   further dimension bounds after this 24-byte header. The caller adopts the
   buffer into its temporary allocation list; no callee stack pointer escapes. */
void __ada_array_result(void* descriptor, const void* source, int first, int last, int64_t elementSize)
{
    int64_t length = last < first ? 0 : (int64_t)last - (int64_t)first + 1;
    int64_t size;
    void* buffer;

    if (elementSize < 0 || (elementSize != 0 && length > INT64_MAX / elementSize)) {
        __ada_raise(ADA_STORAGE_ERROR);
        return;
    }
    size = length * elementSize;
    if ((uint64_t)size > SIZE_MAX) {
        __ada_raise(ADA_STORAGE_ERROR);
        return;
    }
    buffer = calloc(1, size == 0 ? 1 : (size_t)size);
    if (buffer == NULL) {
        __ada_raise(ADA_STORAGE_ERROR);
        return;
    }
    if (size != 0) {
        memcpy(buffer, source, (size_t)size);
    }
    memcpy(descriptor, &buffer, sizeof buffer);
    memcpy((char*)descriptor + 8, &first, sizeof first);
    memcpy((char*)descriptor + 12, &last, sizeof last);
    /* alloc8 requires storage even for a null result. */
    if (size == 0) {
        size = 1;
    }
    memcpy((char*)descriptor + 16, &size, sizeof size);
}

/* Each activation has separate local-object and temporary allocation lists.
   Checkpoints release a suffix on scope/statement exit; returns release all. */
typedef struct AdaArrayAllocation
{
    struct AdaArrayAllocation* next;
    void* data;
} AdaArrayAllocation;

/* Checked row strides for multidimensional arrays. Descriptor lengths and
   array loops currently use signed 32-bit counts. */
int64_t __ada_array_size(int first, int last, int64_t elementSize)
{
    int64_t length = last < first ? 0 : (int64_t)last - first + 1;
    if (length > INT_MAX || elementSize < 0
        || (elementSize != 0 && length > INT64_MAX / elementSize)) {
        __ada_raise(ADA_STORAGE_ERROR);
        return 0;
    }
    return length * elementSize;
}

void* __ada_array_local(void** owner, int first, int last, int64_t elementSize)
{
    int64_t length = last < first ? 0 : (int64_t)last - first + 1;
    AdaArrayAllocation* allocation;
    size_t size;
    if (length > INT_MAX || elementSize < 0
        || (elementSize != 0 && (uint64_t)length > SIZE_MAX / (uint64_t)elementSize)) {
        __ada_raise(ADA_STORAGE_ERROR);
        return NULL;
    }
    size = (size_t)length * (size_t)elementSize;
    allocation = malloc(sizeof *allocation);
    if (allocation == NULL) {
        __ada_raise(ADA_STORAGE_ERROR);
        return NULL;
    }
    allocation->data = calloc(1, size == 0 ? 1 : size);
    if (allocation->data == NULL) {
        free(allocation);
        __ada_raise(ADA_STORAGE_ERROR);
        return NULL;
    }
    allocation->next = *owner;
    *owner = allocation;
    return allocation->data;
}

void __ada_array_rewind(void** owner, void* checkpoint)
{
    AdaArrayAllocation* allocation = *owner;
    while (allocation != checkpoint) {
        AdaArrayAllocation* next = allocation->next;
        free(allocation->data);
        free(allocation);
        allocation = next;
    }
    *owner = checkpoint;
}

void __ada_array_release(void** owner)
{
    __ada_array_rewind(owner, NULL);
}

void __ada_array_adopt(void** owner, void* data)
{
    AdaArrayAllocation* allocation = malloc(sizeof *allocation);
    if (allocation == NULL) {
        free(data);
        __ada_raise(ADA_STORAGE_ERROR);
        return;
    }
    allocation->data = data;
    allocation->next = *owner;
    *owner = allocation;
}
