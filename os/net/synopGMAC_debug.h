/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-08-24     chinesebear  first version
 */


#ifndef __DEBUG_H__
#define __DEBUG_H__

//#define GMAC_DEBUG

#ifdef GMAC_DEBUG	
#define DEBUG_MES	printf
#define TR0 printf	
#define TR  printf
#else
#define DEBUG_MES(...)
#define TR0(fmt, args...) 		
#define TR(fmt, args...)  
#endif

#endif /*__DEBUG_H__*/
