#pragma once

#include "translation/translation.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *translation_key_to_name(translation_key key);
int translation_key_from_name(const char *name, translation_key *key);

#ifdef __cplusplus
}
#endif
