#ifdef __APPLE__
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include <dlfcn.h>

#include "ffi.h"
#include "jsc_utils.h"

#include <stdio.h>

unsigned char
foo(unsigned int, float);

int
test_main(int argc, const char **argv)
{
    ffi_cif cif;
    ffi_type *arg_types[2];
    void *arg_values[2];
    ffi_status status;

    // Because the return value from foo() is smaller than sizeof(long), it
    // must be passed as ffi_arg or ffi_sarg.
    ffi_arg result;

    // Specify the data type of each argument. Available types are defined
    // in <ffi/ffi.h>.
    arg_types[0] = &ffi_type_uint;
    arg_types[1] = &ffi_type_float;

    // Prepare the ffi_cif structure.
    if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                               2, &ffi_type_uint8, arg_types)) != FFI_OK)
    {
        // Handle the ffi_status error.
    }

    // Specify the values of each argument.
    unsigned int arg1 = 42;
    float arg2 = 5.1;

    arg_values[0] = &arg1;
    arg_values[1] = &arg2;

    // Invoke the function.
    ffi_call(&cif, FFI_FN(foo), &result, arg_values);

    // The ffi_arg 'result' now contains the unsigned char returned from foo(),
    // which can be accessed by a typecast.
    printf("result is %hhu", (unsigned char)result);

    return 0;
}

// The target function.
unsigned char
foo(unsigned int x, float y)
{
    unsigned char result = x - y;
    return result;
}

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

        double (*double_fn)(double);
        double_fn = fp;
        double result = (*double_fn)(JSValueToNumber(ctx, args[1], NULL));

        return JSValueMakeNumber(ctx, result);
    }

    return JSValueMakeNull(ctx);
}