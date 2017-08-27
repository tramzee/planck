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

ffi_type* int_to_ffi_type(int type) {
    //fprintf(stderr, "%d\n", type);
    switch (type) {
        case FFI_TYPE_VOID:
            return &ffi_type_void;
        case FFI_TYPE_UINT8:
            return &ffi_type_uint8;
        case FFI_TYPE_SINT8:
            return &ffi_type_sint8;
        case FFI_TYPE_UINT16:
            return &ffi_type_uint16;
        case FFI_TYPE_SINT16:
            return &ffi_type_sint16;
        case FFI_TYPE_UINT32:
            return &ffi_type_uint32;
        case FFI_TYPE_SINT32:
            return &ffi_type_sint32;
        case FFI_TYPE_UINT64:
            return &ffi_type_uint64;
        case FFI_TYPE_SINT64:
            return &ffi_type_sint64;
        case FFI_TYPE_FLOAT:
            return &ffi_type_float;
        case FFI_TYPE_DOUBLE:
            return &ffi_type_double;
#ifdef FFI_TYPE_LONGDOUBLE
        case FFI_TYPE_LONGDOUBLE:
            return &ffi_type_longdouble;
#endif
        case FFI_TYPE_POINTER:
            return &ffi_type_pointer;
        default:
            // TODO throw?
            return NULL;
    }
}

JSValueRef function_native_call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc > 0
        && JSValueGetType(ctx, args[0]) == kJSTypeString // Encoded fp
        && JSValueGetType(ctx, args[1]) == kJSTypeNumber // return type
        && JSValueGetType(ctx, args[2]) == kJSTypeObject // array of numbers, arg types
        && JSValueGetType(ctx, args[3]) == kJSTypeObject // array of arguments
        ) {

        char *fp_str = value_to_c_string(ctx, args[0]);
        void *fp = str_to_void_star(fp_str);
        free(fp_str);

        unsigned int arg_count = 1; // TODO, derive from args[2] length

        JSObjectRef arg_types_ref = JSValueToObject(ctx, args[2], NULL);

        int type_ints[arg_count];
        unsigned int i;
        for (i = 0; i < arg_count; i++) {
            type_ints[i] = (int)JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, arg_types_ref, i, NULL), NULL);
        }

        ffi_cif cif;
        ffi_type *arg_types[arg_count];

        for (i = 0; i < arg_count; i++) {
            arg_types[i] = int_to_ffi_type(type_ints[i]);
        }

        ffi_type* return_type = int_to_ffi_type((int)JSValueToNumber(ctx, args[1], NULL));

        ffi_status status;

        // Prepare the ffi_cif structure.
        if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                                   arg_count, return_type, arg_types)) != FFI_OK)
        {
            // Handle the ffi_status error.
        }

        JSObjectRef arg_values_ref = JSValueToObject(ctx, args[3], NULL);

        void *arg_values[arg_count];
        for (i = 0; i < arg_count; i++) {
            JSValueRef arg_value = JSObjectGetPropertyAtIndex(ctx, arg_values_ref, i, NULL);
            switch (type_ints[i]) {
                // TODO add others
                case FFI_TYPE_FLOAT:
                    arg_values[i] = malloc(sizeof(float));
                    *(float *) arg_values[i] = (float)JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_DOUBLE:
                    arg_values[i] = malloc(sizeof(double));
                    *(double *) arg_values[i] = JSValueToNumber(ctx, arg_value, NULL);
                break;
                default:
                    break;
            }
        }

        // TODO switch on return type
        void* result = malloc(sizeof(double));

        ffi_call(&cif, fp, result, arg_values);

        // TODO switch on return type
        JSValueRef rv = JSValueMakeNumber(ctx, *(double*)result);

        for (i = 0; i < arg_count; i++) {
            free(arg_values[i]);
        }
        free(result);

        return rv;
    }

    return JSValueMakeNull(ctx);
}