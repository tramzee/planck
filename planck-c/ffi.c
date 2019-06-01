#ifdef __APPLE__

#include <ffi/ffi.h>

#else
#include <ffi.h>
#endif

#include <stdlib.h>
#include <dlfcn.h>
#include <sys/errno.h>
#include <sys/utsname.h>

#include "ffi.h"
#include "jsc_utils.h"

void *str_to_void_star(const char *s) {
    return (void *) atoll(s);
}

char *void_star_to_str(void *value) {
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
        } else {
            JSValueRef arguments[1];
            arguments[0] = c_string_to_value(ctx, dlerror());
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
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

        char *sym = value_to_c_string(ctx, args[1]);
        void *fp = dlsym(handle, sym);
        free(sym);

        if (fp) {
            char *fp_str = void_star_to_str(fp);
            JSValueRef rv = c_string_to_value(ctx, fp_str);
            free(fp_str);
            return rv;
        } else {
            JSValueRef arguments[1];
            arguments[0] = c_string_to_value(ctx, dlerror());
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
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

        int rv = dlclose(handle);
        if (rv == -1) {
            JSValueRef arguments[1];
            arguments[0] = c_string_to_value(ctx, dlerror());
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
        }
    }

    return JSValueMakeNull(ctx);
}

ffi_type *int_to_ffi_type(int type) {
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

#if SHRT_MAX == 2147483647
typedef short int portable_int32_t;
typedef unsigned short int portable_uint32_t;
#elif INT_MAX == 2147483647
typedef int portable_int32_t;
typedef unsigned int portable_uint32_t;
#elif LONG_MAX == 2147483647
typedef long portable_int32_t ;
typedef unsigned long portable_uint32_t ;
#elif LLONG_MAX == 2147483647
typedef long long portable_int32_t;
typedef unsigned long long portable_uint32_t;
#else
#error "Cannot find 32bit integer."
#endif

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
            type_ints[i] = (int) JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, arg_types_ref, i, NULL), NULL);
        }

        ffi_cif cif;
        ffi_type *arg_types[arg_count];

        for (i = 0; i < arg_count; i++) {
            arg_types[i] = int_to_ffi_type(type_ints[i]);
        }

        int return_type_int = (int) JSValueToNumber(ctx, args[1], NULL);
        ffi_type *return_type = int_to_ffi_type(return_type_int);

        ffi_status status;

        // Prepare the ffi_cif structure.
        if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
                                   arg_count, return_type, arg_types)) != FFI_OK) {
            JSValueRef arguments[1];
            char *errmsg = NULL;
            if (status == FFI_BAD_ABI) {
                errmsg = "ffi_prep_cif failed returning FFI_BAD_ABI";
            } else if (status == FFI_BAD_TYPEDEF) {
                errmsg = "ffi_prep_cif failed returning FFI_BAD_TYPEDEF";
            } else {
                errmsg = "ffi_prep_cif failed";
            }
            arguments[0] = c_string_to_value(ctx, errmsg);
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
            return JSValueMakeNull(ctx);
        }

        JSObjectRef arg_values_ref = JSValueToObject(ctx, args[3], NULL);

        void *arg_values[arg_count];
        for (i = 0; i < arg_count; i++) {
            JSValueRef arg_value = JSObjectGetPropertyAtIndex(ctx, arg_values_ref, i, NULL);
            switch (type_ints[i]) {
                case FFI_TYPE_UINT8:
                    arg_values[i] = malloc(1);
                    *(unsigned char *) arg_values[i] = (unsigned char) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_SINT8:
                    arg_values[i] = malloc(1);
                    *(signed char *) arg_values[i] = (signed char) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_UINT16:
                    arg_values[i] = malloc(2);
                    *(unsigned short *) arg_values[i] = (unsigned short) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_SINT16:
                    arg_values[i] = malloc(2);
                    *(short *) arg_values[i] = (short) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_UINT32:
                    arg_values[i] = malloc(4);
                    *(portable_uint32_t *) arg_values[i] = (portable_uint32_t) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_SINT32:
                    arg_values[i] = malloc(4);
                    *(portable_int32_t *) arg_values[i] = (portable_int32_t) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_UINT64:
                    arg_values[i] = malloc(8);
                    *(signed long long *) arg_values[i] = (signed long long) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_SINT64:
                    arg_values[i] = malloc(8);
                    *(long long *) arg_values[i] = (long long) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_FLOAT:
                    arg_values[i] = malloc(sizeof(float));
                    *(float *) arg_values[i] = (float) JSValueToNumber(ctx, arg_value, NULL);
                    break;
                case FFI_TYPE_DOUBLE:
                    arg_values[i] = malloc(sizeof(double));
                    *(double *) arg_values[i] = JSValueToNumber(ctx, arg_value, NULL);
                    break;
#ifdef FFI_TYPE_LONGDOUBLE
                case FFI_TYPE_LONGDOUBLE:
                    arg_values[i] = malloc(sizeof(long double));
                    *(long double *) arg_values[i] = JSValueToNumber(ctx, arg_value, NULL);
                    break;
#endif
                default: {
                    JSValueRef arguments[1];
                    arguments[0] = c_string_to_value(ctx, "unhandled type ints case");
                    *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                    return JSValueMakeNull(ctx);
                }
                    break;
            }
        }

        // Allocate space to hold the return value

        void *result = NULL;
        switch (return_type_int) {
            case FFI_TYPE_UINT8:
                result = malloc(1);
                break;
            case FFI_TYPE_SINT8:
                result = malloc(1);
                break;
            case FFI_TYPE_UINT16:
                result = malloc(2);
                break;
            case FFI_TYPE_SINT16:
                result = malloc(2);
                break;
            case FFI_TYPE_UINT32:
                result = malloc(4);
                break;
            case FFI_TYPE_SINT32:
                result = malloc(4);
                break;
            case FFI_TYPE_UINT64:
                result = malloc(8);
                break;
            case FFI_TYPE_SINT64:
                result = malloc(8);
                break;
            case FFI_TYPE_FLOAT:
                result = malloc(sizeof(float));
                break;
            case FFI_TYPE_DOUBLE:
                result = malloc(sizeof(double));
                break;
#ifdef FFI_TYPE_LONGDOUBLE
            case FFI_TYPE_LONGDOUBLE:
                result = malloc(sizeof(long double));
                break;
#endif
            default: {
                JSValueRef arguments[1];
                arguments[0] = c_string_to_value(ctx, "unhandled result type ints case");
                *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                return JSValueMakeNull(ctx);
            }
                break;
        }

        ffi_call(&cif, fp, result, arg_values);

        // Marshal the return value

        JSValueRef rv = NULL;
        double temp;
        switch (return_type_int) {
            case FFI_TYPE_UINT8:
                temp = (double) *(unsigned char *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_SINT8:
                temp = (double) *(signed char *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_UINT16:
                temp = (double) *(unsigned short *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_SINT16:
                temp = (double) *(short *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_UINT32:
                temp = (double) *(portable_uint32_t *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_SINT32:
                temp = (double) *(portable_int32_t *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_UINT64:
                temp = (double) *(long long *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_SINT64:
                temp = (double) *(unsigned long long *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
            case FFI_TYPE_FLOAT:
                result = malloc(sizeof(float));
                break;
            case FFI_TYPE_DOUBLE:
                rv = JSValueMakeNumber(ctx, *(double *) result);
                break;
#ifdef FFI_TYPE_LONGDOUBLE
            case FFI_TYPE_LONGDOUBLE:
                temp = (double) *(long double *) result;
                rv = JSValueMakeNumber(ctx, temp);
                break;
#endif
            default: {
                JSValueRef arguments[1];
                arguments[0] = c_string_to_value(ctx, "unhandled result marshal type ints case");
                *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                return JSValueMakeNull(ctx);
            }
                break;
        }

        for (i = 0; i < arg_count; i++) {
            free(arg_values[i]);
        }
        free(result);

        return rv;
    }

    return JSValueMakeNull(ctx);
}

typedef struct native_info {
    void *fp;
    ffi_cif cif;
    size_t arg_count;
    int *type_ints;
    int return_type_int;
    void **arg_values;
    void *result;
} native_info_t;

native_info_t native_infos[1024];

JSValueRef function_invoke_native(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef args[], JSValueRef *exception) {

    native_info_t *native_info = &native_infos[0];

    JSObjectRef arg_values_ref = JSValueToObject(ctx, args[1], NULL);

    size_t arg_count = native_info->arg_count;
    void **arg_values = native_info->arg_values;
    int *type_ints = native_info->type_ints;

    size_t i;
    for (i = 0; i < arg_count; i++) {
        // TODO maybe just get this directly from args somehow
        JSValueRef arg_value = JSObjectGetPropertyAtIndex(ctx, arg_values_ref, i, NULL);
        switch (type_ints[i]) {
            case FFI_TYPE_UINT8:
                *(unsigned char *) arg_values[i] = (unsigned char) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_SINT8:
                *(signed char *) arg_values[i] = (signed char) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_UINT16:
                *(unsigned short *) arg_values[i] = (unsigned short) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_SINT16:
                *(short *) arg_values[i] = (short) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_UINT32:
                *(portable_uint32_t *) arg_values[i] = (portable_uint32_t) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_SINT32:
                *(portable_int32_t *) arg_values[i] = (portable_int32_t) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_UINT64:
                *(signed long long *) arg_values[i] = (signed long long) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_SINT64:
                *(long long *) arg_values[i] = (long long) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_FLOAT:
                *(float *) arg_values[i] = (float) JSValueToNumber(ctx, arg_value, NULL);
                break;
            case FFI_TYPE_DOUBLE:
                *(double *) arg_values[i] = JSValueToNumber(ctx, arg_value, NULL);
                break;
#ifdef FFI_TYPE_LONGDOUBLE
            case FFI_TYPE_LONGDOUBLE:
                *(long double *) arg_values[i] = JSValueToNumber(ctx, arg_value, NULL);
                break;
#endif
            default: {
                JSValueRef arguments[1];
                arguments[0] = c_string_to_value(ctx, "unhandled type ints case");
                *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                return JSValueMakeNull(ctx);
            }
                break;
        }
    }

    void *result = native_info->result;

    ffi_call(&native_info->cif, native_info->fp, result, arg_values);

    int return_type_int = native_info->return_type_int;

    // Marshal the return value

    JSValueRef rv = NULL;
    double temp;
    switch (return_type_int) {
        case FFI_TYPE_UINT8:
            temp = (double) *(unsigned char *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_SINT8:
            temp = (double) *(signed char *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_UINT16:
            temp = (double) *(unsigned short *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_SINT16:
            temp = (double) *(short *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_UINT32:
            temp = (double) *(portable_uint32_t *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_SINT32:
            temp = (double) *(portable_int32_t *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_UINT64:
            temp = (double) *(long long *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_SINT64:
            temp = (double) *(unsigned long long *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
        case FFI_TYPE_FLOAT:
            result = malloc(sizeof(float));
            break;
        case FFI_TYPE_DOUBLE:
            rv = JSValueMakeNumber(ctx, *(double *) result);
            break;
#ifdef FFI_TYPE_LONGDOUBLE
        case FFI_TYPE_LONGDOUBLE:
            temp = (double) *(long double *) result;
            rv = JSValueMakeNumber(ctx, temp);
            break;
#endif
        default: {
            JSValueRef arguments[1];
            arguments[0] = c_string_to_value(ctx, "unhandled result marshal type ints case");
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
            return JSValueMakeNull(ctx);
        }
            break;
    }

    return rv;
}

JSValueRef function_register_native(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef args[], JSValueRef *exception) {
    if (argc > 0
        && JSValueGetType(ctx, args[0]) == kJSTypeString // Encoded fp
        && JSValueGetType(ctx, args[1]) == kJSTypeNumber // return type
        && JSValueGetType(ctx, args[2]) == kJSTypeObject // array of numbers, arg types
            ) {

        size_t native_infos_ndx = 0;

        char *fp_str = value_to_c_string(ctx, args[0]);
        void *fp = str_to_void_star(fp_str);
        native_infos[native_infos_ndx].fp = fp;
        free(fp_str);

        unsigned int arg_count = 1; // TODO, derive from args[2] length
        native_infos[native_infos_ndx].arg_count = arg_count;

        JSObjectRef arg_types_ref = JSValueToObject(ctx, args[2], NULL);

        native_infos[native_infos_ndx].type_ints = malloc(sizeof(int) * arg_count);
        unsigned int i;
        for (i = 0; i < arg_count; i++) {
            native_infos[native_infos_ndx].type_ints[i] =
                    (int) JSValueToNumber(ctx, JSObjectGetPropertyAtIndex(ctx, arg_types_ref, i, NULL), NULL);
        }

        ffi_type *arg_types[arg_count];

        for (i = 0; i < arg_count; i++) {
            arg_types[i] = int_to_ffi_type(native_infos[native_infos_ndx].type_ints[i]);
        }

        int return_type_int = (int) JSValueToNumber(ctx, args[1], NULL);
        native_infos[native_infos_ndx].return_type_int = return_type_int;
        ffi_type *return_type = int_to_ffi_type(return_type_int);

        ffi_status status;

        // Prepare the ffi_cif structure.
        if ((status = ffi_prep_cif(&native_infos[native_infos_ndx].cif, FFI_DEFAULT_ABI,
                                   arg_count, return_type, arg_types)) != FFI_OK) {
            JSValueRef arguments[1];
            char *errmsg = NULL;
            if (status == FFI_BAD_ABI) {
                errmsg = "ffi_prep_cif failed returning FFI_BAD_ABI";
            } else if (status == FFI_BAD_TYPEDEF) {
                errmsg = "ffi_prep_cif failed returning FFI_BAD_TYPEDEF";
            } else {
                errmsg = "ffi_prep_cif failed";
            }
            arguments[0] = c_string_to_value(ctx, errmsg);
            *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
            return JSValueMakeNull(ctx);
        }

        native_infos[native_infos_ndx].arg_values = malloc((sizeof(void *)) * arg_count);
        for (i = 0; i < arg_count; i++) {
            switch (native_infos[native_infos_ndx].type_ints[i]) {
                case FFI_TYPE_UINT8:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(1);
                    break;
                case FFI_TYPE_SINT8:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(1);
                    break;
                case FFI_TYPE_UINT16:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(2);
                    break;
                case FFI_TYPE_SINT16:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(2);
                    break;
                case FFI_TYPE_UINT32:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(4);
                    break;
                case FFI_TYPE_SINT32:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(4);
                    break;
                case FFI_TYPE_UINT64:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(8);
                    break;
                case FFI_TYPE_SINT64:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(8);
                    break;
                case FFI_TYPE_FLOAT:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(sizeof(float));
                    break;
                case FFI_TYPE_DOUBLE:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(sizeof(double));
                    break;
#ifdef FFI_TYPE_LONGDOUBLE
                case FFI_TYPE_LONGDOUBLE:
                    native_infos[native_infos_ndx].arg_values[i] = malloc(sizeof(long double));
                    break;
#endif
                default: {
                    JSValueRef arguments[1];
                    arguments[0] = c_string_to_value(ctx, "unhandled type ints case");
                    *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                    return JSValueMakeNull(ctx);
                }
                    break;
            }
        }

        // Allocate space to hold the return value

        switch (return_type_int) {
            case FFI_TYPE_UINT8:
                native_infos[native_infos_ndx].result = malloc(1);
                break;
            case FFI_TYPE_SINT8:
                native_infos[native_infos_ndx].result = malloc(1);
                break;
            case FFI_TYPE_UINT16:
                native_infos[native_infos_ndx].result = malloc(2);
                break;
            case FFI_TYPE_SINT16:
                native_infos[native_infos_ndx].result = malloc(2);
                break;
            case FFI_TYPE_UINT32:
                native_infos[native_infos_ndx].result = malloc(4);
                break;
            case FFI_TYPE_SINT32:
                native_infos[native_infos_ndx].result = malloc(4);
                break;
            case FFI_TYPE_UINT64:
                native_infos[native_infos_ndx].result = malloc(8);
                break;
            case FFI_TYPE_SINT64:
                native_infos[native_infos_ndx].result = malloc(8);
                break;
            case FFI_TYPE_FLOAT:
                native_infos[native_infos_ndx].result = malloc(sizeof(float));
                break;
            case FFI_TYPE_DOUBLE:
                native_infos[native_infos_ndx].result = malloc(sizeof(double));
                break;
#ifdef FFI_TYPE_LONGDOUBLE
            case FFI_TYPE_LONGDOUBLE:
                native_infos[native_infos_ndx].result = malloc(sizeof(long double));
                break;
#endif
            default: {
                JSValueRef arguments[1];
                arguments[0] = c_string_to_value(ctx, "unhandled result type ints case");
                *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
                return JSValueMakeNull(ctx);
            }
                break;
        }

    }

    return JSValueMakeNull(ctx);
}

JSValueRef function_uname(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                          size_t argc, const JSValueRef args[], JSValueRef *exception) {

    struct utsname struct_utsname;
    int rv = uname(&struct_utsname);
    if (rv == -1) {
        JSValueRef arguments[1];
        arguments[0] = c_string_to_value(ctx, strerror(errno));
        *exception = JSObjectMakeError(ctx, 1, arguments, NULL);
        return JSValueMakeNull(ctx);
    } else {
        JSObjectRef result = JSObjectMake(ctx, NULL, NULL);
        JSObjectSetProperty(ctx, result, JSStringCreateWithUTF8CString("sysname"),
                            c_string_to_value(ctx, struct_utsname.sysname),
                            kJSPropertyAttributeReadOnly, NULL);
        JSObjectSetProperty(ctx, result, JSStringCreateWithUTF8CString("nodename"),
                            c_string_to_value(ctx, struct_utsname.nodename),
                            kJSPropertyAttributeReadOnly, NULL);
        JSObjectSetProperty(ctx, result, JSStringCreateWithUTF8CString("release"),
                            c_string_to_value(ctx, struct_utsname.release),
                            kJSPropertyAttributeReadOnly, NULL);
        JSObjectSetProperty(ctx, result, JSStringCreateWithUTF8CString("version"),
                            c_string_to_value(ctx, struct_utsname.version),
                            kJSPropertyAttributeReadOnly, NULL);
        JSObjectSetProperty(ctx, result, JSStringCreateWithUTF8CString("machine"),
                            c_string_to_value(ctx, struct_utsname.machine),
                            kJSPropertyAttributeReadOnly, NULL);
        return result;
    }

}

/*
 * How to use.
 *
 * Let native.c contain
 * #include <math.h>
 *
 * double funky(double x) {
 *	return sinh(1.0 / tgamma(x));
 * }
 *
 * Compile with
 * cc -fPIC -shared -o libnative.so native.c
 *
 * Then in Planck
 *
 * (def libnative (js/PLANCK_DLOPEN "libnative.so"))
 * (def funky (js/PLANCK_DLSYM libnative "funky"))
 * (js/PLANCK_NATIVE_CALL funky 3 #js [3] #js [3.2])
 * (js/PLANCK_DLCLOSE libnative)
 * Third line should should evaluate to 0.42434936931066086
 *
 * A wrapper ClojureScript library might have code like
 *
 * (defn make-fn [sym ret-type arg-types]
 *   (fn [& args]
 *     (js/PLANCK_NATIVE_CALL sym ret-type arg-types (into-array args))))
 *
 * which makes it easly to instead do
 *
 * (def funky (make-fn (js/PLANCK_DLSYM libnative "funky") 3 #js [3]))
 *
 * (funky 3.2)
 */