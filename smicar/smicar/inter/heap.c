#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "simcar.h"

static void
check_gc(SIMCAR_Interpreter *inter)
{
#if 0
    crb_garbage_collect(inter);
#endif
    if (inter->heap.current_heap_size > inter->heap.current_threshold) {
        /* fprintf(stderr, "garbage collecting..."); */
        crb_garbage_collect(inter);
        /* fprintf(stderr, "done.\n"); */

        inter->heap.current_threshold
            = inter->heap.current_heap_size + HEAP_THRESHOLD_SIZE;
    }
}

static SIMCAR_Object *
alloc_object(SIMCAR_Interpreter *inter, ObjectType type)
{
    SIMCAR_Object *ret;

    check_gc(inter);
    ret = MEM_malloc(sizeof(SIMCAR_Object));
    inter->heap.current_heap_size += sizeof(SIMCAR_Object);
    ret->type = type;
    ret->marked = SIMCAR_FALSE;
    ret->prev = NULL;
    ret->next = inter->heap.header;
    inter->heap.header = ret;
    if (ret->next) {
        ret->next->prev = ret;
    }

    return ret;
}

static void
add_ref_in_native_method(SIMCAR_LocalEnvironment *env, SIMCAR_Object *obj)
{
    RefInNativeFunc *new_ref;

    new_ref = MEM_malloc(sizeof(RefInNativeFunc));
    new_ref->object = obj;
    new_ref->next = env->ref_in_native_method;
    env->ref_in_native_method = new_ref;
}

SIMCAR_Object *
crb_literal_to_crb_string(SIMCAR_Interpreter *inter, char *str)
{
    SIMCAR_Object *ret;

    ret = alloc_object(inter, STRING_OBJECT);
    ret->u.string.string = str;
    ret->u.string.is_literal = SIMCAR_TRUE;

    return ret;
}

SIMCAR_Object *
crb_create_crowbar_string_i(SIMCAR_Interpreter *inter, char *str)
{
    SIMCAR_Object *ret;

    ret = alloc_object(inter, STRING_OBJECT);
    ret->u.string.string = str;
    inter->heap.current_heap_size += strlen(str) + 1;
    ret->u.string.is_literal = SIMCAR_FALSE;

    return ret;
}

SIMCAR_Object *
SIMCAR_create_crowbar_string(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                          char *str)
{
    SIMCAR_Object *ret;

    ret = crb_create_crowbar_string_i(inter, str);
    add_ref_in_native_method(env, ret);

    return ret;
}

SIMCAR_Object *
crb_create_array_i(SIMCAR_Interpreter *inter, int size)
{
    SIMCAR_Object *ret;

    ret = alloc_object(inter, ARRAY_OBJECT);
    ret->u.array.size = size;
    ret->u.array.alloc_size = size;
    ret->u.array.array = MEM_malloc(sizeof(SIMCAR_Value) * size);
    inter->heap.current_heap_size += sizeof(SIMCAR_Value) * size;

    return ret;
}

SIMCAR_Object *
SIMCAR_create_array(SIMCAR_Interpreter *inter, SIMCAR_LocalEnvironment *env,
                 int size)
{
    SIMCAR_Object *ret;

    ret = crb_create_array_i(inter, size);
    add_ref_in_native_method(env, ret);

    return ret;
}

void
crb_array_add(SIMCAR_Interpreter *inter, SIMCAR_Object *obj, SIMCAR_Value v)
{
    int new_size;

    DBG_assert(obj->type == ARRAY_OBJECT, ("bad type..%d\n", obj->type));

    check_gc(inter);
    if (obj->u.array.size + 1 > obj->u.array.alloc_size) {
        new_size = obj->u.array.alloc_size * 2;
        if (new_size == 0
            || new_size - obj->u.array.alloc_size > ARRAY_ALLOC_SIZE) {
            new_size = obj->u.array.alloc_size + ARRAY_ALLOC_SIZE;
        }
        obj->u.array.array = MEM_realloc(obj->u.array.array,
                                         new_size * sizeof(SIMCAR_Value));
        inter->heap.current_heap_size
            += (new_size - obj->u.array.alloc_size) * sizeof(SIMCAR_Value);
        obj->u.array.alloc_size = new_size;
    }
    obj->u.array.array[obj->u.array.size] = v;
    obj->u.array.size++;
}

void
crb_array_resize(SIMCAR_Interpreter *inter, SIMCAR_Object *obj, int new_size)
{
    int new_alloc_size;
    SIMCAR_Boolean need_realloc;
    int i;

    check_gc(inter);
    
    if (new_size > obj->u.array.alloc_size) {
        new_alloc_size = obj->u.array.alloc_size * 2;
        if (new_alloc_size < new_size) {
            new_alloc_size = new_size + ARRAY_ALLOC_SIZE;
        } else if (new_alloc_size - obj->u.array.alloc_size
                   > ARRAY_ALLOC_SIZE) {
            new_alloc_size = obj->u.array.alloc_size + ARRAY_ALLOC_SIZE;
        }
        need_realloc = SIMCAR_TRUE;
    } else if (obj->u.array.alloc_size - new_size > ARRAY_ALLOC_SIZE) {
        new_alloc_size = new_size;
        need_realloc = SIMCAR_TRUE;
    } else {
        need_realloc = SIMCAR_FALSE;
    }
    if (need_realloc) {
        check_gc(inter);
        obj->u.array.array = MEM_realloc(obj->u.array.array,
                                         new_alloc_size * sizeof(SIMCAR_Value));
        inter->heap.current_heap_size
            += (new_alloc_size - obj->u.array.alloc_size) * sizeof(SIMCAR_Value);
        obj->u.array.alloc_size = new_alloc_size;
    }
    for (i = obj->u.array.size; i < new_size; i++) {
        obj->u.array.array[i].type = SIMCAR_NULL_VALUE;
    }
    obj->u.array.size = new_size;
}

static void
gc_mark(SIMCAR_Object *obj)
{
    if (obj->marked)
        return;

    obj->marked = SIMCAR_TRUE;

    if (obj->type == ARRAY_OBJECT) {
        int i;
        for (i = 0; i < obj->u.array.size; i++) {
            if (dkc_is_object_value(obj->u.array.array[i].type)) {
                gc_mark(obj->u.array.array[i].u.object);
            }
        }
    }
}

static void
gc_reset_mark(SIMCAR_Object *obj)
{
    obj->marked = SIMCAR_FALSE;
}

static void
gc_mark_ref_in_native_method(SIMCAR_LocalEnvironment *env)
{
    RefInNativeFunc *ref;

    for (ref = env->ref_in_native_method; ref; ref = ref->next) {
        gc_mark(ref->object);
    }
}

static void
gc_mark_objects(SIMCAR_Interpreter *inter)
{
    SIMCAR_Object *obj;
    Variable *v;
    SIMCAR_LocalEnvironment *lv;
    int i;

    for (obj = inter->heap.header; obj; obj = obj->next) {
        gc_reset_mark(obj);
    }
    
    for (v = inter->variable; v; v = v->next) {
        if (dkc_is_object_value(v->value.type)) {
            gc_mark(v->value.u.object);
        }
    }
    
    for (lv = inter->top_environment; lv; lv = lv->next) {
        for (v = lv->variable; v; v = v->next) {
            if (dkc_is_object_value(v->value.type)) {
                gc_mark(v->value.u.object);
            }
        }
        gc_mark_ref_in_native_method(lv);
    }

    for (i = 0; i < inter->stack.stack_pointer; i++) {
        if (dkc_is_object_value(inter->stack.stack[i].type)) {
            gc_mark(inter->stack.stack[i].u.object);
        }
    }
}

static void
gc_dispose_object(SIMCAR_Interpreter *inter, SIMCAR_Object *obj)
{
    switch (obj->type) {
    case ARRAY_OBJECT:
        inter->heap.current_heap_size
            -= sizeof(SIMCAR_Value) * obj->u.array.alloc_size;
        MEM_free(obj->u.array.array);
        break;
    case STRING_OBJECT:
        if (!obj->u.string.is_literal) {
            inter->heap.current_heap_size -= strlen(obj->u.string.string) + 1;
            MEM_free(obj->u.string.string);
        }
        break;
    case OBJECT_TYPE_COUNT_PLUS_1:
    default:
        DBG_assert(0, ("bad type..%d\n", obj->type));
    }
    inter->heap.current_heap_size -= sizeof(SIMCAR_Object);
    MEM_free(obj);
}

static void
gc_sweep_objects(SIMCAR_Interpreter *inter)
{
    SIMCAR_Object *obj;
    SIMCAR_Object *tmp;

    for (obj = inter->heap.header; obj; ) {
        if (!obj->marked) {
            if (obj->prev) {
                obj->prev->next = obj->next;
            } else {
                inter->heap.header = obj->next;
            }
            if (obj->next) {
                obj->next->prev = obj->prev;
            }
            tmp = obj->next;
            gc_dispose_object(inter, obj);
            obj = tmp;
        } else {
            obj = obj->next;
        }
    }
}

void
crb_garbage_collect(SIMCAR_Interpreter *inter)
{
    gc_mark_objects(inter);
    gc_sweep_objects(inter);
}
