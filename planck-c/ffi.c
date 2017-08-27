#ifdef __APPLE__
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include <dlfcn.h>

#include "ffi.h"
#include "jsc_utils.h"

void* str_to_void_star(const char *s) {
    return (void*) atoll(s);
}

char *void_star_to_str(void* value) {
    char *rv = malloc(21);
    sprintf(rv, "%llu", (unsigned long long) value);
    return rv;
}

JSValueRef function_dlopen(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                           size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc == 1
        && JSValueGetType(ctx, args[0]) == kJSTypeString) {

        char *path = value_to_c_string(ctx, args[0]);
        void *handle = dlopen(path, RTLD_LAZY);
        free(path);

        if (handle) {
            char *handle_str = void_star_to_str(handle);
            JSValueRef rv = c_string_to_value(ctx, handle_str);
            free(handle_str);
            return rv;
        }
    }

    return JSValueMakeNull(ctx);
}

JSValueRef function_dlsym(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                          size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc == 2
        && JSValueGetType(ctx, args[0]) == kJSTypeString
        && JSValueGetType(ctx, args[1]) == kJSTypeString) {

        char *handle_str = value_to_c_string(ctx, args[0]);
        void *handle = str_to_void_star(handle_str);
        free(handle_str);

        char* sym = value_to_c_string(ctx, args[1]);
        void* fp = dlsym(handle, sym);
        free(sym);

        if (fp) {
            char *fp_str = void_star_to_str(fp);
            JSValueRef rv = c_string_to_value(ctx, fp_str);
            free(fp_str);
            return rv;
        }
    }

    return JSValueMakeNull(ctx);
}

JSValueRef function_dlclose(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                            size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc == 1
        && JSValueGetType(ctx, args[0]) == kJSTypeString) {

        char *handle_str = value_to_c_string(ctx, args[0]);
        void *handle = str_to_void_star(handle_str);
        free(handle_str);

        dlclose(handle);
    }

    return JSValueMakeNull(ctx);
}

JSValueRef function_native_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc > 0
        && JSValueGetType(ctx, args[0]) == kJSTypeString) {

        char *fp_str = value_to_c_string(ctx, args[0]);
        void *fp = str_to_void_star(fp_str);
        free(fp_str);

        ffi_cif cif;
        ffi_type *arg_types[1];
        void *arg_values[1];
        ffi_status status;

        // Specify the data type of each argument. Available types are defined
        // in <ffi/ffi.h>.
        arg_types[0] = &ffi_type_double;

        // Prepare the ffi_cif structure.
        if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                                   1, &ffi_type_double, arg_types)) != FFI_OK)
        {
            // Handle the ffi_status error.
        }

        // Specify the values of each argument.
        double arg1 = JSValueToNumber(ctx, args[1], NULL);

        arg_values[0] = &arg1;

        // Invoke the function.
        double result;
        ffi_call(&cif, fp, &result, arg_values);

        return JSValueMakeNumber(ctx, result);
    }

    return JSValueMakeNull(ctx);
}