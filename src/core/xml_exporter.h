#ifndef CORE_XML_EXPORTER_H
#define CORE_XML_EXPORTER_H

#include "core/buffer.h"

/**
 * @brief Adds the requested number of whitespaces to the buffer.
 * 
 * @param buf The buffer that will be written to the file.
 * @param count How many whitespaces to add
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_whitespaces(buffer *buf, int count);

/**
 * @brief Adds a LF+CR (new line) to the buffer.
 * 
 * @param buf The buffer that will be written to the file.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_newline(buffer *buf);

/**
 * @brief Adds the xml header (for events) info to the buffer. Should be the first thing in the buffer.
 * 
 * @param buf The buffer that will be written to the file.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_doctype_header_events(buffer *buf);

/**
 * @brief Starts a new xml tag (opening) at the given indent.
 * 
 * @param buf The buffer that will be written to the file.
 * @param tag The xml tag to add to the buffer.
 * @param indent How many spaces the tag must be indented.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_open_tag_start(buffer *buf, const uint8_t *tag, int indent);

/**
 * @brief Outputs an attribute with its value. Should be done after an open tag start, but before the open tag end.
 * 
 * @param buf The buffer that will be written to the file.
 * @param name The name of the attribute.
 * @param value The value (as text) of the attribute.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_open_tag_attribute(buffer *buf, const char *name, const uint8_t *value);

/**
 * @brief Outputs an attribute with its value. Should be done after an open tag start, but before the open tag end.
 * 
 * @param buf The buffer that will be written to the file.
 * @param name The name of the attribute.
 * @param value The int value to convert to text and write to buffer as the value of the attribute.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_open_tag_attribute_int(buffer *buf, const char *name, int value);

/**
 * @brief Closes off a xml tag (opening), either finally or still allowing for children tags.
 * 
 * @param buf The buffer that will be written to the file.
 * @param finally If not 0, then will close the open tag with />, else will allow for children with just a >.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_open_tag_end(buffer *buf, int finally);

/**
 * @brief Creates an end tag for a xml (should be after children tags).
 * 
 * @param buf The buffer that will be written to the file.
 * @param tag The xml tag to add to the buffer.
 * @param indent How many spaces the tag must be indented.
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_ending_tag(buffer *buf, const uint8_t *tag, int indent);

/**
 * @brief Exports scenario events to a buffer.
 * 
 * @param buf The buffer..
 * @return 1 if successful, 0 if not. 
 */
int xml_exporter_scenario_events(buffer *buf);

#endif // CORE_XML_EXPORTER_H
