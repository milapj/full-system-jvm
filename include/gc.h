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
#ifndef __GC_H__
#define __GC_H__

#include <hawkbeans.h>

#if DEBUG_GC == 1
#define GC_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define GC_DEBUG(fmt, args...)
#endif

/* GC will run every 20 ms or so */
#define GC_DEFAULT_INTERVAL 20

struct nk_hashtable;
struct jthread;

typedef enum ref_state {
	GC_REF_ABSENT,
	GC_REF_PRESENT,
} ref_state_t; 

/* this may have more info soon */
typedef struct ref_entry {
	ref_state_t state;
} ref_entry_t;

typedef struct gc_ref_tbl {
	struct nk_hashtable * htable;
} gc_ref_tbl_t;

typedef struct gc_stats {
	u8 gc_time;
	u8 mark_time;
	u8 sweep_time;
	u4 obj_collected;
	u4 bytes_reclaimed;
} gc_stats_t;

typedef struct gc_time {
	u8 time_since_collect;
	int interval_ms;
} gc_time_t;

typedef struct gc_state {
	struct list_head root_list;
	gc_ref_tbl_t * ref_tbl;

	gc_stats_t collect_stats;
	gc_time_t time_info;
	int trace;
} gc_state_t;

typedef struct gc_root {
	struct list_head link;
	int (*scan)(struct gc_state * gc_state, void * priv_data);
	const char * name;
	void * ptr;
} gc_root_t;


int gc_insert_ref(struct obj_ref * ref);
int gc_collect(struct jthread * t);
int gc_should_collect(struct jthread * t);
int gc_init(struct jthread * main, struct obj_ref * base_obj, int trace, int interval);

/* allocation interface */
struct obj_ref * gc_array_alloc(u1 type, i4 count);
struct obj_ref * gc_str_obj_alloc(const char * str);
struct obj_ref * gc_obj_alloc(struct java_class * cls);

#endif



