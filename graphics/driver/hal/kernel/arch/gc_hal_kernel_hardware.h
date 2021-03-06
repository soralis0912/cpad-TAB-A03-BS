/****************************************************************************
*
*    Copyright (c) 2005 - 2015 by Vivante Corp.
*    
*    This program is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the license, or
*    (at your option) any later version.
*    
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*    GNU General Public License for more details.
*    
*    You should have received a copy of the GNU General Public License
*    along with this program; if not write to the Free Software
*    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*
*****************************************************************************/


#ifndef __gc_hal_kernel_hardware_h_
#define __gc_hal_kernel_hardware_h_

#if gcdENABLE_VG
#include "gc_hal_kernel_hardware_vg.h"

#endif

#ifdef __cplusplus
extern "C" {

#endif

typedef struct _gckPulseEaterCounter * gckPulseEaterCounter;
typedef struct _gckPulseEaterDB * gckPulseEaterDB;
typedef struct _pulseEater_frequency_table * pulseEaterFTL;

/*
    hwPeriod --- setting data in register
    hwMs --- 10*hw-period-time
*/
struct _pulseEater_frequency_table
{
    gctUINT32    index;
    gctUINT32    hwPeriod;
    gctUINT32    swPeriod;
    gctUINT8     hwMs;
    gctUINT32    frequency;
};

struct _gckPulseEaterCounter
{
    gctUINT32                   sfPeriod;
    gctUINT32                   freq;
    gctUINT32                   time;
    gctUINT8                    hwMs;
    gctUINT8                    pulseCount[4];
};

struct _gckPulseEaterDB
{
    gctUINT32                   lastIndex;
    gctUINT32                   timeStamp;
    gctUINT32                   timeStampInLastRound;
    struct _gckPulseEaterCounter data[PULSE_EATER_COUNT];
    gctUINT32                   startTime;
    gctUINT32                   busyRatio[4];
    gctUINT32                   totalTime[4];
    gctUINT32                   startIndex;
    gctBOOL                     moreRound;
};

typedef enum {
    gcvHARDWARE_FUNCTION_MMU,
    gcvHARDWARE_FUNCTION_FLUSH,

    gcvHARDWARE_FUNCTION_NUM,
}
gceHARDWARE_FUNCTION;


typedef struct _gcsHARWARE_FUNCTION
{
    /* Entry of the function. */
    gctUINT32                   address;

    /* Bytes of the function. */
    gctUINT32                   bytes;

    /* Hardware address of END in this function. */
    gctUINT32                   endAddress;

    /* Logical of END in this function. */
    gctUINT8_PTR                endLogical;
}
gcsHARDWARE_FUNCTION;

/* gckHARDWARE object. */
struct _gckHARDWARE
{
    /* Object. */
    gcsOBJECT                   object;

    /* Pointer to gctKERNEL object. */
    gckKERNEL                   kernel;

    /* Pointer to gctOS object. */
    gckOS                       os;

    /* Core */
    gceCORE                     core;

    /* Chip characteristics. */
    gcsHAL_QUERY_CHIP_IDENTITY  identity;
    gctBOOL                     allowFastClear;
    gctBOOL                     allowCompression;
    gctUINT32                   powerBaseAddress;
    gctBOOL                     extraEventStates;

    /* Big endian */
    gctBOOL                     bigEndian;

    /* Chip status */
#if 0
    gctPOINTER                  powerMutex;

#endif
    gctUINT32                   powerProcess;
    gctUINT32                   powerThread;
    gceCHIPPOWERSTATE           chipPowerState;
    gctUINT32                   lastWaitLink;
    gctUINT32                   lastEnd;
    gctPOINTER                  clockState;
    gctPOINTER                  powerState;
#if 0
    gctPOINTER                  globalSemaphore;

#endif
    gckRecursiveMutex           recMutexPower;
    gctBOOL                     clk2D3D_Enable;
    gctUINT32                   refExtClock;
    gctUINT32                   refExtPower;

    gctISRMANAGERFUNC           startIsr;
    gctISRMANAGERFUNC           stopIsr;
    gctPOINTER                  isrContext;

    gctUINT32                   mmuVersion;

    /* Whether use new MMU. It is meaningless
    ** for old MMU since old MMU is always enabled.
    */
    gctBOOL                     enableMMU;

    /* Type */
    gceHARDWARE_TYPE            type;

#if gcdPOWEROFF_TIMEOUT
    gctUINT32                   powerOffTime;
    gctUINT32                   powerOffTimeout;
    gctPOINTER                  powerOffTimer;
    gctBOOL                     enablePowerOffTimeout;

#endif

#if gcdENABLE_FSCALE_VAL_ADJUST
    gctUINT32                   powerOnFscaleVal;

#endif
    gctPOINTER                  pageTableDirty;

#if gcdLINK_QUEUE_SIZE
    struct _gckLINKQUEUE        linkQueue;

#endif

    gctBOOL                     powerManagement;
    gctBOOL                     gpuProfiler;

    gctBOOL                     endAfterFlushMmuCache;

#if MRVL_CONFIG_ENABLE_GPUFREQ
    struct gcsDEVOBJECT         devObj;
#   if MRVL_CONFIG_SHADER_CLK_CONTROL
    struct gcsDEVOBJECT         devShObj;
#   endif

#endif

    gctPOINTER                  clockMutex;

    gckPulseEaterDB             pulseEaterDB[2];
    gctPOINTER                  pulseEaterMutex;
    gctPOINTER                  pulseEaterTimer;
    gctINT                      hwPeriod;
    gctUINT32                   sfPeriod;
    gctUINT8                    hwMs;
    gctUINT32                   freq;

    gctUINT32                   minFscaleValue;

    gctPOINTER                  pendingEvent;

    /* Function used by gckHARDWARE. */
    gctPHYS_ADDR                functionPhysical;
    gctPOINTER                  functionLogical;
    gctUINT32                   functionAddress;
    gctSIZE_T                   functionBytes;

    gcsHARDWARE_FUNCTION        functions[gcvHARDWARE_FUNCTION_NUM];
    gctUINT32                   executeCount;
    gctUINT32                   lastExecuteAddress;

    /* Clock gating state. */
    gctUINT32                   chipClockGatingState;
};

gceSTATUS
gckHARDWARE_GetBaseAddress(
    IN gckHARDWARE Hardware,
    OUT gctUINT32_PTR BaseAddress
    );

gceSTATUS
gckHARDWARE_NeedBaseAddress(
    IN gckHARDWARE Hardware,
    IN gctUINT32 State,
    OUT gctBOOL_PTR NeedBase
    );

gceSTATUS
gckHARDWARE_GetFrameInfo(
    IN gckHARDWARE Hardware,
    OUT gcsHAL_FRAME_INFO * FrameInfo
    );

#ifdef __cplusplus
}

#endif


#endif /* __gc_hal_kernel_hardware_h_ */
