/* Shared declarations for the run time library linked into every compiled
   program. */

#ifndef ADART_H
#define ADART_H

#include <stdint.h>

/* Exception identifiers.  Sema registers the predefined exceptions in this
   order, so the run time can raise one of them by number and the handlers the
   compiler emitted will recognise it. */
enum AdaExceptionId
{
    ADA_CONSTRAINT_ERROR = 1,
    ADA_PROGRAM_ERROR = 2,
    ADA_STORAGE_ERROR = 3,
    ADA_NUMERIC_ERROR = 4,
    ADA_TASKING_ERROR = 5,
    ADA_STATUS_ERROR = 6,
    ADA_MODE_ERROR = 7,
    ADA_NAME_ERROR = 8,
    ADA_USE_ERROR = 9,
    ADA_DEVICE_ERROR = 10,
    ADA_END_ERROR = 11,
    ADA_DATA_ERROR = 12,
    ADA_LAYOUT_ERROR = 13
};

/* Defined by the generated code.  Writing them is how the run time hands a
   failure back to the Ada program. */
extern int __ada_exception;
extern const char* __ada_exception_name;

/* Marks an exception as pending.  The generated code inspects the globals
   after every call and jumps to the applicable handler. */
void __ada_raise(int id);

/* The storage an allocator takes from and Ada.Unchecked_Deallocation gives
   back.  Every allocation comes out cleared, so that an access component of
   the new object starts as null.  Storage_Error is raised when the request
   cannot be met, and freeing null does nothing. */
void* __ada_allocate(long size);
void __ada_deallocate(void* address);

/* Internal unconstrained-array return descriptor: pointer, two 32-bit bounds,
   and a 64-bit transfer size. The caller owns and releases the buffer. */
void __ada_array_result(void* descriptor, const void* source, int first, int last, int64_t elementSize);

/* Renders a real value the way Ada.Text_IO does.  Fore is the least number of
   characters before the point including the sign, Aft the number after it, and
   Exp the width of the exponent field counting its sign.  An Exp of zero asks
   for plain decimal notation. */
void __ada_format_float(char* buffer, int size, double value, int fore, int aft, int exponent);

/* 'Image, whose enumeration form needs the literal names the emitter records
   for the type. */
const char* __ada_image_integer(int value);
const char* __ada_image_long_integer(long long value);
const char* __ada_image_enum(int value, const char** names, int count);
const char* __ada_image_character(int value);

/* 'Value, which reads back what 'Image wrote.  Surrounding blanks are ignored,
   and anything else raises Constraint_Error. */
int __ada_value_integer(const char* text, int length, int low, int high);
long long __ada_value_long_integer(const char* text, int length, long long low, long long high);

/* Checked signed arithmetic: add, subtract, multiply, divide, rem, mod, power. */
long long __ada_integer_operation(int operation, int bits, long long left, long long right);
int __ada_value_enum(const char* text, int length, const char** names, int count);
int __ada_value_character(const char* text, int length);

#endif
