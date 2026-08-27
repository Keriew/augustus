#ifndef CUSTOM_MESSAGES_LOCALIZATION_H
#define CUSTOM_MESSAGES_LOCALIZATION_H

#include <stdint.h>

void custom_messages_localization_clear(void);
int custom_messages_localization_load(void);

uint8_t *custom_messages_localization_get_title(int message_id);
uint8_t *custom_messages_localization_get_subtitle(int message_id);
uint8_t *custom_messages_localization_get_text(int message_id);

#endif // CUSTOM_MESSAGES_LOCALIZATION_H
