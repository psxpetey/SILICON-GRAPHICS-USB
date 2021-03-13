/*
********************************************************************
* USB stack and host controller driver for SGI IRIX 6.5            *
*                                                                  *
* Programmed by BSDero                                             *
* bsdero at gmail dot com                                          *
* 2011/2012                                                        *
*                                                                  *
*                                                                  *
* File: trace.c                                                    *
* Description: Tracing and debug macros and variable               *
*                                                                  *
*                                                                  *
********************************************************************
*******************************************************************************************************
* FIXLIST (latest at top)                                                                             *
*-----------------------------------------------------------------------------------------------------*
* Author      MM-DD-YYYY     Description                                                              *
*-----------------------------------------------------------------------------------------------------*
* BSDero      08-08-2012     -TRACEWAR macro updated for no level. It should display warnings despite *
*                            configured trace level                                                   *
*                                                                                                     *
* BSDero      07-31-2012     -Use cmn_err() instead printf()                                          *
*                                                                                                     *
* BSDero      05-18-2012     -Added TRACE_LEVEL_DELAY macros                                          *
*                                                                                                     *
* BSDero      05-13-2012     -Added TRACERR and TRACEWAR macros                                       *
*                                                                                                     *
* BSDero      01-09-2012     -Initial version                                                         *
*                                                                                                     *
*******************************************************************************************************
*/


#define __FUNCTION__                                __func__
#define TRACE_LEVEL_DELAY                           120

#define TRACE( trc_class, trc_level, fmt, ...)                                                                      \
            if(( global_trace_class.level >= trc_level) && ( (trc_class & global_trace_class.class) != 0)){         \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                   \
                    USECDELAY( 1000000);                                                                            \
                printf( "USB:%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);                     \
            }

#define TRACERR( trc_class, fmt, ...)                                                                               \
            if(((trc_class | TRC_ERROR) & global_trace_class.class) != 0){                                          \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                   \
                    USECDELAY( 500000);                                                                             \
                cmn_err( CE_ALERT, "%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);              \
            }

#define TRACEWAR( trc_class, fmt, ...)                                                                           \
            if(((trc_class | TRC_WARNING) & global_trace_class.class) != 0){ \
                if( global_trace_class.level > TRACE_LEVEL_DELAY)                                                           \
                    USECDELAY( 1000000);                                                                                    \
                cmn_err( CE_WARN, "%s,%s,%d:"fmt"\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);                       \
            }


