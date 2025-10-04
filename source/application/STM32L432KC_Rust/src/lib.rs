/************************************************************************************************************
 * 
 * @file           : lib.rs
 * @brief          : Library module
 * @details        : This file contains the library module for the system.
 * @author         : Sungsu Kim
 * @date           : 2025-10-03
 * @copyright      : Copyright (c) 2026 Sungsu Kim
 *
 ************************************************************************************************************/

/************************************************* Imports **************************************************/ 
#![no_std]

pub mod stepper;

#[macro_use]
pub mod cli;

/************************************************* Aliases *************************************************/
pub type MutexEmbassy = embassy_sync::blocking_mutex::raw::ThreadModeRawMutex;