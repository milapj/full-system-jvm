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
#ifndef __TYPES_H__
#define __TYPES_H__

/* Linux x64 */
typedef long           i8;
typedef int            i4;
typedef short          i2;
typedef unsigned long  u8;
typedef unsigned int   u4;
typedef unsigned short u2;
typedef unsigned char  u1;
typedef float          f4;
typedef double         d8;

#define HB_NULL (0)

#define T_BOOLEAN 4
#define T_CHAR 5
#define T_FLOAT	6
#define T_DOUBLE 7
#define T_BYTE 8
#define T_SHORT 9
#define T_INT 10
#define T_LONG 11
#define T_REF 12

#endif
