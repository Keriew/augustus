#ifndef FIGURE_TYPE_REGISTRY_H
#define FIGURE_TYPE_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

const char *figure_type_registry_get_failure_reason(void);
int figure_type_registry_load(void);
void figure_type_registry_reset(void);

#ifdef __cplusplus
}
#endif

#endif // FIGURE_TYPE_REGISTRY_H
