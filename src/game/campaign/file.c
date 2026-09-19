#include "file.h"

#include "core/file.h"

#include "zip/zip.h"

#define CAMPAIGNS_PREFIX_SIZE sizeof(CAMPAIGNS_DIRECTORY)

static struct {
    int is_folder;
    char file_name[FILE_NAME_MAX];
    int file_name_offset;
    struct {
        FILE *stream;
        struct zip_t *parser;
    } zip;
} data;

int campaign_file_exists(const char *filename)
{
    if (data.is_folder) {
        snprintf(&data.file_name[data.file_name_offset], FILE_NAME_MAX - data.file_name_offset, "/%s", filename);
        return dir_get_file_at_location(data.file_name, PATH_LOCATION_CAMPAIGN) != 0;
    }
    int close_at_end = data.zip.parser == 0;
    if (!campaign_file_open_zip()) {
        return 0;
    }
    int has_file = zip_entry_open(data.zip.parser, filename) == 0;
    zip_entry_close(data.zip.parser);
    if (close_at_end) {
        campaign_file_close_zip();
    }
    return has_file;
}

static void *load_file_at_path(const char *path, size_t *length)
{
    *length = 0;
    const char *filename = dir_get_file_at_location(path, PATH_LOCATION_CAMPAIGN);
    FILE *stream = filename ? file_open(filename, "rb") : 0;
    if (!stream) {
        return 0;
    }
    long file_size = -1;
    if (fseek(stream, 0, SEEK_END) == 0) {
        file_size = ftell(stream);
    }
    if (file_size < 0 || fseek(stream, 0, SEEK_SET) != 0) {
        file_close(stream);
        return 0;
    }
    size_t size = (size_t) file_size;
    uint8_t *buffer = malloc(size ? size : 1);
    if (!buffer) {
        file_close(stream);
        return 0;
    }
    size_t result = fread(buffer, 1, size, stream);
    file_close(stream);
    if (result != size) {
        free(buffer);
        return 0;
    }
    *length = size;
    return buffer;
}

static void *load_zip_entry(struct zip_t *zip, const char *file, size_t *length)
{
    *length = 0;
    if (zip_entry_open(zip, file) < 0) {
        return 0;
    }
    size_t size = zip_entry_size(zip);
    uint8_t *buffer = malloc(size ? size : 1);
    if (!buffer) {
        zip_entry_close(zip);
        return 0;
    }
    size_t result = zip_entry_noallocread(zip, buffer, size);
    zip_entry_close(zip);
    if (result != size) {
        free(buffer);
        return 0;
    }
    *length = size;
    return buffer;
}

static void *load_file_from_folder(const char *file, size_t *length)
{
    *length = 0;
    int size = snprintf(&data.file_name[data.file_name_offset],
        FILE_NAME_MAX - data.file_name_offset, "/%s", file);
    if (size < 0 || size >= FILE_NAME_MAX - data.file_name_offset) {
        return 0;
    }
    return load_file_at_path(data.file_name, length);
}

static void *load_file_from_zip(const char *file, size_t *length)
{
    *length = 0;
    int close_at_end = data.zip.parser == 0;
    if (!campaign_file_open_zip()) {
        return 0;
    }
    void *buffer = load_zip_entry(data.zip.parser, file, length);
    if (close_at_end) {
        campaign_file_close_zip();
    }
    return buffer;
}

struct campaign_file_reader {
    char name[FILE_NAME_MAX];
    int is_folder;
    FILE *stream;
    struct zip_t *zip;
};

campaign_file_reader *campaign_file_reader_open(const char *campaign_name)
{
    if (!campaign_name || !*campaign_name || strlen(campaign_name) >= FILE_NAME_MAX) {
        return 0;
    }
    campaign_file_reader *reader = calloc(1, sizeof(*reader));
    if (!reader) {
        return 0;
    }
    strcpy(reader->name, campaign_name);
    reader->is_folder = !file_has_extension(campaign_name, "campaign");
    if (!reader->is_folder) {
        const char *filename = dir_get_file_at_location(campaign_name, PATH_LOCATION_CAMPAIGN);
        reader->stream = filename ? file_open(filename, "rb") : 0;
        reader->zip = reader->stream ? zip_cstream_open(reader->stream, 0, 'r') : 0;
        if (!reader->zip) {
            campaign_file_reader_close(reader);
            return 0;
        }
    }
    return reader;
}

void campaign_file_reader_close(campaign_file_reader *reader)
{
    if (!reader) {
        return;
    }
    if (reader->zip) {
        zip_close(reader->zip);
    }
    if (reader->stream) {
        file_close(reader->stream);
    }
    free(reader);
}

void *campaign_file_reader_load(campaign_file_reader *reader, const char *file, size_t *length)
{
    *length = 0;
    if (!reader || !file || !*file) {
        return 0;
    }
    if (!reader->is_folder) {
        return load_zip_entry(reader->zip, file, length);
    }
    char path[FILE_NAME_MAX];
    int size = snprintf(path, sizeof(path), "%s/%s", reader->name, file);
    return size >= 0 && size < FILE_NAME_MAX ? load_file_at_path(path, length) : 0;
}

int campaign_file_reader_exists(campaign_file_reader *reader, const char *file)
{
    if (!reader || !file || !*file) {
        return 0;
    }
    if (reader->is_folder) {
        char path[FILE_NAME_MAX];
        int size = snprintf(path, sizeof(path), "%s/%s", reader->name, file);
        return size >= 0 && size < FILE_NAME_MAX &&
            dir_get_file_at_location(path, PATH_LOCATION_CAMPAIGN) != 0;
    }
    if (zip_entry_open(reader->zip, file) != 0) {
        return 0;
    }
    zip_entry_close(reader->zip);
    return 1;
}

void *campaign_file_load(const char *file, size_t *length)
{
    return data.is_folder ? load_file_from_folder(file, length) : load_file_from_zip(file, length);
}

void campaign_file_set_path(const char *path)
{
    campaign_file_close_zip();
    if (path && path[0]) {
        data.is_folder = !file_has_extension(path, "campaign");
        data.file_name_offset = snprintf(data.file_name, FILE_NAME_MAX, "%s", path);
    } else {
        data.file_name[0] = 0;
        data.file_name_offset = 0;
        data.is_folder = 0;
    }
}

const char *campaign_file_remove_prefix(const char *path)
{
    if (!data.file_name[0]) {
        return 0;
    }
    if (strncmp(path, CAMPAIGNS_DIRECTORY "/", CAMPAIGNS_PREFIX_SIZE) != 0) {
        return 0;
    }
    path += CAMPAIGNS_PREFIX_SIZE;

    return path;
}

int campaign_file_is_zip(void)
{
    return !data.is_folder;
}

int campaign_file_open_zip(void)
{
    if (data.is_folder) {
        return 1;
    }
    if (!*data.file_name) {
        return 0;
    }
    if (!data.zip.stream) {
        const char *filename = dir_get_file_at_location(data.file_name, PATH_LOCATION_CAMPAIGN);
        if (!filename) {
            return 0;
        }
        data.zip.stream = file_open(filename, "rb");
        if (!data.zip.stream) {
            return 0;
        }
    }
    if (!data.zip.parser) {
        data.zip.parser = zip_cstream_open(data.zip.stream, 0, 'r');
        if (!data.zip.parser) {
            campaign_file_close_zip();
            return 0;
        }
    }
    return 1;
}

void campaign_file_close_zip(void)
{
    if (data.zip.parser) {
        zip_close(data.zip.parser);
        data.zip.parser = 0;
    }
    if (data.zip.stream) {
        file_close(data.zip.stream);
        data.zip.stream = 0;
    }
}
