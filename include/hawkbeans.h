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
#ifndef __HAWKBEANS_H__
#define __HAWKBEANS_H__

#define HAWKBEANS_VERSION_STRING "0.0.1"

/* TODO: this should be arch specific */
#include <stdio.h>
#define HB_INFO(fmt, args...) printf("Hawkbeans: " fmt, ##args)
#define HB_ERR(fmt, args...) fprintf(stderr, "HB ERROR: " fmt, ##args)

#include <config.h>

#if DEBUG_ENABLE == 1
#define HB_DEBUG(fmt, args...) fprintf(stderr, "HB DEBUG: " fmt, ##args)
#else
#define HB_DEBUG(fmt, args...)
#endif


#endif
