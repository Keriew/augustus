#ifndef EMPIRE_XML_H
#define EMPIRE_XML_H

#include "core/buffer.h"

int empire_xml_parse_file(const char *filename);

int *get_current_invasion_path_id(void);

#endif // EMPIRE_XML_H
