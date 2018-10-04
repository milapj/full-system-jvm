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

#include <class.h>
#include <constants.h>
#include <hashtable.h>
#include <stack.h>
#include <thread.h>
#include <bc_interp.h>

#include <arch/x64-linux/bootstrap_loader.h>

struct nk_hashtable * class_map;
extern jthread_t * cur_thread;


struct nk_hashtable *
hb_get_classmap (void)
{
	return class_map;
}


/* 
 * Gets a string from the constant pool given
 * an index into the pool and an associated class.
 * If the string is not resolved, we resolve it
 * so we don't have to do the lookup again.
 *
 * @return: The string on success, NULL otherwise.
 */
const char * 
hb_get_const_str (u2 idx, java_class_t * cls)
{
	CONSTANT_Utf8_info_t * u;

	if (idx > cls->const_pool_count) {
		return NULL;
	}

	if (IS_RESOLVED(cls->const_pool[idx])) {
		return (char*)MASK_RESOLVED_BIT(cls->const_pool[idx]);
	} 

	if (cls->const_pool[idx]->tag != CONSTANT_Utf8) {
		HB_ERR("Non-UTF8 constant in %s (type = %d)\n", __func__,
			cls->const_pool[idx]->tag);
		return NULL;
	}

	u = (CONSTANT_Utf8_info_t*)cls->const_pool[idx];

	cls->const_pool[idx] = (const_pool_info_t*)MARK_RESOLVED(u->bytes);

	return (char*)u->bytes;
}


/*
 * Gets a name of the given class
 *
 * @return: a string representing the name on success,
 * NULL otherwise
 *
 */
const char *
hb_get_class_name (java_class_t * cls)
{
	if (cls) {
		return cls->name;
	}

	return NULL;
}


/*
 * Gets the name of the superclass of the given
 * class. 
 *
 * @return: the super class name on success, NULL otherwise.
 *
 */
const char *
hb_get_super_class_nm (java_class_t * cls)
{
	java_class_t * super = NULL;

	if (!cls) {
		HB_ERR("NULL class given in %s\n", __func__);
		return NULL;
	}

	super = hb_get_super_class(cls);
	
	if (super) {
		return hb_get_class_name(super);
	}

	return NULL;
}


/*
 * Returns the pointer representation of the
 * superclass of the given class.
 *
 * @return: A pointer to the super class structure
 * on success, NULL otherwise.
 *
 */
java_class_t * 
hb_get_super_class (java_class_t * cls)
{
	java_class_t * super = NULL;

	if (!cls) {
		HB_ERR("NULL class given in %s\n", __func__);
		return NULL;
	}

	if (IS_RESOLVED(cls->const_pool[cls->super])) {
		super = (java_class_t*)MASK_RESOLVED_BIT(cls->const_pool[cls->super]);
	} else {
		super = hb_resolve_class(cls->super, cls);
	}
	
	return super;
}


/*
 * Gives the number of instance vars for this object. 
 * Note this recursively applies to superclasses.
 *
 */
int 
hb_get_obj_field_count (java_class_t * cls)
{
	java_class_t * super = NULL;
	int mfc = cls->fields_count;
	
	super = hb_get_super_class(cls);
	if (super) {
		mfc += hb_get_obj_field_count(super);
	}

	return mfc;
}


/*
 * Walks backwords in this object's class hierarchy
 * to initialize the instance variables for the object.
 * Sets up the fields in such a way that the fields
 * for the most elder class will appear first.
 *
 * @return: 0 on succes, -1 otherwise.
 *
 */
int
hb_setup_obj_fields (native_obj_t * obj, java_class_t * cls)
{
	int fc = 0;
	int start_idx;
	int i;
	java_class_t * super = NULL;
	
	super = hb_get_super_class(cls);

	// recur
	if (super) {
		start_idx = hb_setup_obj_fields(obj, super);
	} else {
		start_idx = 0;
	}
	
	fc = cls->fields_count + start_idx;

	// we only setup fields for this class
	for (i = start_idx; i < fc; i++) {
		var_t x;
		x.long_val = 0;
		obj->fields[i]      = x;
		obj->field_infos[i] = &cls->fields[i-start_idx];
	}
	
	return fc;
}


/*
 * Gets the method index in the given class
 * for the method with the given name.
 *
 * @return: an index into the method array for the
 * current class if method exists, otherwise -1
 *
 * KCH NOTE: this will only work for non-polymorphic functions!
 *
 * KCH TODO: should this be applied recursively? 
 *
 */
int
hb_get_method_idx (const char * name, java_class_t * cls)
{
	int i;

	for (i = 0; i < cls->methods_count; i++) {
		if (strcmp(name, hb_get_const_str(cls->methods[i].name_idx, cls)) == 0) {
			return i;
		}
	}

	return -1;
}


/*
 * Resolves a symbolic reference to a class.
 * See Orcale JVM spec 5.4.3.1
 *
 * This will replace a symbolic reference with
 * a direct reference to the class.
 *
 * @return: a pointer to a class structure if we
 * can resolve, NULL otherwise
 *
 * KCH TODO: access control
 * 
 */
// WRITE ME
java_class_t * 
hb_resolve_class (u2 const_idx, java_class_t * src_cls)
{
	HB_ERR("%s UNIMPLEMENTED\n", __func__);
	return NULL;
}


/*
 * Gets a pointer to the method info structure
 * for constructor of this class. 
 *
 * @return: the method info struct pointer, NULL otherwise.
 *
 */
method_info_t * 
hb_get_ctor_minfo (java_class_t * cls)
{
	int i;

	for (i = 0; i < cls->methods_count; i++) {
		u2 nidx = cls->methods[i].name_idx;
		const char * tnm = hb_get_const_str(nidx, cls);
		if (strcmp(tnm, "<init>") == 0) {
			return &cls->methods[i];
		}
	
	}

	return NULL;
}


/*
 * Resolves a method given the methods name and 
 * descriptor. Useful for looking for methods in class
 * that is not the current class.
 *
 * @return: the method info struct of the resolved method,
 * NULL otherwise
 *
 */
method_info_t *
hb_find_method_by_desc (const char * mname,
			   const char * mdesc,
			   java_class_t * cls)
{
	method_info_t * ret = NULL;
	int i;

	// TODO: this should throw an IncompatibleClassChangeError excp
	if (hb_is_interface(cls)) {
		HB_ERR("Attempt to find method in interface in %s\n", __func__);
		return NULL;
	}

	// TODO: should check if this is a sig. polymorphic method
	for (i = 0; i < cls->methods_count; i++) {
		u2 nidx = cls->methods[i].name_idx;
		u2 didx = cls->methods[i].desc_idx;
		const char * tnm = hb_get_const_str(nidx, cls);
		const char * tds = hb_get_const_str(didx, cls);
		if (strcmp(tnm, mname) == 0 && strcmp(tds, mdesc) == 0) {
			ret = &cls->methods[i];
			break;
		}
	}

	// recursive search
	if (!ret) {
		java_class_t * super = hb_get_super_class(cls);
		if (super) {
			ret = hb_find_method_by_desc(mname, mdesc, super);
		}
	}
			
	// TODO: also check superinterfaces (5.4.3.3)
	if (!ret) {
		HB_ERR("Could not find method ref (looked in %s)\n", hb_get_class_name(cls));
	}
	
	return ret;
}

/*
 * Looks for a matching method ref in the target class C (recursively):
 * 	Description: Oracle JVM spec 5.4.3.3 (Method Resolution)
 * 	
 * TODO: should throw exceptions!
 *
 * @return: a method info struct if this method can be 
 * resolved, NULL otherwise.
 *
 */
// WRITE ME
method_info_t *
hb_resolve_method (u2 const_idx,
		   java_class_t * src_cls,
		   java_class_t * target_cls)
		       
{
	HB_ERR("%s UNIMPLEMENTED\n", __func__);
	return NULL;
}
		       

/* 
 * looks for a matching field in class C from class D (which contains
 * the field referenced by the symbolic reference at the const pool
 * given by const_idx. If found, the constant pool entry is
 * resolved into a direct reference.
 *
 * See JVM spec 5.4.3.2 (Field Resolution)
 *
 * KCH NOTE: NULL should be passed in for target_cls on 
 * initial (top-level) invocation
 *
 * KCH TODO: access control
 *
 * @return: a pointer to a field structure if we can 
 * resolve the field, NULL otherwise.
 *
 */
field_info_t *
hb_find_field (u2 const_idx, 
	       java_class_t * src_cls, 
	       java_class_t * target_cls)
{
	CONSTANT_Fieldref_info_t * fr = NULL;
	CONSTANT_NameAndType_info_t * nnt = NULL;
	field_info_t * ret = NULL;
	const char * field_nm;
	const char * field_desc;
	int i;

	fr = (CONSTANT_Fieldref_info_t*)src_cls->const_pool[const_idx];

	if (fr->tag != CONSTANT_Fieldref) {
		HB_ERR("%s attempt to use non-fieldref constant\n", __func__);
		return NULL;
	}

	// this is not a recursive lookup
	if (!target_cls) {
		target_cls = hb_resolve_class(fr->class_idx, src_cls);
	}

	// if we still don't have a target, something bad happened
	if (!target_cls) {
		HB_ERR("Could not resolve class ref in %s\n", __func__);
		return NULL;
	}

	nnt = (CONSTANT_NameAndType_info_t*)src_cls->const_pool[fr->name_and_type_idx];

	field_nm   = hb_get_const_str(nnt->name_idx, src_cls);
	field_desc = hb_get_const_str(nnt->desc_idx, src_cls);

	//HB_INFO("Looking for field %s [%s]\n", field_nm, field_desc);

	for (i = 0; i < target_cls->fields_count; i++) {
		u2 nidx = target_cls->fields[i].name_idx;
		u2 didx = target_cls->fields[i].desc_idx;
		const char * tnm = hb_get_const_str(nidx, target_cls);
		const char * tds = hb_get_const_str(didx, target_cls);

		if (strcmp(tnm, field_nm) == 0 && strcmp(tds, field_desc) == 0) {
			ret = &target_cls->fields[i];
			break;
		}
	}

	// if we still haven't found it, recursively search
	if (!ret) {
		java_class_t * super = hb_get_super_class(target_cls);
		if (super) {
			ret = hb_find_field(const_idx, src_cls, super);
		}
	} 

	return ret;
}


/*
 * Resolves a field by replacing the
 * symbolic reference in the constant pool
 * with a direct refefence
 *
 * @return: 0 on success, -1 otherwise
 *
 */
int
hb_resolve_field (field_info_t * f, 
	 	  native_obj_t * obj,
	 	  java_class_t * cls,
	 	  u2 const_idx)
{
	int i;
	u1 rslvd = 0;
	void * const_entry = NULL;
	
	// match the field to one of the object's instance vars
	for (i = 0; i < obj->field_count; i++) {
		if (obj->field_infos[i] == f) {
			rslvd = 1;
			break;
		}
	}

	if (!rslvd) {
		HB_ERR("Could not resolve field\n");
		return -1;
	}
	
	/* 
	 * we now know that this field 
         * corresponds to offset i in the object's 
         * field array, update the constant pool.
         */
	const_entry = (void*)MARK_FIELD_RESOLVED(i);

	cls->const_pool[const_idx] = (const_pool_info_t*)const_entry;
	
	return 0;
}


/* 
 * Resolves a static field by replacing
 * fixing a direct reference in the constant pool entry
 * 
 */
int
hb_resolve_static_field (field_info_t * f, 
	                 java_class_t * cls,
	                 u2 const_idx)
{
	void * const_entry = (void*)MARK_FIELD_RESOLVED(f);
	cls->const_pool[const_idx] = (const_pool_info_t*)const_entry;

	return 0;
}


/* 
 * links the static field to 
 * its predetermined value (recorded in the constant pool) 
 * 
 * @return: 0 on success, -1 otherwise
 *
 */
static int
fix_static_field_val (field_info_t * fi, java_class_t * cls, int idx)
{
	// set its initial value
	var_t var;

	switch (fi->cpe->tag) {
		case CONSTANT_String: {
			CONSTANT_String_info_t * si = (CONSTANT_String_info_t*)fi->cpe;
			var.ptr_val = (u8)hb_get_const_str(si->str_idx, cls);
			CL_DEBUG("Fixing static field string val to %s\n", (char*)var.ptr_val);
			break;
		}
		case CONSTANT_Integer: {
			CONSTANT_Integer_info_t * ii = (CONSTANT_Integer_info_t*)fi->cpe;
			var.int_val = ii->bytes;
			CL_DEBUG("Fixing static field int val to 0x%08x\n", var.int_val);

			break;
		}
		case CONSTANT_Long: {
			CONSTANT_Long_info_t * li = (CONSTANT_Long_info_t*)fi->cpe;
			var.long_val = (u8)li->lo_bytes | (((u8)li->hi_bytes) << 32);
			CL_DEBUG("Fixing static long field val to 0x%lx\n", var.long_val);
			break;
		}
		case CONSTANT_Float: {
			CONSTANT_Float_info_t * fli = (CONSTANT_Float_info_t*)fi->cpe;
			var.float_val = fli->bytes;
			CL_DEBUG("Fixing static float field val to %f\n", var.float_val);
			break;
		}
		case CONSTANT_Double: {
			CONSTANT_Double_info_t * di = (CONSTANT_Double_info_t*)fi->cpe;
			var.dbl_val = (u8)di->lo_bytes | (((u8)di->hi_bytes) << 32);
			CL_DEBUG("Fixing static double field val to %f\n", var.dbl_val);
			break;
		}
		default:
			HB_ERR("Invalid static field type (%d)\n", fi->cpe->tag);
			return -1;
	}
		
	// set the initial value
	cls->field_vals[idx] = var;

	return 0;
}


/*
 * Prepares a class by setting the initial values for any
 * static fields it may have. 
 *
 * See Oracle JVM spec 5.4.2
 *
 * @return: 0 on success, -1 otherwise
 *
 */
int
hb_prep_class (java_class_t * cls)
{
	// static fields are set to their initial values
	int i;
	
	for (i = 0; i < cls->fields_count; i++) {
		if (cls->fields[i].acc_flags & ACC_STATIC) {
			cls->fields[i].value = &cls->field_vals[i];

			if (cls->fields[i].has_const) {
				fix_static_field_val(&cls->fields[i], cls, i);
			}
		}
	}

	CL_DEBUG("Class prepped (static fields initialized)\n");

	cls->status = CLS_PREPPED;
	
	return 0;
}


/*
 * Initializes a class by invoking its class initialization
 * method (if it exists). 
 *
 * See Oracle JVM spec 5.5
 *
 * KCH NOTE: assumes the *current* class
 * is already inited!
 *
 * TODO: recursively init superclasses
 *
 * @return: 0 on success, -1 otherwise.
 *
 */
int 
hb_init_class (java_class_t * cls)
{
	int init_idx = hb_get_method_idx("<clinit>", cls);

	if (init_idx < 0) {
		CL_DEBUG("No <clinit> found, skipping class initialization\n");
		cls->status = CLS_INITED;
		return 0;
	}

	if (hb_push_frame(cur_thread,
				cls,
				init_idx) != 0) {
		HB_ERR("Could not push init method stack frame\n");
		return -1;
	}
		
	CL_DEBUG("Executing class initializer\n");

	hb_exec(cur_thread);

	cls->status = CLS_INITED;

	CL_DEBUG("Class inited (class/interface initializers invoked)\n");

	return 0;
}


static unsigned
class_map_hash_fn (unsigned long key) 
{
	char * buf = (char*)key;
	return nk_hash_buffer((unsigned char*)buf, strlen(buf));
}


static int
class_map_eq_fn (unsigned long k1, unsigned long k2)
{
	char * nm1 = (char*)k1;
	char * nm2 = (char*)k2;
	return strcmp(nm1, nm2) == 0;
}


/* 
 * Initializes the global class map (a hashtable).
 *
 * @return: 0 on success, -1 otherwise
 *
 */
int
hb_classmap_init (void)
{
	class_map = nk_create_htable(0, 
				     class_map_hash_fn,
				     class_map_eq_fn);

	if (!class_map) {
		HB_ERR("Could not create class map hashtable\n");
		return -1;
	}
	
	return 0;
}


/*
 * Adds a class to the class map (based on the 
 * class name recorded in the constant pool)
 */
void
hb_add_class (const char * class_nm, java_class_t * cls)
{
	nk_htable_insert(class_map, (unsigned long)class_nm, (unsigned long)cls);
}


/*
 * Checks if a class has been loaded into the global
 * class map.
 *
 * @return: 1 if the class has been loaded already, 0 otherwise
 *
 */
int
hb_class_is_loaded (const char * class_nm)
{
	return (void*)nk_htable_search(class_map, (unsigned long)class_nm) != NULL;
}


/*
 * Gets a class pointer from the global class map
 * based on the class name. 
 *
 * @return: the class pointer if the class has been loaded,
 * NULL otherwise
 *
 */
java_class_t * 
hb_get_class (const char * class_nm)
{
	return (java_class_t*)nk_htable_search(class_map, (unsigned long)class_nm);
}


/*
 * Gets a class pointer from the class map,
 * or if it doesn't exist, will load, prep,
 * and initialize the class.
 *
 * @return: the class pointer if successful, NULL
 * otherwise
 *
 */
java_class_t *
hb_get_or_load_class (const char * class_nm)
{
	java_class_t * cls = hb_get_class(class_nm);
	
	if (!cls) {
		CL_DEBUG("Loading new class from %s: %s\n", class_nm, __func__);
		cls = hb_load_class(class_nm);
		if (!cls) {
			HB_ERR("Couldn't load class\n");
			return NULL;
		}

		hb_add_class(class_nm, cls);

		hb_prep_class(cls);

		hb_init_class(cls);
	}
	
	return cls;
}

