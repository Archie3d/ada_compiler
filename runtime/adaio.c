/* File handling shared by Text_IO, Sequential_IO, Direct_IO and Stream_IO.

   An Ada File_Type is a one component record holding a pointer into the pool
   below, so the generated code only ever passes the address of that record and
   never needs to know what a file looks like. */

#include "adaio.h"
#include "adart.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ADA_MAX_FILES 32

static AdaFile filePool[ADA_MAX_FILES];
static AdaFile standardInput;
static AdaFile standardOutput;
static AdaFile standardError;
static int poolPrepared = 0;

/* Each of these is the single component of a File_Type record, which is why
   their addresses can be handed straight to the generated code. */
static AdaFile* standardInputHandle;
static AdaFile* standardOutputHandle;
static AdaFile* standardErrorHandle;
static AdaFile* currentInputHandle;
static AdaFile* currentOutputHandle;

static void closeEverything(void)
{
    int i;

    fflush(stdout);
    for (i = 0; i < ADA_MAX_FILES; ++i) {
        if (filePool[i].isOpen && filePool[i].stream != 0) {
            fclose(filePool[i].stream);
            filePool[i].isOpen = 0;
            filePool[i].stream = 0;
        }
    }
}

static void openStandard(AdaFile* file, FILE* stream, int mode)
{
    file->stream = stream;
    file->mode = mode;
    file->isOpen = 1;
    file->isStandard = 1;
    file->name[0] = '\0';
}

static void prepare(void)
{
    if (poolPrepared) {
        return;
    }
    poolPrepared = 1;

    openStandard(&standardInput, stdin, ADA_MODE_IN);
    openStandard(&standardOutput, stdout, ADA_MODE_OUT);
    openStandard(&standardError, stderr, ADA_MODE_OUT);

    standardInputHandle = &standardInput;
    standardOutputHandle = &standardOutput;
    standardErrorHandle = &standardError;
    currentInputHandle = &standardInput;
    currentOutputHandle = &standardOutput;

    atexit(closeEverything);
}

AdaFileRef __ada_standard_input(void)
{
    prepare();
    return &standardInputHandle;
}

AdaFileRef __ada_standard_output(void)
{
    prepare();
    return &standardOutputHandle;
}

AdaFileRef __ada_standard_error(void)
{
    prepare();
    return &standardErrorHandle;
}

AdaFileRef __ada_current_input(void)
{
    prepare();
    return &currentInputHandle;
}

AdaFileRef __ada_current_output(void)
{
    prepare();
    return &currentOutputHandle;
}

void __ada_set_input(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        currentInputHandle = file;
    }
}

void __ada_set_output(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file != 0) {
        currentOutputHandle = file;
    }
}

FILE* __ada_input_stream(void)
{
    prepare();
    return currentInputHandle != 0 ? currentInputHandle->stream : stdin;
}

FILE* __ada_output_stream(void)
{
    prepare();
    return currentOutputHandle != 0 ? currentOutputHandle->stream : stdout;
}

/* An unopened File_Type is a null handle, which every operation but Is_Open
   rejects with Status_Error. */
AdaFile* __ada_file_checked(AdaFileRef handle, int requiredMode)
{
    AdaFile* file = handle != 0 ? *handle : 0;

    prepare();
    if (file == 0 || !file->isOpen) {
        __ada_raise(ADA_STATUS_ERROR);
        return 0;
    }
    if (requiredMode == ADA_MODE_ANY) {
        return file;
    }
    if (requiredMode == ADA_MODE_IN) {
        if (file->mode != ADA_MODE_IN && file->mode != ADA_MODE_INOUT) {
            __ada_raise(ADA_MODE_ERROR);
            return 0;
        }
        return file;
    }
    if (file->mode == ADA_MODE_IN) {
        __ada_raise(ADA_MODE_ERROR);
        return 0;
    }
    return file;
}

static AdaFile* takeFreeEntry(void)
{
    int i;

    for (i = 0; i < ADA_MAX_FILES; ++i) {
        if (!filePool[i].isOpen) {
            return &filePool[i];
        }
    }
    __ada_raise(ADA_STORAGE_ERROR);
    return 0;
}

static const char* modeString(int mode, int create)
{
    /* Create always starts from an empty file, even for reading, so it asks
       for a stream that may be written whatever the Ada mode says.  The mode
       recorded against the file is what later rejects the wrong operation. */
    if (create) {
        return mode == ADA_MODE_APPEND ? "a+b" : "w+b";
    }
    switch (mode) {
    case ADA_MODE_IN:
        return "rb";
    case ADA_MODE_OUT:
        return "wb";
    case ADA_MODE_APPEND:
        return "ab";
    case ADA_MODE_INOUT:
        return "r+b";
    default:
        return "rb";
    }
}

/* Create truncates or makes the file, Open insists that it is already there.
   An empty name asks for a scratch file that lives only as long as it is
   open, which is what Ada does for an anonymous file. */
void __ada_file_open(AdaFileRef handle, int mode, const char* name, int length, int create)
{
    AdaFile* file;

    prepare();
    if (handle == 0) {
        __ada_raise(ADA_USE_ERROR);
        return;
    }
    if (*handle != 0 && (*handle)->isOpen) {
        __ada_raise(ADA_STATUS_ERROR);
        return;
    }
    if (length < 0) {
        length = 0;
    }
    if (length >= (int)sizeof filePool[0].name) {
        __ada_raise(ADA_NAME_ERROR);
        return;
    }

    file = takeFreeEntry();
    if (file == 0) {
        return;
    }

    if (length > 0) {
        memcpy(file->name, name, (size_t)length);
    }
    file->name[length] = '\0';
    file->mode = mode;
    file->isStandard = 0;

    if (length == 0) {
        file->stream = tmpfile();
    } else {
        file->stream = fopen(file->name, modeString(mode, create));
    }
    if (file->stream == 0) {
        /* Opening something that is not there is Name_Error; failing to make
           it is the environment refusing the request. */
        __ada_raise(create ? ADA_USE_ERROR : ADA_NAME_ERROR);
        return;
    }

    file->isOpen = 1;
    *handle = file;
}

void __ada_file_close(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    if (file == 0) {
        return;
    }
    if (file->isStandard) {
        __ada_raise(ADA_MODE_ERROR);
        return;
    }
    fclose(file->stream);
    file->stream = 0;
    file->isOpen = 0;
    *handle = 0;
}

void __ada_file_delete(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);
    char name[sizeof filePool[0].name];

    if (file == 0) {
        return;
    }
    if (file->isStandard) {
        __ada_raise(ADA_USE_ERROR);
        return;
    }
    strcpy(name, file->name);
    fclose(file->stream);
    file->stream = 0;
    file->isOpen = 0;
    *handle = 0;
    if (name[0] != '\0') {
        remove(name);
    }
}

/* Reset rewinds the file, optionally in a new mode.  A mode of ADA_MODE_ANY
   keeps the one the file already has. */
void __ada_file_reset(AdaFileRef handle, int mode)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    if (file == 0) {
        return;
    }
    if (file->isStandard) {
        __ada_raise(ADA_MODE_ERROR);
        return;
    }
    if (mode == ADA_MODE_ANY) {
        mode = file->mode;
    }
    fflush(file->stream);
    if (file->name[0] == '\0') {
        /* A scratch file has no name to reopen, so it is only rewound. */
        rewind(file->stream);
        file->mode = mode;
        return;
    }
    file->stream = freopen(file->name, modeString(mode, 0), file->stream);
    if (file->stream == 0) {
        file->isOpen = 0;
        *handle = 0;
        __ada_raise(ADA_USE_ERROR);
        return;
    }
    file->mode = mode;
}

/* Reset without a mode, which Ada declares as a profile of its own rather than
   as a default, keeps whatever mode the file was opened with. */
void __ada_file_reset_same(AdaFileRef handle)
{
    __ada_file_reset(handle, ADA_MODE_ANY);
}

int __ada_file_is_open(AdaFileRef handle)
{
    AdaFile* file = handle != 0 ? *handle : 0;

    prepare();
    return file != 0 && file->isOpen ? 1 : 0;
}

int __ada_file_mode(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    return file != 0 ? file->mode : ADA_MODE_IN;
}

const char* __ada_file_name(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    return file != 0 ? file->name : "";
}

/* Nothing in this run time reads the form string, so a file always reports
   the default one. */
const char* __ada_file_form(AdaFileRef handle)
{
    __ada_file_checked(handle, ADA_MODE_ANY);
    return "";
}

int __ada_file_end_of_file(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);
    int c;

    if (file == 0) {
        return 1;
    }
    c = fgetc(file->stream);
    if (c == EOF) {
        return 1;
    }
    ungetc(c, file->stream);
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Text_IO                                                                   */
/* ------------------------------------------------------------------------ */

void __ada_create(AdaFileRef handle, int mode, const char* name, int length, const char* form, int formLength)
{
    (void)form;
    (void)formLength;
    __ada_file_open(handle, mode, name, length, 1);
}

void __ada_open(AdaFileRef handle, int mode, const char* name, int length, const char* form, int formLength)
{
    (void)form;
    (void)formLength;
    __ada_file_open(handle, mode, name, length, 0);
}

static void writeInteger(FILE* stream, int value, int width, int base)
{
    static const char* const digits = "0123456789ABCDEF";
    char body[80];
    char text[96];
    int at = (int)sizeof body - 1;
    unsigned int magnitude;

    if (base < 2 || base > 16) {
        __ada_raise(ADA_CONSTRAINT_ERROR);
        return;
    }
    if (base == 10) {
        fprintf(stream, "%*d", width, value);
        return;
    }

    magnitude = value < 0 ? (unsigned int)(-(long)value) : (unsigned int)value;
    body[at] = '\0';
    do {
        body[--at] = digits[magnitude % (unsigned int)base];
        magnitude /= (unsigned int)base;
    } while (magnitude != 0);

    snprintf(text, sizeof text, "%s%d#%s#", value < 0 ? "-" : "", base, body + at);
    fprintf(stream, "%*s", width, text);
}

void __ada_text_put(AdaFileRef handle, const char* item, int length)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file != 0 && length > 0) {
        fwrite(item, 1, (size_t)length, file->stream);
    }
}

void __ada_text_put_line(AdaFileRef handle, const char* item, int length)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file == 0) {
        return;
    }
    if (length > 0) {
        fwrite(item, 1, (size_t)length, file->stream);
    }
    fputc('\n', file->stream);
}

void __ada_text_put_character(AdaFileRef handle, int item)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file != 0) {
        fputc(item, file->stream);
    }
}

void __ada_text_put_integer(AdaFileRef handle, int item, int width, int base)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file != 0) {
        writeInteger(file->stream, item, width, base);
    }
}

void __ada_text_put_float(AdaFileRef handle, double item, int fore, int aft, int exponent)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);
    char buffer[128];

    if (file != 0) {
        __ada_format_float(buffer, (int)sizeof buffer, item, fore, aft, exponent);
        fputs(buffer, file->stream);
    }
}

void __ada_text_new_line(AdaFileRef handle, int spacing)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);
    int i;

    if (file == 0) {
        return;
    }
    for (i = 0; i < spacing; ++i) {
        fputc('\n', file->stream);
    }
}

static void skipLines(AdaFile* file, int spacing)
{
    int i;
    int c;

    for (i = 0; i < spacing; ++i) {
        do {
            c = fgetc(file->stream);
        } while (c != '\n' && c != EOF);
        if (c == EOF) {
            __ada_raise(ADA_END_ERROR);
            return;
        }
    }
}

void __ada_text_skip_line(AdaFileRef handle, int spacing)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        skipLines(file, spacing);
    }
}

static int atEndOfLine(AdaFile* file)
{
    int c = fgetc(file->stream);

    if (c == EOF) {
        return 1;
    }
    ungetc(c, file->stream);
    return c == '\n' ? 1 : 0;
}

int __ada_text_end_of_line(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    return file != 0 ? atEndOfLine(file) : 1;
}

static void readCharacter(AdaFile* file, char* item)
{
    int c;

    /* Get skips over line boundaries, which is what Ada asks of it. */
    do {
        c = fgetc(file->stream);
    } while (c == '\n');
    if (c == EOF) {
        __ada_raise(ADA_END_ERROR);
        *item = ' ';
        return;
    }
    *item = (char)c;
}

void __ada_text_get(AdaFileRef handle, char* item)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        readCharacter(file, item);
    }
}

/* Get_Line fills as much of the string as the line holds and reports how far
   it got, then steps past the line terminator. */
static void readLine(AdaFile* file, char* item, int length, int* last)
{
    int count = 0;
    int c;

    for (;;) {
        c = fgetc(file->stream);
        if (c == EOF) {
            if (count == 0) {
                __ada_raise(ADA_END_ERROR);
            }
            break;
        }
        if (c == '\n') {
            break;
        }
        if (count >= length) {
            ungetc(c, file->stream);
            break;
        }
        item[count++] = (char)c;
    }
    *last = count;
}

void __ada_text_get_line(AdaFileRef handle, char* item, int length, int* last)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file == 0) {
        *last = 0;
        return;
    }
    readLine(file, item, length, last);
}

/* The width of the target is not visible from the profile, so it travels with
   the address. */
static void storeInteger(void* item, int size, long value)
{
    if (size == 1) {
        *(signed char*)item = (signed char)value;
    } else if (size == 2) {
        *(short*)item = (short)value;
    } else if (size == 8) {
        *(long long*)item = (long long)value;
    } else {
        *(int*)item = (int)value;
    }
}

static void readInteger(AdaFile* file, void* item, int size, int low, int high)
{
    long value = 0;

    if (fscanf(file->stream, "%ld", &value) != 1) {
        __ada_raise(feof(file->stream) ? ADA_END_ERROR : ADA_DATA_ERROR);
        storeInteger(item, size, 0);
        return;
    }
    /* Ada makes a number the instantiated type cannot hold a Data_Error rather
       than a Constraint_Error. */
    if (value < (long)low || value > (long)high) {
        __ada_raise(ADA_DATA_ERROR);
        storeInteger(item, size, 0);
        return;
    }
    storeInteger(item, size, value);
}

void __ada_text_get_integer(AdaFileRef handle, void* item, int size, int low, int high)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        readInteger(file, item, size, low, high);
    }
}

/* The word the input is looking at, read up to the first character that cannot
   be part of one.  Recognising what the word means is the business of the Ada
   generic that asked for it, which is the only side that knows the type. */
void __ada_text_get_word(AdaFileRef handle, char* item, int length, int* last)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);
    int written = 0;
    int c;

    *last = 0;
    if (file == 0) {
        return;
    }

    do {
        c = fgetc(file->stream);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');

    if (c == EOF) {
        __ada_raise(ADA_END_ERROR);
        return;
    }
    while (c != EOF && !isspace(c)) {
        if (written < length) {
            item[written++] = (char)c;
        }
        c = fgetc(file->stream);
    }
    if (c != EOF) {
        ungetc(c, file->stream);
    }
    *last = written;
}

void __ada_get_word(char* item, int length, int* last)
{
    __ada_text_get_word(__ada_current_input(), item, length, last);
}

/* Float_IO.Get reports input that is not a number as Data_Error, so that is
   what a word which spells no real number raises here. */
double __ada_real_value(const char* text, int length)
{
    char buffer[64];
    char* stop = 0;
    double value;

    if (length <= 0 || length >= (int)sizeof buffer) {
        __ada_raise(ADA_DATA_ERROR);
        return 0.0;
    }
    memcpy(buffer, text, (size_t)length);
    buffer[length] = '\0';

    value = strtod(buffer, &stop);
    if (stop == buffer || *stop != '\0') {
        __ada_raise(ADA_DATA_ERROR);
        return 0.0;
    }
    return value;
}

static void storeReal(void* item, int size, double value)
{
    if (size == 4) {
        *(float*)item = (float)value;
    } else {
        *(double*)item = value;
    }
}

static void readReal(AdaFile* file, void* item, int size)
{
    double value = 0.0;

    if (fscanf(file->stream, "%lf", &value) != 1) {
        __ada_raise(feof(file->stream) ? ADA_END_ERROR : ADA_DATA_ERROR);
        storeReal(item, size, 0.0);
        return;
    }
    storeReal(item, size, value);
}

void __ada_text_get_float(AdaFileRef handle, void* item, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        readReal(file, item, size);
    }
}

/* The compiler records the literals of an enumeration type in lower case, which
   is the spelling Lower_Case asks for; Upper_Case is the Ada default. */
void __ada_text_put_enum(AdaFileRef handle, int item, int width, int set, const char** names, int count)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);
    const char* name;
    int length;
    int i;

    if (file == 0) {
        return;
    }
    if (item < 0 || item >= count) {
        __ada_raise(ADA_DATA_ERROR);
        return;
    }

    name = names[item];
    for (i = 0; name[i] != '\0'; ++i) {
        fputc(set == 0 ? name[i] : toupper((unsigned char)name[i]), file->stream);
    }
    /* A field wider than the literal is filled out on the right. */
    for (length = i; length < width; ++length) {
        fputc(' ', file->stream);
    }
}

static void readEnum(AdaFile* file, void* item, int size, const char** names, int count)
{
    char word[128];
    int length = 0;
    int c;
    int i;

    do {
        c = fgetc(file->stream);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');

    if (c == EOF) {
        __ada_raise(ADA_END_ERROR);
        storeInteger(item, size, 0);
        return;
    }
    while (c != EOF && (isalnum(c) || c == '_')) {
        if (length < (int)sizeof word - 1) {
            word[length++] = (char)tolower(c);
        }
        c = fgetc(file->stream);
    }
    if (c != EOF) {
        ungetc(c, file->stream);
    }
    word[length] = '\0';

    for (i = 0; i < count; ++i) {
        if (strcmp(word, names[i]) == 0) {
            storeInteger(item, size, i);
            return;
        }
    }
    __ada_raise(ADA_DATA_ERROR);
    storeInteger(item, size, 0);
}

void __ada_text_get_enum(AdaFileRef handle, void* item, int size, const char** names, int count)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file != 0) {
        readEnum(file, item, size, names, count);
    }
}

/* ------------------------------------------------------------------------ */
/* Text_IO on the current input and output                                   */
/* ------------------------------------------------------------------------ */

void __ada_put(const char* item, int length)
{
    __ada_text_put(__ada_current_output(), item, length);
}

void __ada_put_line(const char* item, int length)
{
    __ada_text_put_line(__ada_current_output(), item, length);
}

void __ada_put_character(int item)
{
    __ada_text_put_character(__ada_current_output(), item);
}

void __ada_put_integer(int item, int width, int base)
{
    __ada_text_put_integer(__ada_current_output(), item, width, base);
}

void __ada_new_line(int spacing)
{
    __ada_text_new_line(__ada_current_output(), spacing);
}

void __ada_skip_line(int spacing)
{
    __ada_text_skip_line(__ada_current_input(), spacing);
}

int __ada_end_of_file(void)
{
    return __ada_file_end_of_file(__ada_current_input());
}

int __ada_end_of_line(void)
{
    return __ada_text_end_of_line(__ada_current_input());
}

void __ada_get(char* item)
{
    __ada_text_get(__ada_current_input(), item);
}

void __ada_get_line(char* item, int length, int* last)
{
    __ada_text_get_line(__ada_current_input(), item, length, last);
}

void __ada_get_integer(void* item, int size, int low, int high)
{
    __ada_text_get_integer(__ada_current_input(), item, size, low, high);
}

void __ada_get_float(void* item, int size)
{
    __ada_text_get_float(__ada_current_input(), item, size);
}

void __ada_put_enum(int item, int width, int set, const char** names, int count)
{
    __ada_text_put_enum(__ada_current_output(), item, width, set, names, count);
}

void __ada_get_enum(void* item, int size, const char** names, int count)
{
    __ada_text_get_enum(__ada_current_input(), item, size, names, count);
}

/* ------------------------------------------------------------------------ */
/* Sequential_IO                                                             */
/* ------------------------------------------------------------------------ */

void __ada_read_element(AdaFileRef handle, void* item, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file == 0) {
        return;
    }
    if (fread(item, 1, (size_t)size, file->stream) != (size_t)size) {
        __ada_raise(ADA_END_ERROR);
    }
}

void __ada_write_element(AdaFileRef handle, const void* item, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file == 0) {
        return;
    }
    if (fwrite(item, 1, (size_t)size, file->stream) != (size_t)size) {
        __ada_raise(ADA_DEVICE_ERROR);
    }
}

/* ------------------------------------------------------------------------ */
/* Direct_IO                                                                 */
/* ------------------------------------------------------------------------ */

/* Direct_IO writes its modes as In_File, Inout_File, Out_File. */
static int directMode(int mode)
{
    switch (mode) {
    case 0:
        return ADA_MODE_IN;
    case 1:
        return ADA_MODE_INOUT;
    case 2:
        return ADA_MODE_OUT;
    default:
        return ADA_MODE_ANY;
    }
}

static int adaDirectMode(int mode)
{
    switch (mode) {
    case ADA_MODE_IN:
        return 0;
    case ADA_MODE_INOUT:
        return 1;
    default:
        return 2;
    }
}

void __ada_direct_create(AdaFileRef handle, int mode, const char* name, int length, const char* form,
                         int formLength)
{
    (void)form;
    (void)formLength;
    __ada_file_open(handle, directMode(mode), name, length, 1);
}

void __ada_direct_open(AdaFileRef handle, int mode, const char* name, int length, const char* form, int formLength)
{
    (void)form;
    (void)formLength;
    __ada_file_open(handle, directMode(mode), name, length, 0);
}

void __ada_direct_reset(AdaFileRef handle, int mode)
{
    __ada_file_reset(handle, mode < 0 ? ADA_MODE_ANY : directMode(mode));
}

void __ada_direct_reset_same(AdaFileRef handle)
{
    __ada_file_reset_same(handle);
}

int __ada_direct_mode(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    return file != 0 ? adaDirectMode(file->mode) : 0;
}

/* Ada counts elements from one, so an index becomes a byte offset here. */
static long offsetOf(int index, int size)
{
    return (long)(index - 1) * (long)size;
}

void __ada_direct_read_at(AdaFileRef handle, void* item, int size, int index)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);

    if (file == 0) {
        return;
    }
    if (index > 0 && fseek(file->stream, offsetOf(index, size), SEEK_SET) != 0) {
        __ada_raise(ADA_END_ERROR);
        return;
    }
    if (fread(item, 1, (size_t)size, file->stream) != (size_t)size) {
        __ada_raise(ADA_END_ERROR);
    }
}

void __ada_direct_write_at(AdaFileRef handle, const void* item, int size, int index)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file == 0) {
        return;
    }
    if (index > 0 && fseek(file->stream, offsetOf(index, size), SEEK_SET) != 0) {
        __ada_raise(ADA_USE_ERROR);
        return;
    }
    if (fwrite(item, 1, (size_t)size, file->stream) != (size_t)size) {
        __ada_raise(ADA_DEVICE_ERROR);
    }
}

void __ada_direct_set_index(AdaFileRef handle, int index, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    if (file == 0) {
        return;
    }
    if (fseek(file->stream, offsetOf(index, size), SEEK_SET) != 0) {
        __ada_raise(ADA_USE_ERROR);
    }
}

int __ada_direct_index(AdaFileRef handle, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);
    long position;

    if (file == 0) {
        return 1;
    }
    position = ftell(file->stream);
    if (position < 0) {
        __ada_raise(ADA_USE_ERROR);
        return 1;
    }
    return (int)(position / size) + 1;
}

int __ada_direct_size(AdaFileRef handle, int size)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);
    long here;
    long end;

    if (file == 0) {
        return 0;
    }
    here = ftell(file->stream);
    if (here < 0 || fseek(file->stream, 0, SEEK_END) != 0) {
        __ada_raise(ADA_USE_ERROR);
        return 0;
    }
    end = ftell(file->stream);
    fseek(file->stream, here, SEEK_SET);
    return end < 0 ? 0 : (int)(end / size);
}

/* ------------------------------------------------------------------------ */
/* Stream_IO                                                                 */
/* ------------------------------------------------------------------------ */

#define ADA_STREAM_BUFFERS 4
#define ADA_STREAM_BUFFER_SIZE 4096

AdaFile* __ada_stream_of(AdaFileRef handle)
{
    return __ada_file_checked(handle, ADA_MODE_ANY);
}

void __ada_stream_read(AdaFile* stream, void* item, int size)
{
    if (stream == 0 || !stream->isOpen) {
        __ada_raise(ADA_STATUS_ERROR);
        return;
    }
    if (stream->mode == ADA_MODE_OUT || stream->mode == ADA_MODE_APPEND) {
        __ada_raise(ADA_MODE_ERROR);
        return;
    }
    if (fread(item, 1, (size_t)size, stream->stream) != (size_t)size) {
        __ada_raise(ADA_END_ERROR);
    }
}

void __ada_stream_write(AdaFile* stream, const void* item, int size)
{
    if (stream == 0 || !stream->isOpen) {
        __ada_raise(ADA_STATUS_ERROR);
        return;
    }
    if (stream->mode == ADA_MODE_IN) {
        __ada_raise(ADA_MODE_ERROR);
        return;
    }
    if (fwrite(item, 1, (size_t)size, stream->stream) != (size_t)size) {
        __ada_raise(ADA_DEVICE_ERROR);
    }
}

void __ada_stream_write_bounds(AdaFile* stream, int first, int last)
{
    __ada_stream_write(stream, &first, (int)sizeof first);
    __ada_stream_write(stream, &last, (int)sizeof last);
}

/* The value 'Input yields has nowhere to live in the compiled code, so the
   buffers here hold it.  They rotate so that a few reads can be combined in
   one expression. */
void* __ada_stream_read_array(AdaFile* stream, int elementSize, int* first, int* last)
{
    static char buffers[ADA_STREAM_BUFFERS][ADA_STREAM_BUFFER_SIZE];
    static int next = 0;

    char* buffer = buffers[next];
    long count;

    next = (next + 1) % ADA_STREAM_BUFFERS;
    *first = 1;
    *last = 0;

    __ada_stream_read(stream, first, (int)sizeof *first);
    __ada_stream_read(stream, last, (int)sizeof *last);
    if (__ada_exception != 0) {
        return buffer;
    }

    count = (long)*last - (long)*first + 1;
    if (count < 0) {
        count = 0;
    }
    if (count * elementSize > ADA_STREAM_BUFFER_SIZE) {
        __ada_raise(ADA_STORAGE_ERROR);
        *last = *first - 1;
        return buffer;
    }
    if (count > 0) {
        __ada_stream_read(stream, buffer, (int)(count * elementSize));
    }
    return buffer;
}

/* Read and Write over a Stream_Element_Array work in bytes, since a stream
   element is one byte wide. */
void __ada_stream_elements_read(AdaFileRef handle, void* item, int length, int first, int* last)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_IN);
    size_t got;

    if (file == 0) {
        *last = 0;
        return;
    }
    got = fread(item, 1, (size_t)length, file->stream);
    *last = (int)((long long)first + (long long)got - 1);
    if (ferror(file->stream)) {
        __ada_raise(ADA_DEVICE_ERROR);
    }
}

void __ada_stream_elements_write(AdaFileRef handle, const void* item, int length)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_OUT);

    if (file == 0) {
        return;
    }
    if (fwrite(item, 1, (size_t)length, file->stream) != (size_t)length) {
        __ada_raise(ADA_DEVICE_ERROR);
    }
}

void __ada_stream_set_index(AdaFileRef handle, int index)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);

    if (file != 0 && fseek(file->stream, (long)index - 1, SEEK_SET) != 0) {
        __ada_raise(ADA_USE_ERROR);
    }
}

int __ada_stream_index(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);
    long position;

    if (file == 0) {
        return 1;
    }
    position = ftell(file->stream);
    return position < 0 ? 1 : (int)position + 1;
}

int __ada_stream_size(AdaFileRef handle)
{
    AdaFile* file = __ada_file_checked(handle, ADA_MODE_ANY);
    long here;
    long end;

    if (file == 0) {
        return 0;
    }
    here = ftell(file->stream);
    if (here < 0 || fseek(file->stream, 0, SEEK_END) != 0) {
        __ada_raise(ADA_USE_ERROR);
        return 0;
    }
    end = ftell(file->stream);
    fseek(file->stream, here, SEEK_SET);
    return end < 0 ? 0 : (int)end;
}
