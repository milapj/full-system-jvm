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
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include <hawkbeans.h>
#include <bc_interp.h>
#include <class.h>
#include <thread.h>
#include <stack.h>
#include <mm.h>
#include <gc.h>

#include <arch/x64-linux/bootstrap_loader.h>

jthread_t * cur_thread;

static void
version (void)
{
	printf("Hawkbeans Java Virtual Machine Version %s\n", HAWKBEANS_VERSION_STRING);
	printf("Kyle C. Hale (c) 2017, Illinois Institute of Technology\n");
	exit(EXIT_SUCCESS);
}


static void 
usage (const char * prog)
{
	fprintf(stderr, "This is the Hawkbeans Java Virtual Machine. Usage\n\n");
	fprintf(stderr, "%s [options] classfile\n\n", prog);
	fprintf(stderr, "Arguments:\n\n");
	fprintf(stderr, " %20.20s Print the version number and exit\n", "--version, -V");
	fprintf(stderr, " %20.20s Print this message\n", "--help, -h");
	fprintf(stderr, " %20.20s Set the heap size (in MB). Default is 1MB.\n", "--heap-size, -H");
	fprintf(stderr, " %20.20s Trace the Garbage Collector\n", "--trace-gc, -t");
	fprintf(stderr, " %20.20s GC collection interval in ms\n", "--gc-interval, -c");
	fprintf(stderr, "\n\n");
	exit(EXIT_SUCCESS);
}


static struct option long_options[] = {
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"heap-size", required_argument, 0, 'H'},
	{"trace-gc", no_argument, 0, 't'},
	{"gc-interval", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};


static struct gopts {
	int heap_size_megs;
	int trace_gc;
	const char * class_path;
	int gc_interval;
} glob_opts;


static obj_ref_t * 
create_argv_array (int argc, char ** argv) 
{
	int i;
	obj_ref_t * arr = array_alloc(T_REF, argc);
	native_obj_t * arr_obj = NULL;
	
	if (!arr) {
		HB_ERR("Could not create jargv array\n"); 
		return NULL;
	}

	arr_obj = (native_obj_t*)arr->heap_ptr;

	for (i = 0; i < argc; i++) {
		obj_ref_t * str_obj = string_object_alloc(argv[i]);
		if (!str_obj) {
			HB_ERR("Could not create string object for argv array\n");
			return NULL;
		}
		arr_obj->fields[i].obj = str_obj;
	}
	
	return arr;
}


static void
parse_args (int argc, char ** argv)
{
	int c;

	while (1) {
		int opt_idx = 0;
		c = getopt_long(argc, argv, "c:hVH:t", long_options, &opt_idx);
		
		if (c == -1) {
			break;
		}

		switch (c) {
			case 0:
				break;
			case 'V':
				version();
				break;
			case 'h':
				usage(argv[0]);
				break;
			case 'c':
				glob_opts.gc_interval = atoi(optarg);
				break;
			case 'H': 
				glob_opts.heap_size_megs = atoi(optarg);
				break;
			case 't':
				glob_opts.trace_gc = 1;
			case '?':
				break;
			default:
				printf("Unknown option: %o.\n", c);
				break;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
	}
	
	// the rest are for String[] passed to java main
	glob_opts.class_path = argv[optind++];
}


int 
main (int argc, char ** argv) 
{
	java_class_t * cls = NULL;
	obj_ref_t * obj = NULL;
	obj_ref_t * jargv = NULL;
	jthread_t * main_thread = NULL;
	int main_idx;

	HB_DEBUG("======= HAWKBEANS JVM ========\n");

	parse_args(argc, argv);

	/* setup the heap using default sizes */
	heap_init(glob_opts.heap_size_megs);

	/* initialize the hashtable that stores loaded classes */
	hb_classmap_init();

	cls = hb_load_class(glob_opts.class_path);

	if (!cls) {
		HB_ERR("Could not load base class (%s)\n", glob_opts.class_path);
		exit(EXIT_FAILURE);
	}

	obj = object_alloc(cls);

	main_idx    = hb_get_method_idx("main", cls);
	main_thread = hb_create_thread(cls, "main");

	if (!main_thread) {
		HB_ERR("Could not create main thread\n");
		exit(EXIT_FAILURE);
	}

	cur_thread = main_thread;

	jargv = create_argv_array(argc-optind, &argv[optind]);

	hb_push_base_frame(main_thread, cls, jargv, main_idx);

	if (!jargv) {
		HB_ERR("Could not create Java argv array\n");
		exit(EXIT_FAILURE);
	}

	gc_init(main_thread, obj, glob_opts.trace_gc, glob_opts.gc_interval);

	gc_insert_ref(jargv);
	
	hb_exec(main_thread);

	HB_DEBUG("======= HAWKBEANS EXIT ========\n");

	return 0;
}
