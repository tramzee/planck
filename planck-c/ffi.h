#include <JavaScriptCore/JavaScript.h>

JSValueRef function_dlopen(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                           size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_dlsym(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                          size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_dlclose(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                            size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_native_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_invoke_native(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_register_native(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef args[], JSValueRef *exception);

JSValueRef function_uname(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                          size_t argc, const JSValueRef args[], JSValueRef *exception);
