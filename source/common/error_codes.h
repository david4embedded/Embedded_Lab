/************************************************************************************************************
 * 
 * @file error_codes.h
 * @brief This file defines error codes used in the message passing system.
 * 
 * @author Sungsu Kim
 * @copyright 2025 Sungsu Kim
 * @date 2025-07-27
 * @version 1.0
 * 
 ************************************************************************************************************/

#pragma once

/**
 * @brief Enumeration of error codes used in the message passing system.
 */
enum ErrorCodes
{
   OK                               = 0,
   NOT_INITIALIZED                  = 1,
   NO_BUFFER_GIVEN                  = 2,
   BUFFER_SIZE_TOO_BIG              = 3,
   NO_RECEIVERS_GIVEN               = 4,
   RECEIVERS_TOO_MANY               = 5,
   MUTEX_INIT_FAILED                = 6,
   MUTEX_GET_FAILED                 = 7,
   MSG_SEMAPHORE_INIT_FAILED        = 8,
   MSG_SEMAPHORE_TAKE_TIMEOUT       = 9,
   NO_MESSAGE_INDEX_IN_BUFFER       = 10,
   INVALID_MESSAGE_POINTER          = 11,
   DESTINATION_ID_OUT_OF_RANGE      = 12,
   NO_MESSAGE_FOUND_FOR_DESTINATION = 13
};
