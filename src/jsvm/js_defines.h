#ifndef WINDOW_JS_DEFINES_H
#define WINDOW_JS_DEFINES_H

#define DEF_GLOBAL_OBJECT(J,name) js_newobject(J);
#define REGISTER_FUNCTION(J,func,name,params) js_newcfunction(J, func, name, params); js_setproperty(J, -2, name);
#define REGISTER_GLOBAL_OBJECT(J,name) js_setglobal(J, #name);
#define REGISTER_GLOBAL_FUNCTION(J,func,name,params) js_newcfunction(J, func, name, params); js_setglobal(J, name);
#define REGISTER_GLOBAL_CONSTANT(J, name) js_newnumber(J, name); js_setglobal(J, #name);

#endif // WINDOW_JSVM_H