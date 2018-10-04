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
#include <string.h>
#include <stdlib.h>

#include <types.h>
#include <class.h>
#include <stack.h>
#include <thread.h>
#include <gc.h>


extern jthread_t * cur_thread;

/* KCH CLEANUP: considerable amount of redundancy in stack push
 * functions */


void
hb_dump_op_stack (void)
{
	op_stack_t * stack = cur_thread->cur_frame->op_stack;
	int i;
	
	HB_INFO("OP_STACK (sp=%d):\n", stack->sp);

	for (i = 0; i <= stack->sp; i++) {
		HB_INFO("  [%p]\n", (void*)stack->oprs[i].ptr_val);
	}
}


void 
hb_dump_locals (void)
{
	stack_frame_t * frame = cur_thread->cur_frame;
	int i;

	HB_INFO("LOCALS:\n");

	for (i = 0; i < frame->max_locals; i++) {
		HB_INFO("  [%p]\n", (void*)frame->locals[i].ptr_val);
	}
}


int
hb_pop_frame (jthread_t * t)
{
	stack_frame_t * frame = t->cur_frame;
	stack_frame_t * prev  = frame->prev;

	if (prev) {
		prev->next = NULL;
	}

	t->cur_frame = prev;

	free(frame->locals);
	free(frame->op_stack);
	free(frame);

	return 0;
}


/* TODO handle arrays... */
static int
get_parm_count (const char * mdesc, int * nwide)
{
	int i;
	int count = 0;
	int len = strlen(mdesc);
	
	for (i = 0; i < len; i++, mdesc++) {

		if (*mdesc == '(') continue;

		if (*mdesc == '[') continue;

		if (*mdesc == 'L') {
			while (*mdesc != ';') {
				i++;
				mdesc++;
			}
		}

		if (*mdesc == ')') { 
			break;
		}

		// double and long take up 2 positions
		if (*mdesc == 'J' || *mdesc == 'D') {
			count++;
			(*nwide)++;
		}

		count++;
	}

	return count;
}


static void
copy_locals (jthread_t * t, int nargs, const char * mdesc, u1 type)
{
	int i;
	int stackcount = 0;
	int localcount = 0;
	int len = strlen(mdesc);
	op_stack_t * prevop = t->cur_frame->prev->op_stack;

	// accomodate "this" pointer
	if (type != ST_INVOKE_STATIC) {
		t->cur_frame->locals[localcount] = prevop->oprs[prevop->sp - nargs + 1 + stackcount];
		stackcount++;
		localcount++;
	}
	
	for (i = 0; i < len; i++, mdesc++) {

		if (*mdesc == '(') continue;

		if (*mdesc == '[') continue;

		if (*mdesc == 'L') {
			while (*mdesc != ';') {
				i++;
				mdesc++;
			}
		}

		if (*mdesc == ')') { 
			break;
		}

		// double and long take up 2 positions
		if (*mdesc == 'J' || *mdesc == 'D') {
			t->cur_frame->locals[localcount].long_val = prevop->oprs[prevop->sp - nargs + 1 + stackcount].long_val & 0xffffffff;
			t->cur_frame->locals[localcount+1].long_val = prevop->oprs[prevop->sp - nargs + 1 + stackcount].long_val >> 32;
			stackcount++;
			localcount+=2;
			continue;
		}

		t->cur_frame->locals[localcount] = prevop->oprs[prevop->sp - nargs + 1 + stackcount];
		stackcount++;
		localcount++;
	}
}


int
hb_get_parm_count_from_method (jthread_t * t, 
			       method_info_t * mi,
			       u1 invoke_type,
			       int * nwide)
{
	const char * mdesc   = hb_get_const_str(mi->desc_idx, mi->owner);
	int nargs = get_parm_count(mdesc, nwide);

	if (invoke_type != ST_INVOKE_STATIC) {
		nargs++;
	}

	return nargs;
}


/*
 * This method copies parameters from the operand
 * stack of the current method onto the local var array
 * of the new (fresh) method. This function assumes
 * that the new (fresh) frame has already been pushed, 
 * so the current frame is actually the previous frame.
 *
 * If invoke_type is ST_INVOKE_STATIC, we omit the object
 * pointer at position 0
 *
 * @return: 0 on success, -1 otherwise
 *
 */
int 
hb_setup_method_parms (jthread_t * t,
		       method_info_t * mi,
		       u1 invoke_type)
{
	const char * mdesc   = hb_get_const_str(mi->desc_idx, mi->owner);
	stack_frame_t * prev = t->cur_frame->prev;
	op_stack_t * prevop  = prev->op_stack;
	int i;
	int nwide;

	int nargs = get_parm_count(mdesc, &nwide);

	ST_DEBUG("Found %d args in (%s) %s\n", nargs, mdesc, __func__);

	// object pointer
	if (invoke_type != ST_INVOKE_STATIC) {
		nargs++;
	}

	ST_DEBUG("Prev op stack (bottom to top):\n");
	for (i = 0; i < nargs; i++) {
		ST_DEBUG("  [%p]\n", (void*)prevop->oprs[prevop->sp - nargs + i + 1].long_val);
	}

	copy_locals(t, nargs, mdesc, invoke_type);

	//memcpy(t->cur_frame->locals, &prevop->oprs[prevop->sp - nargs + 1], sizeof(var_t)*nargs);

	ST_DEBUG("NEW LOCALS:\n");

	for (i = 0; i < nargs; i++) {
		ST_DEBUG("[%p]\n", (void*)t->cur_frame->locals[i].long_val);
	}
		
	// pop operands off current stack frame
	prevop->sp -= nargs;

	return 0;
}


static int
init_op_stack (stack_frame_t * frame, int max_oprs)
{
	frame->op_stack = malloc(sizeof(op_stack_t) + sizeof(var_t)*max_oprs);
	if (!frame->op_stack) {
		HB_ERR("Could not allocate operand stack\n");
		return -1;
	}

	memset(frame->op_stack, 0, sizeof(op_stack_t) + sizeof(var_t)*max_oprs);
	frame->op_stack->max_oprs = max_oprs;
	frame->op_stack->sp       = 0;
	frame->op_stack->frame    = frame;
	return 0;
}


static inline void
init_frame (stack_frame_t * frame, 
	    jthread_t * owner, 
	    java_class_t * cls,
	    method_info_t* minfo)
{
	frame->pc    = 0;
	frame->owner = owner;
	frame->cls   = cls;
	frame->minfo = minfo;
	frame->next  = NULL;
	frame->prev  = NULL;

	frame->max_locals = 0;
	frame->locals     = NULL;
}


static inline void
link_frame (stack_frame_t * frame, jthread_t * t) 
{
	if (t->cur_frame) {
		t->cur_frame->next = frame;
		frame->prev        = t->cur_frame;
	}

	t->cur_frame = frame;
}


int 
hb_push_frame_by_method (jthread_t * t,
			 method_info_t * mi)
{
	java_class_t * cls = mi->owner;
	stack_frame_t * frame = malloc(sizeof(stack_frame_t));
	int max_oprs;

	if (!frame) {
		HB_ERR("Could not allocate frame\n");
		return -1;
	}

	memset(frame, 0, sizeof(stack_frame_t));

	init_frame(frame, t, cls, mi);

	// is this not a native/abstract method?
	if (mi->code_attr) {

		frame->max_locals = mi->code_attr->max_locals;
		frame->locals     = malloc(sizeof(var_t)*frame->max_locals);

		if (!frame->locals) {
			HB_ERR("Could not allocate local vars\n");
			return -1;
		}

		memset(frame->locals, 0, sizeof(var_t)*frame->max_locals);

		max_oprs = mi->code_attr->max_stack + 1;

		if (init_op_stack(frame, max_oprs) != 0) {
			HB_ERR("Could not setup op stack in %s\n", __func__);
			return -1;
		}
	}

	link_frame(frame, t);

	return 0;
}


int
hb_push_ctor_frame (jthread_t * t,
		    obj_ref_t * oref)
{
	native_obj_t * obj = (native_obj_t*)oref->heap_ptr;
	java_class_t * cls = obj->class;
	method_info_t * mi = NULL;

	mi = hb_get_ctor_minfo(cls);
	
	if (!mi) {
		HB_ERR("Couldn't get ctor minfo\n in %s\n", __func__);
		return -1;
	}

	if (hb_push_frame_by_method(t, mi) != 0) {
		HB_ERR("Could not push ctor frame\n");
		return -1;
	}

	t->cur_frame->locals[0].obj = oref;

	return 0;
}


/* 
 * NOTE: cls is expected to be the *current* class
 */
int 
hb_push_frame (jthread_t * t, 
		java_class_t * cls, 
		u2 method_idx)
{
	method_info_t * mi = &cls->methods[method_idx];

	return hb_push_frame_by_method(t, mi);
}


int 
hb_push_base_frame (jthread_t * t, 
		    java_class_t * cls, 
		    obj_ref_t * base_obj,
		    u2 method_idx)
{
	method_info_t * mi = &cls->methods[method_idx];

	if (hb_push_frame_by_method(t, mi) != 0) {
		HB_ERR("Could not push base frame\n");
		return -1;
	}

	t->cur_frame->locals[0].obj = base_obj;
	ST_DEBUG("Pushed base frame with %d oprs\n", t->cur_frame->op_stack->max_oprs);

	return 0;
}
