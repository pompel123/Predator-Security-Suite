#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file predator_calypso_listener.h
 * @brief Calypso NFC Listener - Emulates a valid TL Lausanne Mobilis ticket
 * 
 * REAL IMPLEMENTATION - Will respond to TL bus validators
 */

/**
 * Initialize emulated ticket with balance and trips
 * @param balance_centimes Balance in centimes (10000 = 100.00 CHF)
 * @param trips Number of remaining trips
 */
void calypso_listener_init_ticket(uint16_t balance_centimes, uint8_t trips);

/**
 * Handle command from validator
 * @param cmd Command buffer from validator
 * @param cmd_len Command length
 * @param response Response buffer to fill
 * @param response_len Pointer to response length
 * @return true if handled successfully
 */
bool calypso_listener_handle_command(const uint8_t* cmd, size_t cmd_len,
                                     uint8_t* response, size_t* response_len);

/**
 * Get current ticket info for display
 * @param balance Pointer to receive balance (can be NULL)
 * @param trips Pointer to receive trips (can be NULL)
 */
void calypso_listener_get_ticket_info(uint16_t* balance, uint8_t* trips);
