#ifndef STDLIB_FILE_H
#define STDLIB_FILE_H

#include "runtime/core/vm.h"

// File I/O functions
TaggedValue file_open(int arg_count, TaggedValue* args);
TaggedValue file_close(int arg_count, TaggedValue* args);
TaggedValue file_read(int arg_count, TaggedValue* args);
TaggedValue file_write(int arg_count, TaggedValue* args);
TaggedValue file_readLine(int arg_count, TaggedValue* args);
TaggedValue file_writeLine(int arg_count, TaggedValue* args);
TaggedValue file_exists(int arg_count, TaggedValue* args);
TaggedValue file_delete(int arg_count, TaggedValue* args);
TaggedValue file_copy(int arg_count, TaggedValue* args);
TaggedValue file_move(int arg_count, TaggedValue* args);

// Directory functions
TaggedValue dir_create(int arg_count, TaggedValue* args);
TaggedValue dir_delete(int arg_count, TaggedValue* args);
TaggedValue dir_exists(int arg_count, TaggedValue* args);
TaggedValue dir_list(int arg_count, TaggedValue* args);

#endif // STDLIB_FILE_H