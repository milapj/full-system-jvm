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
#ifndef __STACK_H__
#define __STACK_H__

#include <class.h>
#include <hawkbeans.h>

#if DEBUG_STACXK == 1
#define ST_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define ST_DEBUG(fmt, args...)
#endif


#define ST_INVOKE_SPECIAL 0
#define ST_INVOKE_STATIC  1
#define ST_INVOKE_VIRT    2

typedef struct op_stack {
	u4 max_oprs;
	int sp;
	// back link
	struct stack_frame * frame;

	var_t oprs[0];

} op_stack_t;

/* created when a method is invoked */
typedef struct stack_frame {

	u2 pc;

	method_info_t * minfo;

	var_t * locals; // method-specificw
	u4 max_locals;

	op_stack_t * op_stack;

	struct stack_frame * prev;
	struct stack_frame * next;

	struct jthread * owner;

	struct java_class * cls;

} stack_frame_t;


int hb_push_base_frame (struct jthread * t,
			java_class_t * cls,
			obj_ref_t * base_obj,
			u2 method_idx);

void hb_dump_op_stack (void);
void hb_dump_locals(void);
int hb_pop_frame (struct jthread * t);
int hb_push_frame_by_method (struct jthread * t, 
			     struct method_info * mi);
int hb_push_frame (struct jthread * t,
			java_class_t * cls,
			u2 method_idx);
int hb_push_ctor_frame (struct jthread * t, struct obj_ref * oref);

int hb_get_parm_count_from_method (struct jthread * t, 
				   struct method_info * mi,
				   u1 invoke_type,
				   int * nwide);
int hb_setup_method_parms (struct jthread * t,
			   struct method_info * mi,
			   u1 invoke_type);

#endif
