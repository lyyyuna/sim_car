#ifndef PUBLIC_SIMCAR_DEV_H_INCLUDED
#define PUBLIC_SIMCAR_DEV_H_INCLUDED
#include "SIM.h"

typedef enum {
    SIMCAR_FALSE = 0,
    SIMCAR_TRUE = 1
} SIMCAR_Boolean;

typedef struct SIMCAR_Object_tag SIMCAR_Object;
typedef struct SIMCAR_Array_tag SIMCAR_Array;
typedef struct SIMCAR_String_tag SIMCAR_String;
typedef struct SIMCAR_LocalEnvironment_tag SIMCAR_LocalEnvironment;

typedef struct {
    char        *name;
} SIMCAR_NativePointerInfo;

typedef enum {
    SIMCAR_BOOLEAN_VALUE = 1,
    SIMCAR_INT_VALUE,
    SIMCAR_DOUBLE_VALUE,
    SIMCAR_STRING_VALUE,
    SIMCAR_NATIVE_POINTER_VALUE,
    SIMCAR_NULL_VALUE,
    SIMCAR_ARRAY_VALUE
} SIMCAR_ValueType;

typedef struct {
    SIMCAR_NativePointerInfo       *info;
    void                        *pointer;
} SIMCAR_NativePointer;

typedef struct {
    SIMCAR_ValueType       type;
    union {
        SIMCAR_Boolean     boolean_value;
        int             int_value;
        double          double_value;
        SIMCAR_NativePointer       native_pointer;
        SIMCAR_Object      *object;
    } u;
} SIMCAR_Value;

typedef SIMCAR_Value SIMCAR_NativeFunctionProc(SIMCAR_Interpreter *interpreter,
                                         SIMCAR_LocalEnvironment *env,
                                         int arg_count, SIMCAR_Value *args);

void SIMCAR_add_native_function(SIMCAR_Interpreter *interpreter,
                             char *name, SIMCAR_NativeFunctionProc *proc);
void SIMCAR_add_global_variable(SIMCAR_Interpreter *inter,
                             char *identifier, SIMCAR_Value *value);
SIMCAR_Object *
SIMCAR_create_crowbar_string(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                          char *str);
SIMCAR_Object *SIMCAR_create_array(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                             int size);

char *SIMCAR_value_to_string(SIMCAR_Value *value);

#endif /* PUBLIC_SIMCAR_DEV_H_INCLUDED */
