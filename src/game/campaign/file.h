#ifndef CAMPAIGN_FILE_H
#define CAMPAIGN_FILE_H

#include <stddef.h>

int campaign_file_exists(const char *file);
void *campaign_file_load(const char *file, size_t *length);
/* Scoped independent reads keep a ZIP open across a preview's related files. */
typedef struct campaign_file_reader campaign_file_reader;
campaign_file_reader *campaign_file_reader_open(const char *campaign_name);
void campaign_file_reader_close(campaign_file_reader *reader);
void *campaign_file_reader_load(campaign_file_reader *reader, const char *file, size_t *length);
int campaign_file_reader_exists(campaign_file_reader *reader, const char *file);
void campaign_file_set_path(const char *path);
const char *campaign_file_remove_prefix(const char *path);
int campaign_file_is_zip(void);
int campaign_file_open_zip(void);
void campaign_file_close_zip(void);

#endif // CAMPAIGN_FILE_H
