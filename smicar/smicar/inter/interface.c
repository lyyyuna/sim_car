#include "MEM.h"
#include "DBG.h"
#define GLOBAL_VARIABLE_DEFINE
#include "simcar.h"

static void
add_native_functions(SIMCAR_Interpreter *inter)
{
    SIMCAR_add_native_function(inter, "print", crb_nv_print_proc);
    SIMCAR_add_native_function(inter, "fopen", crb_nv_fopen_proc);
    SIMCAR_add_native_function(inter, "fclose", crb_nv_fclose_proc);
    SIMCAR_add_native_function(inter, "fgets", crb_nv_fgets_proc);
    SIMCAR_add_native_function(inter, "fputs", crb_nv_fputs_proc);
    SIMCAR_add_native_function(inter, "new_array", crb_nv_new_array_proc);
	SIMCAR_add_native_function(inter, "getLeft", crb_nv_get_left_proc);
	SIMCAR_add_native_function(inter, "getRight", crb_nv_get_right_proc);
	SIMCAR_add_native_function(inter, "getSpeed", crb_nv_get_speed_proc);
	SIMCAR_add_native_function(inter, "setServo", crb_nv_set_servo_proc);
	SIMCAR_add_native_function(inter, "setMotor", crb_nv_set_motor_proc);
}

SIMCAR_Interpreter *
SIMCAR_create_interpreter(void)
{
    MEM_Storage storage;
    SIMCAR_Interpreter *interpreter;

    storage = MEM_open_storage(0);
    interpreter = MEM_storage_malloc(storage,
                                     sizeof(struct SIMCAR_Interpreter_tag));
    interpreter->interpreter_storage = storage;
    interpreter->execute_storage = NULL;
    interpreter->variable = NULL;
    interpreter->function_list = NULL;
    interpreter->statement_list = NULL;
    interpreter->current_line_number = 1;
    interpreter->stack.stack_alloc_size = 0;
    interpreter->stack.stack_pointer = 0;
    interpreter->stack.stack
        = MEM_malloc(sizeof(SIMCAR_Value) * STACK_ALLOC_SIZE);
    interpreter->heap.current_heap_size = 0;
    interpreter->heap.current_threshold = HEAP_THRESHOLD_SIZE;
    interpreter->heap.header = NULL;
    interpreter->top_environment = NULL;

    crb_set_current_interpreter(interpreter);
    add_native_functions(interpreter);

    return interpreter;
}

void
SIMCAR_compile(SIMCAR_Interpreter *interpreter, FILE *fp)
{
    extern int yyparse(void);
    extern FILE *yyin;

    crb_set_current_interpreter(interpreter);

    yyin = fp;
    if (yyparse()) {
        fprintf(stderr, "Error ! Error ! Error !\n");
        exit(1);
    }
    crb_reset_string_literal_buffer();
}

void
SIMCAR_interpret(SIMCAR_Interpreter *interpreter)
{
    interpreter->execute_storage = MEM_open_storage(0);
    crb_add_std_fp(interpreter);
    crb_execute_statement_list(interpreter, NULL, interpreter->statement_list);
    crb_garbage_collect(interpreter);
}

static void
release_global_strings(SIMCAR_Interpreter *interpreter) {
    while (interpreter->variable) {
        Variable *temp = interpreter->variable;
        interpreter->variable = temp->next;
    }
}

void
SIMCAR_dispose_interpreter(SIMCAR_Interpreter *interpreter)
{
    release_global_strings(interpreter);

    if (interpreter->execute_storage) {
        MEM_dispose_storage(interpreter->execute_storage);
    }
    interpreter->variable = NULL;
    crb_garbage_collect(interpreter);
    DBG_assert(interpreter->heap.current_heap_size == 0,
               ("%d bytes leaked.\n", interpreter->heap.current_heap_size));
    MEM_free(interpreter->stack.stack);
    MEM_dispose_storage(interpreter->interpreter_storage);
}

void
SIMCAR_add_native_function(SIMCAR_Interpreter *interpreter,
                        char *name, SIMCAR_NativeFunctionProc *proc)
{
    FunctionDefinition *fd;

    fd = crb_malloc(sizeof(FunctionDefinition));
    fd->name = name;
    fd->type = NATIVE_FUNCTION_DEFINITION;
    fd->u.native_f.proc = proc;
    fd->next = interpreter->function_list;

    interpreter->function_list = fd;
}
