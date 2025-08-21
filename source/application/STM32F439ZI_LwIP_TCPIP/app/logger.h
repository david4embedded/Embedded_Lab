/************************************************************************************************************
 * 
 * @file logger.h
 * @brief Logger module for logging messages.
 *  
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-08-20
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

/******************************************** Function Declarations *****************************************/ 
void  LOGGER_init                      ( );
void  LOGGER_msgXferCompleteCallback   ( );

#if defined (__cplusplus)
}
#endif
