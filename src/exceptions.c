/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2017, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#include <stdlib.h>
#include <string.h>
#include <types.h>
#include <class.h>
#include <stack.h>
#include <mm.h>
#include <thread.h>
#include <exceptions.h>
#include <bc_interp.h>
#include <gc.h>


extern jthread_t * cur_thread;
_Bool in_range(u2, u2, u2);
/* 
 * Check the interval
 * low is inclusive and high is exclusive
 */
_Bool
in_range(u2 low, u2 high, u2 pc){
  return ((pc-high)*(pc-low)) < 0;
}
/* 
 * Maps internal exception identifiers to fully
 * qualified class paths for the exception classes.
 * Note that the ones without fully qualified paths
 * will not be properly raised. 
 *
 * TODO: add the classes for these
 *
 */
static const char * excp_strs[16] __attribute__((used)) =
{
	"java/lang/NullPointerException",
	"java/lang/IndexOutOfBoundsException",
	"java/lang/ArrayIndexOutOfBoundsException",
	"IncompatibleClassChangeError",
	"java/lang/NegativeArraySizeException",
	"java/lang/OutOfMemoryError",
	"java/lang/ClassNotFoundException",
	"java/lang/ArithmeticException",
	"java/lang/NoSuchFieldError",
	"java/lang/NoSuchMethodError",
	"java/lang/RuntimeException",
	"java/io/IOException",
	"FileNotFoundException",
	"java/lang/InterruptedException",
	"java/lang/NumberFormatException",
	"java/lang/StringIndexOutOfBoundsException",
};




/*
 * Throws an exception given an internal ID
 * that refers to an exception type. This is to 
 * be used by the runtime (there is no existing
 * exception object, so we have to create a new one
 * and init it).
 *
 * @return: none. exits on failure.
 *
 */
// WRITE ME
void
hb_throw_and_create_excp (u1 type)
{
  java_class_t *class_of_exception = hb_get_or_load_class(excp_strs[type]);
  
  obj_ref_t *object_of_class = gc_obj_alloc(class_of_exception);
  if(hb_invoke_ctor(object_of_class)){
    HB_ERR("The constructor is failed to invoke\n");
    exit(EXIT_FAILURE);
  };
  hb_throw_exception(object_of_class);
  return;
}



/* 
 * gets the exception message from the object 
 * ref referring to the exception object.
 *
 * NOTE: caller must free the string
 *
 */
static char *
get_excp_str (obj_ref_t * eref)
{
	char * ret;
	native_obj_t * obj = (native_obj_t*)eref->heap_ptr;
		
	obj_ref_t * str_ref = obj->fields[0].obj;
	native_obj_t * str_obj;
	obj_ref_t * arr_ref;
	native_obj_t * arr_obj;
	int i;
	
	if (!str_ref) {
		return NULL;
	}

	str_obj = (native_obj_t*)str_ref->heap_ptr;
	
	arr_ref = str_obj->fields[0].obj;

	if (!arr_ref) {
		return NULL;
	}

	arr_obj = (native_obj_t*)arr_ref->heap_ptr;

	ret = malloc(arr_obj->flags.array.length+1);

	for (i = 0; i < arr_obj->flags.array.length; i++) {
		ret[i] = arr_obj->fields[i].char_val;
	}

	ret[i] = 0;

	return ret;
}


/*
 * Throws an exception using an
 * object reference to some exception object (which
 * implements Throwable). To be used with athrow.
 * If we're given a bad reference, we throw a 
 * NullPointerException.
 *
 * @return: none. exits on failure.  
 *
 */

// WRITE ME
void
hb_throw_exception (obj_ref_t * eref)
{
  native_obj_t *native_object = (native_obj_t *)(eref->heap_ptr);
  java_class_t *class_of_object = (native_object->class);
  if(!class_of_object){
    exit(EXIT_FAILURE);
  }
  const char *class_name_of_object = hb_get_class_name(class_of_object);
  // In the simple case, the class_name_of_object is ArithmeticException
  method_info_t *method_info = cur_thread->cur_frame->minfo;
  excp_table_t *exception_table = method_info->code_attr->excp_table;
  u2 exception_table_length = method_info->code_attr->excp_table_len;
  u2 i;
  HB_ERR("Exception in thread %s %s at %s\n", cur_thread->name, class_name_of_object,hb_get_class_name(cur_thread->class) );
  for(i = 0; i < exception_table_length; i++){
    u2 catch_type_index = exception_table[i].catch_type;
    CONSTANT_Class_info_t *class_of_exception_caught_by_handler = (CONSTANT_Class_info_t *)class_of_object->const_pool[catch_type_index];
    u2 name_index = class_of_exception_caught_by_handler->name_idx;
    u2 low = exception_table[i].start_pc;
    u2 high= exception_table[i].end_pc;
    u2 pc = cur_thread->cur_frame->pc;
    const char* exception_type = hb_get_const_str(name_index, class_of_object);
      if( in_range(low,high,pc) && !strcmp(exception_type, class_name_of_object)){
      var_t v;
      v.obj = eref;
      op_stack_t *stack = cur_thread->cur_frame->op_stack;
      stack->oprs[++(stack->sp)] = v;
      cur_thread->cur_frame->pc = exception_table[i].handler_pc;
      hb_exec(cur_thread);
      return;
    }
  }
  // Otherwise, pop a frame and continue searching -> How ? Recursively
  hb_pop_frame(cur_thread);
  if(!cur_thread->cur_frame){
    // If no suitable exception handler is found before the top of the method invocation chain is reached,
    // the execution of the thread in which the exception was thrown is "TERMINATED"
    return;
  }
  hb_throw_exception(eref);
}
