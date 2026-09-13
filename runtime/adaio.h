/* The file layer shared by the Ada input output packages. */

#ifndef ADAIO_H
#define ADAIO_H

#include <stdio.h>

/* Modes as the run time understands them.  Each package numbers its own
   File_Mode enumeration differently, so its entry points translate before
   calling in here. */
#define ADA_MODE_IN 0
#define ADA_MODE_OUT 1
#define ADA_MODE_APPEND 2
#define ADA_MODE_INOUT 3
#define ADA_MODE_ANY (-1)

typedef struct AdaFile
{
    FILE* stream;
    int mode;
    int isOpen;
    int isStandard; /* The standard files are never closed or deleted. */
    char name[1024];
} AdaFile;

/* An Ada File_Type is a one component record holding one of these pointers, so
   every entry point receives the address of that record. */
typedef AdaFile** AdaFileRef;

/* Returns the file behind a File_Type, or null after raising Status_Error for
   a file that is not open and Mode_Error for one open the wrong way round. */
AdaFile* __ada_file_checked(AdaFileRef handle, int requiredMode);

void __ada_file_open(AdaFileRef handle, int mode, const char* name, int length, int create);
void __ada_file_close(AdaFileRef handle);
void __ada_file_delete(AdaFileRef handle);
/* One whitespace delimited word, which is as much as the run time can make of
   input without knowing the Ada type it is being read into. */
void __ada_text_get_word(AdaFileRef handle, char* item, int length, int* last);
void __ada_get_word(char* item, int length, int* last);
double __ada_real_value(const char* text, int length);

void __ada_file_reset(AdaFileRef handle, int mode);
void __ada_file_reset_same(AdaFileRef handle);
int __ada_file_is_open(AdaFileRef handle);
int __ada_file_mode(AdaFileRef handle);
const char* __ada_file_name(AdaFileRef handle);
const char* __ada_file_form(AdaFileRef handle);
int __ada_file_end_of_file(AdaFileRef handle);

AdaFileRef __ada_standard_input(void);
AdaFileRef __ada_standard_output(void);
AdaFileRef __ada_standard_error(void);
AdaFileRef __ada_current_input(void);
AdaFileRef __ada_current_output(void);
void __ada_set_input(AdaFileRef handle);
void __ada_set_output(AdaFileRef handle);

/* Where the parameterless Text_IO profiles read and write. */
FILE* __ada_input_stream(void);
FILE* __ada_output_stream(void);

/* Opening a named file.  Text_IO, Sequential_IO and Stream_IO all number their
   modes the way the run time does and share these.  The form string is
   accepted and ignored, since there are no implementation defined options to
   select. */
void __ada_create(AdaFileRef handle, int mode, const char* name, int length, const char* form, int formLength);
void __ada_open(AdaFileRef handle, int mode, const char* name, int length, const char* form, int formLength);

/* Sequential_IO reads and writes whole objects at the current position. */
void __ada_read_element(AdaFileRef handle, void* item, int size);
void __ada_write_element(AdaFileRef handle, const void* item, int size);

/* Direct_IO numbers its modes In_File, Inout_File, Out_File, so it translates
   on the way in and out, and addresses the file by element rather than byte. */
void __ada_direct_create(AdaFileRef handle, int mode, const char* name, int length, const char* form,
                         int formLength);
void __ada_direct_open(AdaFileRef handle, int mode, const char* name, int length, const char* form,
                       int formLength);
void __ada_direct_reset(AdaFileRef handle, int mode);
void __ada_direct_reset_same(AdaFileRef handle);
int __ada_direct_mode(AdaFileRef handle);
void __ada_direct_read_at(AdaFileRef handle, void* item, int size, int index);
void __ada_direct_write_at(AdaFileRef handle, const void* item, int size, int index);
void __ada_direct_set_index(AdaFileRef handle, int index, int size);
int __ada_direct_index(AdaFileRef handle, int size);
int __ada_direct_size(AdaFileRef handle, int size);

/* Stream_IO addresses the file by byte.  A Stream_Access is the file itself,
   which is all a stream needs to be without tagged types to build one on. */
AdaFile* __ada_stream_of(AdaFileRef handle);
void __ada_stream_read(AdaFile* stream, void* item, int size);
void __ada_stream_write(AdaFile* stream, const void* item, int size);

/* 'Output writes the bounds of an array ahead of its elements, and 'Input
   reads them back into a buffer the run time owns. */
void __ada_stream_write_bounds(AdaFile* stream, int first, int last);
void* __ada_stream_read_array(AdaFile* stream, int elementSize, int* first, int* last);

void __ada_stream_elements_read(AdaFileRef handle, void* item, int length, int first, int* last);
void __ada_stream_elements_write(AdaFileRef handle, const void* item, int length);
void __ada_stream_set_index(AdaFileRef handle, int index);
int __ada_stream_index(AdaFileRef handle);
int __ada_stream_size(AdaFileRef handle);

void __ada_text_put(AdaFileRef handle, const char* item, int length);
void __ada_text_put_line(AdaFileRef handle, const char* item, int length);
void __ada_text_put_character(AdaFileRef handle, int item);
void __ada_text_put_integer(AdaFileRef handle, int item, int width, int base);
void __ada_text_put_float(AdaFileRef handle, double item, int fore, int aft, int exponent);
void __ada_text_new_line(AdaFileRef handle, int spacing);
void __ada_text_skip_line(AdaFileRef handle, int spacing);
int __ada_text_end_of_line(AdaFileRef handle);
void __ada_text_get(AdaFileRef handle, char* item);
void __ada_text_get_line(AdaFileRef handle, char* item, int length, int* last);
void __ada_text_get_float(AdaFileRef handle, void* item, int size);

/* An instance of one of the Text_IO generics tells the run time about the type
   it was made with: how wide a value is, the range one read has to fall in, and
   the names of the literals of an enumeration. */
void __ada_text_get_integer(AdaFileRef handle, void* item, int size, int low, int high);
void __ada_text_put_enum(AdaFileRef handle, int item, int width, int set, const char** names, int count);
void __ada_text_get_enum(AdaFileRef handle, void* item, int size, const char** names, int count);

/* Text_IO on the current input and output. */
void __ada_put(const char* item, int length);
void __ada_put_line(const char* item, int length);
void __ada_put_character(int item);
void __ada_put_integer(int item, int width, int base);
void __ada_new_line(int spacing);
void __ada_skip_line(int spacing);
int __ada_end_of_file(void);
int __ada_end_of_line(void);
void __ada_get(char* item);
void __ada_get_line(char* item, int length, int* last);
void __ada_get_float(void* item, int size);
void __ada_get_integer(void* item, int size, int low, int high);
void __ada_put_enum(int item, int width, int set, const char** names, int count);
void __ada_get_enum(void* item, int size, const char** names, int count);

#endif
