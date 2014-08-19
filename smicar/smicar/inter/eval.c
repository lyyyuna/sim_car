#include <math.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "simcar.h"

static void
push_value(SIMCAR_Interpreter *inter, SIMCAR_Value *value)
{
    DBG_assert(inter->stack.stack_pointer <= inter->stack.stack_alloc_size,
               ("stack_pointer..%d, stack_alloc_size..%d\n",
                inter->stack.stack_pointer, inter->stack.stack_alloc_size));

    if (inter->stack.stack_pointer == inter->stack.stack_alloc_size) {
        inter->stack.stack_alloc_size += STACK_ALLOC_SIZE;
        inter->stack.stack
            = MEM_realloc(inter->stack.stack,
                          sizeof(SIMCAR_Value) * inter->stack.stack_alloc_size);
    }
    inter->stack.stack[inter->stack.stack_pointer] = *value;
    inter->stack.stack_pointer++;
}

static SIMCAR_Value
pop_value(SIMCAR_Interpreter *inter)
{
    SIMCAR_Value ret;

    ret = inter->stack.stack[inter->stack.stack_pointer-1];
    inter->stack.stack_pointer--;

    return ret;
}

static SIMCAR_Value *
peek_stack(SIMCAR_Interpreter *inter, int index)
{
    return &inter->stack.stack[inter->stack.stack_pointer - index - 1];
}

static void
shrink_stack(SIMCAR_Interpreter *inter, int shrink_size)
{
    inter->stack.stack_pointer -= shrink_size;
}


static void
eval_boolean_expression(SIMCAR_Interpreter *inter, SIMCAR_Boolean boolean_value)
{
    SIMCAR_Value   v;

    v.type = SIMCAR_BOOLEAN_VALUE;
    v.u.boolean_value = boolean_value;
    push_value(inter, &v);
}

static void
eval_int_expression(SIMCAR_Interpreter *inter, int int_value)
{
    SIMCAR_Value   v;

    v.type = SIMCAR_INT_VALUE;
    v.u.int_value = int_value;
    push_value(inter, &v);
}

static void
eval_double_expression(SIMCAR_Interpreter *inter, double double_value)
{
    SIMCAR_Value   v;

    v.type = SIMCAR_DOUBLE_VALUE;
    v.u.double_value = double_value;
    push_value(inter, &v);
}

static void
eval_string_expression(SIMCAR_Interpreter *inter, char *string_value)
{
    SIMCAR_Value   v;

    v.type = SIMCAR_STRING_VALUE;
    v.u.object = crb_literal_to_crb_string(inter, string_value);
    push_value(inter, &v);
}

static void
eval_null_expression(SIMCAR_Interpreter *inter)
{
    SIMCAR_Value   v;

    v.type = SIMCAR_NULL_VALUE;

    push_value(inter, &v);
}

static Variable *
search_global_variable_from_env(SIMCAR_Interpreter *inter,
                                SIMCAR_LocalEnvironment *env, char *name)
{
    GlobalVariableRef *pos;

    if (env == NULL) {
        return crb_search_global_variable(inter, name);
    }

    for (pos = env->global_variable; pos; pos = pos->next) {
        if (!strcmp(pos->variable->name, name)) {
            return pos->variable;
        }
    }

    return NULL;
}

static void
eval_identifier_expression(SIMCAR_Interpreter *inter,
                           SIMCAR_LocalEnvironment *env, Expression *expr)
{
    SIMCAR_Value   v;
    Variable    *vp;

    vp = crb_search_local_variable(env, expr->u.identifier);
    if (vp != NULL) {
        v = vp->value;
    } else {
        vp = search_global_variable_from_env(inter, env, expr->u.identifier);
        if (vp != NULL) {
            v = vp->value;
        } else {
            crb_runtime_error(expr->line_number, VARIABLE_NOT_FOUND_ERR,
                              STRING_MESSAGE_ARGUMENT,
                              "name", expr->u.identifier,
                              MESSAGE_ARGUMENT_END);
        }
    }
    push_value(inter, &v);
}

static void eval_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                            Expression *expr);

static SIMCAR_Value *
get_identifier_lvalue(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                      char *identifier)
{
    Variable *new_var;
    Variable *left;

    left = crb_search_local_variable(env, identifier);
    if (left == NULL) {
        left = search_global_variable_from_env(inter, env, identifier);
    }
    if (left != NULL)
        return &left->value;

    if (env != NULL) {
        new_var = crb_add_local_variable(env, identifier);
        left = new_var;
    } else {
        new_var = crb_add_global_variable(inter, identifier);
        left = new_var;
    }

    return &left->value;
}


SIMCAR_Value *
get_array_element_lvalue(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                         Expression *expr)
{
    SIMCAR_Value   array;
    SIMCAR_Value   index;

    eval_expression(inter, env, expr->u.index_expression.array);
    eval_expression(inter, env, expr->u.index_expression.index);
    index = pop_value(inter);
    array = pop_value(inter);

    if (array.type != SIMCAR_ARRAY_VALUE) {
        crb_runtime_error(expr->line_number, INDEX_OPERAND_NOT_ARRAY_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    if (index.type != SIMCAR_INT_VALUE) {
        crb_runtime_error(expr->line_number, INDEX_OPERAND_NOT_INT_ERR,
                          MESSAGE_ARGUMENT_END);
    }

    if (index.u.int_value < 0
        || index.u.int_value >= array.u.object->u.array.size) {
        crb_runtime_error(expr->line_number, ARRAY_INDEX_OUT_OF_BOUNDS_ERR,
                          INT_MESSAGE_ARGUMENT,
                          "size", array.u.object->u.array.size,
                          INT_MESSAGE_ARGUMENT, "index", index.u.int_value,
                          MESSAGE_ARGUMENT_END);
    }
    return &array.u.object->u.array.array[index.u.int_value];
}

SIMCAR_Value *
get_lvalue(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
           Expression *expr)
{
    SIMCAR_Value   *dest;

    if (expr->type == IDENTIFIER_EXPRESSION) {
        dest = get_identifier_lvalue(inter, env, expr->u.identifier);
    } else if (expr->type == INDEX_EXPRESSION) {
        dest = get_array_element_lvalue(inter, env, expr);
    } else {
        crb_runtime_error(expr->line_number, NOT_LVALUE_ERR,
                          MESSAGE_ARGUMENT_END);
    }

    return dest;
}


static void
eval_assign_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                       Expression *left, Expression *expression)
{
    SIMCAR_Value   *src;
    SIMCAR_Value   *dest;

    eval_expression(inter, env, expression);
    src = peek_stack(inter, 0);

    dest = get_lvalue(inter, env, left);
    *dest = *src;
}


static SIMCAR_Boolean
eval_binary_boolean(SIMCAR_Interpreter *inter, ExpressionType operato,
                    SIMCAR_Boolean left, SIMCAR_Boolean right, int line_number)
{
    SIMCAR_Boolean result;

    if (operato == EQ_EXPRESSION) {
        result = left == right;
    } else if (operato == NE_EXPRESSION) {
        result = left != right;
    } else {
        char *op_str = crb_get_operator_string(operato);
        crb_runtime_error(line_number, NOT_BOOLEAN_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

static void
eval_binary_int(SIMCAR_Interpreter *inter, ExpressionType operato,
                int left, int right,
                SIMCAR_Value *result, int line_number)
{
    if (dkc_is_math_operator(operato)) {
        result->type = SIMCAR_INT_VALUE;
    } else if (dkc_is_compare_operator(operato)) {
        result->type = SIMCAR_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operato));
    }

    switch (operato) {
    case BOOLEAN_EXPRESSION:    /* FALLTHRU */
    case INT_EXPRESSION:        /* FALLTHRU */
    case DOUBLE_EXPRESSION:     /* FALLTHRU */
    case STRING_EXPRESSION:     /* FALLTHRU */
    case IDENTIFIER_EXPRESSION: /* FALLTHRU */
    case ASSIGN_EXPRESSION:
        DBG_panic(("bad case...%d", operato));
        break;
    case ADD_EXPRESSION:
        result->u.int_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.int_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.int_value = left * right;
        break;
    case DIV_EXPRESSION:
        result->u.int_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.int_value = left % right;
        break;
    case LOGICAL_AND_EXPRESSION:        /* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        DBG_panic(("bad case...%d", operato));
        break;
    case EQ_EXPRESSION:
        result->u.boolean_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.boolean_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.boolean_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.boolean_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.boolean_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.boolean_value = left <= right;
        break;
    case MINUS_EXPRESSION:              /* FALLTHRU */
    case FUNCTION_CALL_EXPRESSION:      /* FALLTHRU */
    case METHOD_CALL_EXPRESSION:        /* FALLTHRU */
    case NULL_EXPRESSION:       /* FALLTHRU */
    case ARRAY_EXPRESSION:      /* FALLTHRU */
    case INDEX_EXPRESSION:      /* FALLTHRU */
    case INCREMENT_EXPRESSION:  /* FALLTHRU */
    case DECREMENT_EXPRESSION:  /* FALLTHRU */
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad case...%d", operato));
    }
}

static void
eval_binary_double(SIMCAR_Interpreter *inter, ExpressionType operato,
                   double left, double right,
                   SIMCAR_Value *result, int line_number)
{
    if (dkc_is_math_operator(operato)) {
        result->type = SIMCAR_DOUBLE_VALUE;
    } else if (dkc_is_compare_operator(operato)) {
        result->type = SIMCAR_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operato));
    }

    switch (operato) {
    case BOOLEAN_EXPRESSION:    /* FALLTHRU */
    case INT_EXPRESSION:        /* FALLTHRU */
    case DOUBLE_EXPRESSION:     /* FALLTHRU */
    case STRING_EXPRESSION:     /* FALLTHRU */
    case IDENTIFIER_EXPRESSION: /* FALLTHRU */
    case ASSIGN_EXPRESSION:
        DBG_panic(("bad case...%d", operato));
        break;
    case ADD_EXPRESSION:
        result->u.double_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.double_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.double_value = left * right;
        break;
    case DIV_EXPRESSION:
        result->u.double_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.double_value = fmod(left, right);
        break;
    case LOGICAL_AND_EXPRESSION:        /* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        DBG_panic(("bad case...%d", operato));
        break;
    case EQ_EXPRESSION:
        result->u.int_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.int_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.int_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.int_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.int_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.int_value = left <= right;
        break;
    case MINUS_EXPRESSION:              /* FALLTHRU */
    case FUNCTION_CALL_EXPRESSION:      /* FALLTHRU */
    case METHOD_CALL_EXPRESSION:        /* FALLTHRU */
    case NULL_EXPRESSION:               /* FALLTHRU */
    case ARRAY_EXPRESSION:      /* FALLTHRU */
    case INDEX_EXPRESSION:      /* FALLTHRU */
    case INCREMENT_EXPRESSION:
    case DECREMENT_EXPRESSION:
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad default...%d", operato));
    }
}

static SIMCAR_Boolean
eval_compare_string(ExpressionType operato,
                    SIMCAR_Value *left, SIMCAR_Value *right, int line_number)
{
    SIMCAR_Boolean result;
    int cmp;

    cmp = strcmp(left->u.object->u.string.string,
                 right->u.object->u.string.string);

    if (operato == EQ_EXPRESSION) {
        result = (cmp == 0);
    } else if (operato == NE_EXPRESSION) {
        result = (cmp != 0);
    } else if (operato == GT_EXPRESSION) {
        result = (cmp > 0);
    } else if (operato == GE_EXPRESSION) {
        result = (cmp >= 0);
    } else if (operato == LT_EXPRESSION) {
        result = (cmp < 0);
    } else if (operato == LE_EXPRESSION) {
        result = (cmp <= 0);
    } else {
        char *op_str = crb_get_operator_string(operato);
        crb_runtime_error(line_number, BAD_OPERATOR_FOR_STRING_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

static SIMCAR_Boolean
eval_binary_null(SIMCAR_Interpreter *inter, ExpressionType operato,
                 SIMCAR_Value *left, SIMCAR_Value *right, int line_number)
{
    SIMCAR_Boolean result;

    if (operato == EQ_EXPRESSION) {
        result = left->type == SIMCAR_NULL_VALUE && right->type == SIMCAR_NULL_VALUE;
    } else if (operato == NE_EXPRESSION) {
        result =  !(left->type == SIMCAR_NULL_VALUE
                    && right->type == SIMCAR_NULL_VALUE);
    } else {
        char *op_str = crb_get_operator_string(operato);
        crb_runtime_error(line_number, NOT_NULL_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }

    return result;
}

void
chain_string(SIMCAR_Interpreter *inter, SIMCAR_Value *left, SIMCAR_Value *right,
             SIMCAR_Value *result)
{
    char        *right_str;
    SIMCAR_Object *right_obj;
    int         len;
    char        *str;

    right_str = SIMCAR_value_to_string(right);
    right_obj = crb_create_crowbar_string_i(inter, right_str);

    result->type = SIMCAR_STRING_VALUE;
    len = strlen(left->u.object->u.string.string)
        + strlen(right_obj->u.string.string);
    str = MEM_malloc(len + 1);
    strcpy(str, left->u.object->u.string.string);
    strcat(str, right_obj->u.string.string);
    result->u.object = crb_create_crowbar_string_i(inter, str);
}

static void
eval_binary_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                       ExpressionType operato,
                       Expression *left, Expression *right)
{
    SIMCAR_Value   *left_val;
    SIMCAR_Value   *right_val;
    SIMCAR_Value   result;

    eval_expression(inter, env, left);
    eval_expression(inter, env, right);
    left_val = peek_stack(inter, 1);
    right_val = peek_stack(inter, 0);

    if (left_val->type == SIMCAR_INT_VALUE
        && right_val->type == SIMCAR_INT_VALUE) {
        eval_binary_int(inter, operato,
                        left_val->u.int_value, right_val->u.int_value,
                        &result, left->line_number);
    } else if (left_val->type == SIMCAR_DOUBLE_VALUE
               && right_val->type == SIMCAR_DOUBLE_VALUE) {
        eval_binary_double(inter, operato,
                           left_val->u.double_value, right_val->u.double_value,
                           &result, left->line_number);
    } else if (left_val->type == SIMCAR_INT_VALUE
               && right_val->type == SIMCAR_DOUBLE_VALUE) {
        eval_binary_double(inter, operato,
                           (double)left_val->u.int_value,
                           right_val->u.double_value,
                           &result, left->line_number);
    } else if (left_val->type == SIMCAR_DOUBLE_VALUE
               && right_val->type == SIMCAR_INT_VALUE) {
        eval_binary_double(inter, operato,
                           left_val->u.double_value,
                           (double)right_val->u.int_value,
                           &result, left->line_number);
    } else if (left_val->type == SIMCAR_BOOLEAN_VALUE
               && right_val->type == SIMCAR_BOOLEAN_VALUE) {
        result.type = SIMCAR_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_binary_boolean(inter, operato,
                                  left_val->u.boolean_value,
                                  right_val->u.boolean_value,
                                  left->line_number);
    } else if (left_val->type == SIMCAR_STRING_VALUE
               && operato == ADD_EXPRESSION) {
        chain_string(inter, left_val, right_val, &result);
    } else if (left_val->type == SIMCAR_STRING_VALUE
               && right_val->type == SIMCAR_STRING_VALUE) {
        result.type = SIMCAR_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_compare_string(operato, left_val, right_val,
                                  left->line_number);
    } else if (left_val->type == SIMCAR_NULL_VALUE
               || right_val->type == SIMCAR_NULL_VALUE) {
        result.type = SIMCAR_BOOLEAN_VALUE;
        result.u.boolean_value
            = eval_binary_null(inter, operato, left_val, right_val,
                               left->line_number);
    } else {
        char *op_str = crb_get_operator_string(operato);
        crb_runtime_error(left->line_number, BAD_OPERAND_TYPE_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str,
                          MESSAGE_ARGUMENT_END);
    }
    pop_value(inter);
    pop_value(inter);
    push_value(inter, &result);
}

SIMCAR_Value
crb_eval_binary_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                           ExpressionType operato,
                           Expression *left, Expression *right)
{
    eval_binary_expression(inter, env, operato, left, right);
    return pop_value(inter);
}

static void
eval_logical_and_or_expression(SIMCAR_Interpreter *inter,
                               SIMCAR_LocalEnvironment *env,
                               ExpressionType operato,
                               Expression *left, Expression *right)
{
    SIMCAR_Value   left_val;
    SIMCAR_Value   right_val;
    SIMCAR_Value   result;

    result.type = SIMCAR_BOOLEAN_VALUE;
    eval_expression(inter, env, left);
    left_val = pop_value(inter);
    if (left_val.type != SIMCAR_BOOLEAN_VALUE) {
        crb_runtime_error(left->line_number, NOT_BOOLEAN_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    if (operato == LOGICAL_AND_EXPRESSION) {
        if (!left_val.u.boolean_value) {
            result.u.boolean_value = SIMCAR_FALSE;
            goto FUNC_END;
        }
    } else if (operato == LOGICAL_OR_EXPRESSION) {
        if (left_val.u.boolean_value) {
            result.u.boolean_value = SIMCAR_TRUE;
            goto FUNC_END;
        }
    } else {
        DBG_panic(("bad operator..%d\n", operato));
    }

    eval_expression(inter, env, right);
    right_val = pop_value(inter);
    result.u.boolean_value = right_val.u.boolean_value;

  FUNC_END:
    push_value(inter, &result);
}

static void
eval_minus_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                      Expression *operand)
{
    SIMCAR_Value   operand_val;
    SIMCAR_Value   result;

    eval_expression(inter, env, operand);
    operand_val = pop_value(inter);
    if (operand_val.type == SIMCAR_INT_VALUE) {
        result.type = SIMCAR_INT_VALUE;
        result.u.int_value = -operand_val.u.int_value;
    } else if (operand_val.type == SIMCAR_DOUBLE_VALUE) {
        result.type = SIMCAR_DOUBLE_VALUE;
        result.u.double_value = -operand_val.u.double_value;
    } else {
        crb_runtime_error(operand->line_number, MINUS_OPERAND_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }

    push_value(inter, &result);
}

SIMCAR_Value
crb_eval_minus_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                          Expression *operand)
{
    eval_minus_expression(inter, env, operand);
    return pop_value(inter);
}

static SIMCAR_LocalEnvironment *
alloc_local_environment(SIMCAR_Interpreter *inter)
{
    SIMCAR_LocalEnvironment *ret;

    ret = MEM_malloc(sizeof(SIMCAR_LocalEnvironment));
    ret->variable = NULL;
    ret->global_variable = NULL;
    ret->ref_in_native_method = NULL;

    ret->next = inter->top_environment;
    inter->top_environment = ret;

    return ret;
}

static void
dispose_ref_in_native_method(SIMCAR_LocalEnvironment *env)
{
    RefInNativeFunc     *ref;

    while (env->ref_in_native_method) {
        ref = env->ref_in_native_method;
        env->ref_in_native_method = ref->next;
        MEM_free(ref);
    }
}

static void
dispose_local_environment(SIMCAR_Interpreter *inter)
{
    SIMCAR_LocalEnvironment *env = inter->top_environment;

    while (env->variable) {
        Variable        *temp;
        temp = env->variable;
        env->variable = temp->next;
        MEM_free(temp);
    }
    while (env->global_variable) {
        GlobalVariableRef *ref;
        ref = env->global_variable;
        env->global_variable = ref->next;
        MEM_free(ref);
    }
    dispose_ref_in_native_method(env);

    inter->top_environment = env->next;
    MEM_free(env);
}

static void
call_native_function(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                     SIMCAR_LocalEnvironment *caller_env,
                     Expression *expr, SIMCAR_NativeFunctionProc *proc)
{
    SIMCAR_Value   value;
    int         arg_count;
    ArgumentList        *arg_p;
    SIMCAR_Value   *args;

    for (arg_count = 0, arg_p = expr->u.function_call_expression.argument;
         arg_p; arg_p = arg_p->next) {
        eval_expression(inter, caller_env, arg_p->expression);
        arg_count++;
    }
    args = &inter->stack.stack[inter->stack.stack_pointer-arg_count];
    value = proc(inter, env, arg_count, args);
    shrink_stack(inter, arg_count);

    push_value(inter, &value);
}

static void
call_crowbar_function(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                      SIMCAR_LocalEnvironment *caller_env,
                      Expression *expr, FunctionDefinition *func)
{
    SIMCAR_Value   value;
    StatementResult     result;
    ArgumentList        *arg_p;
    ParameterList       *param_p;


    for (arg_p = expr->u.function_call_expression.argument,
             param_p = func->u.crowbar_f.parameter;
         arg_p;
         arg_p = arg_p->next, param_p = param_p->next) {
        Variable *new_var;
        SIMCAR_Value arg_val;

         if (param_p == NULL) {
             crb_runtime_error(expr->line_number, ARGUMENT_TOO_MANY_ERR,
                               MESSAGE_ARGUMENT_END);
         }
         eval_expression(inter, caller_env, arg_p->expression);
         arg_val = pop_value(inter);
         new_var = crb_add_local_variable(env, param_p->name);
         new_var->value = arg_val;
    }
     if (param_p) {
         crb_runtime_error(expr->line_number, ARGUMENT_TOO_FEW_ERR,
                           MESSAGE_ARGUMENT_END);
     }
     result = crb_execute_statement_list(inter, env,
                                         func->u.crowbar_f.block
                                         ->statement_list);
     if (result.type == RETURN_STATEMENT_RESULT) {
         value = result.u.return_value;
     } else {
         value.type = SIMCAR_NULL_VALUE;
     }

     push_value(inter, &value);
}

static void
eval_function_call_expression(SIMCAR_Interpreter *inter,
                              SIMCAR_LocalEnvironment *env,
                              Expression *expr)
{
    FunctionDefinition  *func;
    SIMCAR_LocalEnvironment    *local_env;

    char *identifier = expr->u.function_call_expression.identifier;

    func = crb_search_function(identifier);
    if (func == NULL) {
        crb_runtime_error(expr->line_number, FUNCTION_NOT_FOUND_ERR,
                          STRING_MESSAGE_ARGUMENT, "name", identifier,
                          MESSAGE_ARGUMENT_END);
    }

    local_env = alloc_local_environment(inter);

    switch (func->type) {
    case CROWBAR_FUNCTION_DEFINITION:
        call_crowbar_function(inter, local_env, env, expr, func);
        break;
    case NATIVE_FUNCTION_DEFINITION:
        call_native_function(inter, local_env, env, expr,
                             func->u.native_f.proc);
        break;
    case FUNCTION_DEFINITION_TYPE_COUNT_PLUS_1:
    default:
        DBG_panic(("bad case..%d\n", func->type));
    }
    dispose_local_environment(inter);
}

static void
check_method_argument_count(int line_number,
                            ArgumentList *arg_list, int arg_count)
{
    ArgumentList        *arg_p;
    int count = 0;

    for (arg_p = arg_list; arg_p; arg_p = arg_p->next) {
        count++;
    }

    if (count < arg_count) {
        crb_runtime_error(line_number, ARGUMENT_TOO_FEW_ERR,
                          MESSAGE_ARGUMENT_END);
    } else if (count > arg_count) {
        crb_runtime_error(line_number, ARGUMENT_TOO_MANY_ERR,
                          MESSAGE_ARGUMENT_END);
    }
}

static void
eval_method_call_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                            Expression *expr)
{
    SIMCAR_Value *left;
    SIMCAR_Value result;
    SIMCAR_Boolean error_flag = SIMCAR_FALSE;

    eval_expression(inter, env, expr->u.method_call_expression.expression);
    left = peek_stack(inter, 0);

    if (left->type == SIMCAR_ARRAY_VALUE) {
        if (!strcmp(expr->u.method_call_expression.identifier, "add")) {
            SIMCAR_Value *add;
            check_method_argument_count(expr->line_number,
                                        expr->u.method_call_expression
                                        .argument, 1);
            eval_expression(inter, env,
                            expr->u.method_call_expression.argument
                            ->expression);
            add = peek_stack(inter, 0);
            crb_array_add(inter, left->u.object, *add);
            pop_value(inter);
            result.type = SIMCAR_NULL_VALUE;
        } else if (!strcmp(expr->u.method_call_expression.identifier,
                           "size")) {
            check_method_argument_count(expr->line_number,
                                        expr->u.method_call_expression
                                        .argument, 0);
            result.type = SIMCAR_INT_VALUE;
            result.u.int_value = left->u.object->u.array.size;
        } else if (!strcmp(expr->u.method_call_expression.identifier,
                           "resize")) {
            SIMCAR_Value new_size;
            check_method_argument_count(expr->line_number,
                                        expr->u.method_call_expression
                                        .argument, 1);
            eval_expression(inter, env,
                            expr->u.method_call_expression.argument
                            ->expression);
            new_size = pop_value(inter);
            if (new_size.type != SIMCAR_INT_VALUE) {
                crb_runtime_error(expr->line_number,
                                  ARRAY_RESIZE_ARGUMENT_ERR,
                                  MESSAGE_ARGUMENT_END);
            }
            crb_array_resize(inter, left->u.object, new_size.u.int_value);
            result.type = SIMCAR_NULL_VALUE;
        } else {
            error_flag = SIMCAR_TRUE;
        }

    } else if (left->type == SIMCAR_STRING_VALUE) {
        if (!strcmp(expr->u.method_call_expression.identifier, "length")) {
            check_method_argument_count(expr->line_number,
                                        expr->u.method_call_expression
                                        .argument, 0);
            result.type = SIMCAR_INT_VALUE;
            result.u.int_value = strlen(left->u.object->u.string.string);
        } else {
            error_flag = SIMCAR_TRUE;
        }
    } else {
        error_flag = SIMCAR_TRUE;
    }
    if (error_flag) {
        crb_runtime_error(expr->line_number, NO_SUCH_METHOD_ERR,
                          STRING_MESSAGE_ARGUMENT, "method_name",
                          expr->u.method_call_expression.identifier,
                          MESSAGE_ARGUMENT_END);
    }
    pop_value(inter);
    push_value(inter, &result);
}

static void
eval_array_expression(SIMCAR_Interpreter *inter,
                      SIMCAR_LocalEnvironment *env, ExpressionList *list)
{
    SIMCAR_Value   v;
    int         size;
    ExpressionList *pos;
    int         i;

    size = 0;
    for (pos = list; pos; pos = pos->next) {
        size++;
    }
    v.type = SIMCAR_ARRAY_VALUE;
    v.u.object = crb_create_array_i(inter, size);
    push_value(inter, &v);

    for (pos = list, i = 0; pos; pos = pos->next, i++) {
        eval_expression(inter, env, pos->expression);
        v.u.object->u.array.array[i] = pop_value(inter);
    }

}

static void
eval_index_expression(SIMCAR_Interpreter *inter,
                      SIMCAR_LocalEnvironment *env, Expression *expr)
{
    SIMCAR_Value *left;

    left = get_array_element_lvalue(inter, env, expr);

    push_value(inter, left);
}

static void
eval_inc_dec_expression(SIMCAR_Interpreter *inter,
                        SIMCAR_LocalEnvironment *env, Expression *expr)
{
    SIMCAR_Value   *operand;
    SIMCAR_Value   result;
    int         old_value;
    
    operand = get_lvalue(inter, env, expr->u.inc_dec.operand);
    if (operand->type != SIMCAR_INT_VALUE) {
        crb_runtime_error(expr->line_number, INC_DEC_OPERAND_TYPE_ERR,
                          MESSAGE_ARGUMENT_END);
    }
    old_value = operand->u.int_value;
    if (expr->type == INCREMENT_EXPRESSION) {
        operand->u.int_value++;
    } else {
        DBG_assert(expr->type == DECREMENT_EXPRESSION,
                   ("expr->type..%d\n", expr->type));
        operand->u.int_value--;
    }

    result.type = SIMCAR_INT_VALUE;
    result.u.int_value = old_value;
    push_value(inter, &result);
}

static void
eval_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                Expression *expr)
{
    switch (expr->type) {
    case BOOLEAN_EXPRESSION:
        eval_boolean_expression(inter, expr->u.boolean_value);
        break;
    case INT_EXPRESSION:
        eval_int_expression(inter, expr->u.int_value);
        break;
    case DOUBLE_EXPRESSION:
        eval_double_expression(inter, expr->u.double_value);
        break;
    case STRING_EXPRESSION:
        eval_string_expression(inter, expr->u.string_value);
        break;
    case IDENTIFIER_EXPRESSION:
        eval_identifier_expression(inter, env, expr);
        break;
    case ASSIGN_EXPRESSION:
        eval_assign_expression(inter, env,
                               expr->u.assign_expression.left,
                               expr->u.assign_expression.operand);
        break;
    case ADD_EXPRESSION:        /* FALLTHRU */
    case SUB_EXPRESSION:        /* FALLTHRU */
    case MUL_EXPRESSION:        /* FALLTHRU */
    case DIV_EXPRESSION:        /* FALLTHRU */
    case MOD_EXPRESSION:        /* FALLTHRU */
    case EQ_EXPRESSION: /* FALLTHRU */
    case NE_EXPRESSION: /* FALLTHRU */
    case GT_EXPRESSION: /* FALLTHRU */
    case GE_EXPRESSION: /* FALLTHRU */
    case LT_EXPRESSION: /* FALLTHRU */
    case LE_EXPRESSION: /* FALLTHRU */
        eval_binary_expression(inter, env, expr->type,
                               expr->u.binary_expression.left,
                               expr->u.binary_expression.right);
        break;
    case LOGICAL_AND_EXPRESSION:/* FALLTHRU */
    case LOGICAL_OR_EXPRESSION:
        eval_logical_and_or_expression(inter, env, expr->type,
                                       expr->u.binary_expression.left,
                                       expr->u.binary_expression.right);
        break;
    case MINUS_EXPRESSION:
        eval_minus_expression(inter, env, expr->u.minus_expression);
        break;
    case FUNCTION_CALL_EXPRESSION:
        eval_function_call_expression(inter, env, expr);
        break;
    case METHOD_CALL_EXPRESSION:
        eval_method_call_expression(inter, env, expr);
        break;
    case NULL_EXPRESSION:
        eval_null_expression(inter);
        break;
    case ARRAY_EXPRESSION:
        eval_array_expression(inter, env, expr->u.array_literal);
        break;
    case INDEX_EXPRESSION:
        eval_index_expression(inter, env, expr);
        break;
    case INCREMENT_EXPRESSION:  /* FALLTHRU */
    case DECREMENT_EXPRESSION:
        eval_inc_dec_expression(inter, env, expr);
        break;
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHRU */
    default:
        DBG_panic(("bad case. type..%d\n", expr->type));
    }
}

SIMCAR_Value
crb_eval_expression(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                    Expression *expr)
{
    eval_expression(inter, env, expr);
    return pop_value(inter);
}

void
SIMCAR_shrink_stack(SIMCAR_Interpreter *inter, int shrink_size)
{
    shrink_stack(inter, shrink_size);
}
