#include "message_media_text_blob.h"

#include "core/log.h"
#include "core/string.h"

#include <stdlib.h>
#include <string.h>

static message_media_text_blob_t message_media_text_blob;

message_media_text_blob_t *message_media_text_get_data(void)
{
    return &message_media_text_blob;
}

void message_media_text_blob_clear(void)
{
    memset(message_media_text_blob.text_blob, 0, sizeof(message_media_text_blob.text_blob));
    message_media_text_blob.size = 0;

    memset(message_media_text_blob.text_entries, 0, sizeof(message_media_text_blob.text_entries));
    message_media_text_blob.entry_count = 0;
}

text_blob_string_t *message_media_text_blob_get_entry(int id)
{
    if (id < 0 || id >= message_media_text_blob.entry_count) {
        return 0;
    }
    return &message_media_text_blob.text_entries[id];
}

uint8_t *message_media_text_blob_get_text(int offset)
{
    if (!offset) {
        return 0;
    }
    return &message_media_text_blob.text_blob[offset];
}

text_blob_string_t *message_media_text_blob_add(const uint8_t *text)
{
    int length = string_length(text) + 1; //+1 to allow for end of string.
    int offset = message_media_text_blob.size;
    int index = message_media_text_blob.entry_count;

    if (offset + length >= MESSAGE_MEDIA_TEXT_BLOB_MAXIMUM_SIZE || index >= MESSAGE_MEDIA_TEXT_BLOB_MAXIMUM_ENTRIES) {
        log_error("This will overfill the message_media_text_blob. The game will now crash.", 0, 0);
    }

    message_media_text_blob.text_entries[index].id = index;
    message_media_text_blob.text_entries[index].in_use = 1;
    message_media_text_blob.text_entries[index].length = length;
    message_media_text_blob.text_entries[index].offset = offset;

    string_copy(text, &message_media_text_blob.text_blob[offset], length);
    message_media_text_blob.text_entries[index].text = &message_media_text_blob.text_blob[offset];

    message_media_text_blob.entry_count++;
    message_media_text_blob.size += length;

    return &message_media_text_blob.text_entries[index];
}

void message_media_text_blob_save_state(buffer *blob_buffer, buffer *meta_buffer)
{
    int32_t array_size = message_media_text_blob.size;
    int32_t struct_size = sizeof(uint8_t);
    buffer_init_dynamic_piece(blob_buffer,
        MESSAGE_MEDIA_TEXT_BLOB_VERSION,
        array_size,
        struct_size);

    buffer_write_raw(blob_buffer, message_media_text_blob.text_blob, array_size);

    array_size = message_media_text_blob.entry_count;
    struct_size = (3 * sizeof(int32_t));
    buffer_init_dynamic_piece(meta_buffer,
        MESSAGE_MEDIA_TEXT_BLOB_VERSION,
        array_size,
        struct_size);

    for (int i = 0; i < array_size; i++) {
        buffer_write_i32(meta_buffer, message_media_text_blob.text_entries[i].id);
        buffer_write_i32(meta_buffer, message_media_text_blob.text_entries[i].offset);
        buffer_write_i32(meta_buffer, message_media_text_blob.text_entries[i].length);
    }
}

void message_media_text_blob_load_state(buffer *blob_buffer, buffer *meta_buffer)
{
    message_media_text_blob_clear();

    int buffer_size, version, array_size, struct_size;
    buffer_load_dynamic_piece_header_data(blob_buffer,
        &buffer_size,
        &version,
        &array_size,
        &struct_size);

    message_media_text_blob.size = buffer_size;    
    buffer_read_raw(blob_buffer, message_media_text_blob.text_blob, message_media_text_blob.size);

    buffer_load_dynamic_piece_header_data(meta_buffer,
        &buffer_size,
        &version,
        &array_size,
        &struct_size);

    message_media_text_blob.entry_count = array_size;
    for (int i = 0; i < array_size; i++) {
        message_media_text_blob.text_entries[i].id = buffer_read_i32(meta_buffer);
        message_media_text_blob.text_entries[i].in_use = 1;
        int offset = buffer_read_i32(meta_buffer);
        message_media_text_blob.text_entries[i].offset = offset;
        message_media_text_blob.text_entries[i].length = buffer_read_i32(meta_buffer);
        message_media_text_blob.text_entries[i].text = &message_media_text_blob.text_blob[offset];
    }
}

