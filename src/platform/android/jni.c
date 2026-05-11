#include "jni.h"

#include "core/log.h"

int jni_init_function_handler(const char *class_name, jni_function_handler *handler)
{
    handler->env = jni_get_env();
    if (handler->env == NULL) {
        log_error("Problem setting up JNI environment", 0, 0);
        return 0;
    }
    handler->activity = jni_get_activity();
    if (handler->activity == NULL) {
        log_error("Problem loading the activity.", 0, 0);
        return 0;
    }
    if(class_name) {
        handler->class = (*handler->env)->FindClass(handler->env, class_name);
    } else {
        handler->class = (*handler->env)->GetObjectClass(handler->env, handler->activity);
    }
    if (handler->class == NULL) {
        log_error("Problem loading class '%s'.", 0, 0);
        return 0;
    }
    return 1;
}

int jni_get_static_method_handler(
    const char *class_name, const char *method_name, const char *method_signature, jni_function_handler *handler)
{
    if (!jni_init_function_handler(class_name, handler)) {
        return 0;
    }
    handler->method = (*handler->env)->GetStaticMethodID(handler->env, handler->class, method_name, method_signature);
    if (handler->method == NULL) {
        log_error("Problem loading static method", method_name, 0);
        log_error("From class", class_name, 0);
        return 0;
    }
    return 1;
}

int jni_get_method_handler(
    const char *class_name, const char *method_name, const char *method_signature, jni_function_handler *handler)
{
    if (!jni_init_function_handler(class_name, handler)) {
        return 0;
    }
    handler->method = (*handler->env)->GetMethodID(handler->env, handler->class, method_name, method_signature);
    if (handler->method == NULL) {
        log_error("Problem loading method", method_name, 0);
        log_error("From class", class_name, 0);
        return 0;
    }
    return 1;
}

void jni_destroy_function_handler(jni_function_handler *handler)
{
    if (!handler) {
        return;
    }
    if (handler->env) {
        if (handler->activity) {
            (*handler->env)->DeleteLocalRef(handler->env, handler->activity);
        }
        if (handler->class) {
            (*handler->env)->DeleteLocalRef(handler->env, handler->class);
        }
    }
    handler->env = NULL;
    handler->class = NULL;
    handler->activity = NULL;
    handler->method = NULL;
}
