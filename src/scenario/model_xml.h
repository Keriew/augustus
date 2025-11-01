#ifndef SCENARIO_MODEL_XML_H
#define SCENARIO_MODEL_XML_H

#define MODEL_DATA_VERSION 1

int scenario_model_export_to_xml(const char *filename);
int scenario_model_xml_parse_file(const char *filename);

#endif // SCENARIO_MODEL_XML_H