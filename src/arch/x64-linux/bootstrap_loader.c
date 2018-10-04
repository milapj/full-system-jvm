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
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <hawkbeans.h>
#include <types.h>
#include <constants.h>
#include <class.h>

#include <arch/x64-linux/bootstrap_loader.h>
#include <arch/x64-linux/util.h>

#define GET_AND_INC(field, sz) \
	cls->field = get_u##sz(clb); clb += sz

#define PARSE_AND_CHECK(count, func, str) \
	if (cls->count > 0) { \
		skip = 0; \
		if (func(clb, cls, &skip) != 0) { \
			HB_ERR("Could not parse %s\n", str); \
			return -1; \
		}  \
		clb += skip; \
	}


static u1*
open_class_file (const char * path)
{
	int fd;
	struct stat s;
	void * cm = NULL;
	const char * suf = ".class";
	char buf[512];
	
	// do we need to add the extension?
	if (!strstr(path, suf)) {
		memset(buf, 0, 512);
		strncpy(buf, path, 512);
		strncat(buf, suf, 7);
		path = buf;
	}
		
	if ((fd = open(path, O_RDONLY)) == -1) {
		HB_ERR("Could not open file (%s): %s\n", path, strerror(errno));
		return NULL;
	}

	if (fstat(fd, &s) == -1) {
		HB_ERR("Could not stat file (%s): %s\n", path, strerror(errno));
		return NULL;
	}
	
	if ((cm = mmap(NULL,
		 s.st_size,	
		 PROT_READ,
		 MAP_PRIVATE,
		 fd,
		 0)) == MAP_FAILED) {
		HB_ERR("Could not map file (%s): %s\n", path, strerror(errno));
		return NULL;
	}

	return (u1*)cm;
}


static const_pool_info_t * 
parse_const_pool_entry (u1 * ptr, u1 * sz)
{
	switch (*ptr) {
		case CONSTANT_Class:  {
			CONSTANT_Class_info_t * c = malloc(sizeof(CONSTANT_Class_info_t));
			c->tag      = *ptr;
			c->name_idx = get_u2(ptr+1);
			*sz = 3;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Fieldref: {
			CONSTANT_Fieldref_info_t * c = malloc(sizeof(CONSTANT_Fieldref_info_t));
			c->tag               = *ptr;
			c->class_idx         = get_u2(ptr+1);
			c->name_and_type_idx = get_u2(ptr+3);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Methodref: {
			CONSTANT_Methodref_info_t * c = malloc(sizeof(CONSTANT_Methodref_info_t));
			c->tag               = *ptr;
			c->class_idx         = get_u2(ptr+1);
			c->name_and_type_idx = get_u2(ptr+3);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_InterfaceMethodref: {
			CONSTANT_InterfaceMethodref_info_t * c = malloc(sizeof(CONSTANT_InterfaceMethodref_info_t));
			c->tag               = *ptr;
			c->class_idx         = get_u2(ptr+1);
			c->name_and_type_idx = get_u2(ptr+3);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_String: {
			CONSTANT_String_info_t * c = malloc(sizeof(CONSTANT_String_info_t));
			c->tag     = *ptr;
			c->str_idx = get_u2(ptr+1);
			*sz = 3;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Integer: {
			CONSTANT_Integer_info_t * c = malloc(sizeof(CONSTANT_Integer_info_t));
			c->tag   = *ptr;
			c->bytes = get_u4(ptr+1);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Float: {
			CONSTANT_Float_info_t * c = malloc(sizeof(CONSTANT_Float_info_t));
			c->tag   = *ptr;
			c->bytes = get_u4(ptr+1);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Long: {
			CONSTANT_Long_info_t * c = malloc(sizeof(CONSTANT_Long_info_t));
			c->tag      = *ptr;
			c->hi_bytes = get_u4(ptr+1);
			c->lo_bytes = get_u4(ptr+5);
			*sz = 9;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Double: {
			CONSTANT_Double_info_t * c = malloc(sizeof(CONSTANT_Double_info_t));
			c->tag      = *ptr;
			c->hi_bytes = get_u4(ptr+1);
			c->lo_bytes = get_u4(ptr+5);
			*sz = 9;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_NameAndType: {
			CONSTANT_NameAndType_info_t * c = malloc(sizeof(CONSTANT_NameAndType_info_t));
			c->tag      = *ptr;
			c->name_idx = get_u2(ptr+1);
			c->desc_idx = get_u2(ptr+3);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_Utf8: {
			CONSTANT_Utf8_info_t * c = NULL;
			u1 tag = *ptr;
			u2 len = get_u2(ptr+1);
			c = malloc(sizeof(CONSTANT_Utf8_info_t) + len + 1);
			c->tag = tag;
			c->len = len;
			*sz = 3 + len; 
			memset(c->bytes, 0, len + 1);
			memcpy(c->bytes, ptr+3, len);
			return (const_pool_info_t*)c;
		}
		case CONSTANT_MethodHandle: {
			CONSTANT_MethodHandle_info_t * c = malloc(sizeof(CONSTANT_MethodHandle_info_t));
			c->tag = *ptr;
			c->ref_kind = *(ptr+1);
			c->ref_idx  = get_u2(ptr+2);
			*sz = 4;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_MethodType: {
			CONSTANT_MethodType_info_t * c = malloc(sizeof(CONSTANT_MethodType_info_t));
			c->tag      = *ptr;
			c->desc_idx = get_u2(ptr+1);
			*sz = 3;
			return (const_pool_info_t*)c;
		}
		case CONSTANT_InvokeDynamic: {
			CONSTANT_InvokeDynamic_info_t * c = malloc(sizeof(CONSTANT_InvokeDynamic_info_t));
			c->tag                       = *ptr;
			c->bootstrap_method_attr_idx = get_u2(ptr+1);
			c->name_and_type_idx         = get_u2(ptr+3);
			*sz = 5;
			return (const_pool_info_t*)c;
		}
		default:
			HB_ERR("Invalid constant tag: %d\n", *ptr);
			return NULL;
		}

	return NULL;
}


/* 
 * KCH NOTE: "The value of the constant_pool_count item is equal 
 * to the number of entries in the constant_pool table plus one. A  * constant_pool index is considered valid if it is greater than zero
 * and less than constant_pool_count, with the exception for 
 * constants of type long and double noted in §4.4.5.
 * 
 */
static int 
parse_const_pool (u1 * clb, java_class_t * cls, u4 * skip)
{
	int i;

	cls->const_pool = (const_pool_info_t**)malloc(sizeof(const_pool_info_t*)*(cls->const_pool_count));

	if (!cls->const_pool) {
		HB_ERR("Could not allocate constant pool\n");
		return -1;
	}

	memset(cls->const_pool, 0, sizeof(const_pool_info_t*)*(cls->const_pool_count));

	for (i = 1; i < cls->const_pool_count; i++) {
		const_pool_info_t * cinfo = NULL;
		u1 sz = 0;

		cinfo = parse_const_pool_entry(clb, &sz);

		if (!cinfo) {
			HB_ERR("Could not parse constant pool entry %d\n", i);
			return -1;
		}

		cls->const_pool[i] = cinfo;

		clb += sz;
		*skip += sz;

		// KCH: wide entries take up two slots in the const
  		// pool for some dumb reason. The second entry
		// "should be valid but is considered unusable"
		if (cls->const_pool[i]->tag == CONSTANT_Long ||
		    cls->const_pool[i]->tag == CONSTANT_Double) {
			cls->const_pool[++i] = NULL;
		}
	}
		
	return 0;
}


static int
parse_ixes (u1 * clb, java_class_t * cls, u4 * skip)
{
	int i;

	cls->interfaces = malloc(cls->interfaces_count * sizeof(u2));
	if (!cls->interfaces) {
		HB_ERR("Could not allocate interfaces\n");
		return -1;
	}
	memset(cls->interfaces, 0, cls->interfaces_count * sizeof(u2));

	for (i = 0; i < cls->interfaces_count; i++) {
		GET_AND_INC(interfaces[i], 2);
		*skip += 2;
	}

	return 0;
}

static inline u1
is_const_val_attr (u2 name_idx, java_class_t * cls)
{
	return strncmp(hb_get_const_str(name_idx, cls), "ConstantValue", 13) == 0;
}

static int
parse_const_val_attr (u1 * clb, field_info_t * fi, java_class_t * cls)
{
	//u2 attr_name_idx = get_u2(clb); 
	clb+=2;
	//u4 attr_len      = get_u4(clb); 
	clb+=4;
	u2 const_val_idx = get_u2(clb); clb+=2;

	fi->has_const = 1;
	fi->cpe = cls->const_pool[const_val_idx];
	
	return 0;
}

static int
parse_fields (u1 * clb, java_class_t * cls, u4 * skip)
{
	int i;

	cls->fields = malloc(sizeof(field_info_t)*cls->fields_count);
	if (!cls->fields) {
		HB_ERR("Could not allocate fields\n");
		return -1;
	}
	memset(cls->fields, 0, sizeof(field_info_t)*cls->fields_count);
	
	for (i = 0; i < cls->fields_count; i++) {
		GET_AND_INC(fields[i].acc_flags, 2);
		GET_AND_INC(fields[i].name_idx, 2);
		GET_AND_INC(fields[i].desc_idx, 2);
		GET_AND_INC(fields[i].attr_count, 2);

		cls->fields[i].owner = cls;

		*skip += 8;

		if (cls->fields[i].attr_count > 0) {
			int j;
			for (j = 0; j < cls->fields[i].attr_count; j++) {
				u2 name_idx;
				u4 len;
	
				name_idx = get_u2(clb); 
				clb += 2;
				len = get_u4(clb);

				if (is_const_val_attr(name_idx, cls)) {
					parse_const_val_attr(clb-2, &cls->fields[i], cls);
				}
				clb += 4 + len;
				*skip += 6 + len;
			}
		}
	}

	return 0;
}


static int
parse_method_code (u1 * clb, method_info_t * m, java_class_t * cls)
{
	int i;
	
	u2 attr_name_idx = get_u2(clb); clb+=2;
	u4 attr_len      = get_u4(clb); clb+=4;

	m->code_attr = malloc(sizeof(code_attr_t));
	if (!m->code_attr) {
		HB_ERR("Could not allocate code attribute\n");
		return -1;
	}
	memset(m->code_attr, 0, sizeof(code_attr_t));

	m->code_attr->attr_name_idx = attr_name_idx;
	m->code_attr->attr_len      = attr_len;
	m->code_attr->max_stack     = get_u2(clb); clb+=2;
	m->code_attr->max_locals    = get_u2(clb); clb+=2;
	m->code_attr->code_len      = get_u4(clb); clb+=4;

	m->code_attr->code = malloc(m->code_attr->code_len);
	if (!m->code_attr->code) {
		HB_ERR("Could not allocate code for method!\n");
		return -1;
	}
	
	memcpy(m->code_attr->code, clb, m->code_attr->code_len);

	clb += m->code_attr->code_len;

	m->code_attr->excp_table_len = get_u2(clb); clb+=2;
	
	m->code_attr->excp_table = malloc(sizeof(excp_table_t)*m->code_attr->excp_table_len);
	if (!m->code_attr->excp_table) {
		HB_ERR("Could not allocate exception table\n");
		return -1;
	}

	for (i = 0; i < m->code_attr->excp_table_len; i++) {
		m->code_attr->excp_table[i].start_pc   = get_u2(clb); clb+=2;
		m->code_attr->excp_table[i].end_pc     = get_u2(clb); clb+=2;
		m->code_attr->excp_table[i].handler_pc = get_u2(clb); clb+=2;
		m->code_attr->excp_table[i].catch_type = get_u2(clb); clb+=2;
	}

	m->code_attr->attr_count = get_u2(clb); 

	return 0;
}


static inline u1
is_code_attr (u2 name_idx, java_class_t * cls)
{
	return strncmp(hb_get_const_str(name_idx, cls), "Code", 4) == 0;
}


static inline u1
is_excp_attr (u2 name_idx, java_class_t * cls)
{
	return strncmp(hb_get_const_str(name_idx, cls), "Exception", 9) == 0;
}



static int
parse_methods (u1 * clb, java_class_t * cls, u4 * skip)
{
	int i, j;

	cls->methods = malloc(sizeof(method_info_t)*cls->methods_count);
	if (!cls->methods) {
		HB_ERR("Could not allocate methods\n");	
		return -1;
	}
	memset(cls->methods, 0, sizeof(method_info_t)*cls->methods_count);

	for (i = 0; i < cls->methods_count; i++) {
		GET_AND_INC(methods[i].acc_flags, 2);
		GET_AND_INC(methods[i].name_idx, 2);
		GET_AND_INC(methods[i].desc_idx, 2);
		GET_AND_INC(methods[i].attr_count, 2);

		cls->methods[i].owner = cls;

		*skip += 8;

		if (cls->methods[i].attr_count > 0) {

			for (j = 0; j < cls->methods[i].attr_count; j++) {
				u2 name_idx;
				u4 len;

				name_idx = get_u2(clb); 
				clb += 2;
				len = get_u4(clb);
				clb += 4;
				
				if (is_code_attr(name_idx, cls)) {
					parse_method_code(clb-6, &cls->methods[i], cls);
				} 
					
				clb += len;
				*skip += len;
			}
		}
	}

	return 0;
}

/* TODO: complete this */
static int
parse_attrs (u1 * clb, java_class_t * cls, u4 * skip)
{
	CL_DEBUG("WARNING: %s not implemented\n", __func__);
	return 0;
}


static int
parse_class (u1 * clb, java_class_t * cls)
{
	u4 skip;

	GET_AND_INC(magic, 4);
	GET_AND_INC(minor_version, 2);
	GET_AND_INC(major_version, 2);
	GET_AND_INC(const_pool_count, 2);

	PARSE_AND_CHECK(const_pool_count, parse_const_pool, "const pool");

	GET_AND_INC(acc_flags, 2);
	GET_AND_INC(this, 2);
	GET_AND_INC(super, 2);
	GET_AND_INC(interfaces_count, 2);

	PARSE_AND_CHECK(interfaces_count, parse_ixes, "interfaces");

	GET_AND_INC(fields_count, 2);

	PARSE_AND_CHECK(fields_count, parse_fields, "fields");

	GET_AND_INC(methods_count, 2);

	PARSE_AND_CHECK(methods_count, parse_methods, "methods");

	GET_AND_INC(attr_count, 2);

	PARSE_AND_CHECK(attr_count, parse_attrs, "attributes");
	
	return 0;
}

/* TODO: deeper verification required */
static int
verify_class (java_class_t * cls)
{
	if (cls->magic != JAVA_MAGIC) {
		HB_ERR("Bad magic: 0x%04x\n", cls->magic);
		return -1;
	} 

	return 0;
}

static void
print_class_info (java_class_t * cls)
{
	HB_INFO("Minor Version: %x\n", cls->minor_version);
	HB_INFO("Major Verison: %x\n", cls->major_version);
	HB_INFO("Const pool count: %d\n", cls->const_pool_count);
	HB_INFO("Interfaces count: %d\n", cls->interfaces_count);
	HB_INFO("Fields count: %d\n", cls->fields_count);
	HB_INFO("Methods count: %d\n", cls->methods_count);
	HB_INFO("Attributes count: %d\n", cls->attr_count);
}

/*
 * TODO: should throw a ClassNotFoundException on error
 */
java_class_t * 
hb_load_class (const char * path)
{
	java_class_t * cls = NULL;
	u1 * class_bytes   = NULL;

	class_bytes = open_class_file(path);

	if (!class_bytes) {
		HB_ERR("Could not load class file\n");
		return NULL;
	}

	cls = malloc(sizeof(java_class_t));
	if (!cls) {
		HB_ERR("Could not allocate class struct\n");
		return NULL;
	}
	memset(cls, 0, sizeof(java_class_t));

	if (parse_class(class_bytes, cls) != 0) {
		HB_ERR("Could not parse class file\n");
		return NULL;
	}

	if (verify_class(cls) != 0) {
		HB_ERR("Could not verify class\n");
		return NULL;
	}

	cls->name   = path;

	CL_DEBUG("Class file (for class %s) verified and loaded\n", hb_get_class_name(cls));

	cls->field_vals = malloc(sizeof(var_t)*cls->fields_count);
	if (!cls->field_vals) {
		HB_ERR("Could not allocate class fields\n");
		return NULL;
	}
	memset(cls->field_vals, 0, sizeof(var_t)*cls->fields_count);

	cls->status = CLS_LOADED;
	
	return cls;
}
