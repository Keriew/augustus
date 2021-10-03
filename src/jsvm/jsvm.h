#ifndef WINDOW_JSVM_H
#define WINDOW_JSVM_H

void js_vm_setup(void);
void js_vm_sync(void);
int js_vm_exec_function_args(const char *funcname, const char *szTypes, ...);
int js_vm_exec_function(const char *funcname);
int js_vm_get_absolute_path(const char *path, char *rpath, int max_path);
void js_vm_reload_file(const char *path);
const char *js_vm_scripts_folder(void);

#endif // WINDOW_JSVM_H