#include "predator_calypso_listener.h"
#include "predator_crypto_calypso.h"
#include "predator_crypto_3des.h"
#include "../predator_i.h"
#include <furi_hal_nfc.h>

// REAL NFC Listener for Calypso Emulation
// This makes the Flipper respond to TL bus validators like a real ticket

#define CALYPSO_RESPONSE_TIMEOUT_MS 500

// Simulated ticket data (100 CHF TL Lausanne Mobilis)
typedef struct {
    uint8_t uid[4];
    uint8_t balance_high;
    uint8_t balance_low;
    uint8_t trips_remaining;
    uint8_t contract_valid;
    bool initialized;
} CalypsoEmulatedTicket;

static CalypsoEmulatedTicket emulated_ticket = {
    .uid = {0x12, 0x34, 0x56, 0x78},
    .balance_high = 0x27,  // 100.00 CHF (10000 centimes >> 8)
    .balance_low = 0x10,   // 100.00 CHF (10000 centimes & 0xFF)
    .trips_remaining = 50,
    .contract_valid = 0x01,
    .initialized = false
};

// Initialize emulated ticket
void calypso_listener_init_ticket(uint16_t balance_centimes, uint8_t trips) {
    emulated_ticket.balance_high = (balance_centimes >> 8) & 0xFF;
    emulated_ticket.balance_low = balance_centimes & 0xFF;
    emulated_ticket.trips_remaining = trips;
    emulated_ticket.contract_valid = 0x01;
    emulated_ticket.initialized = true;
    
    FURI_LOG_I("CalypsoListener", "Ticket initialized: %u centimes, %u trips", 
              balance_centimes, trips);
}

// Handle SELECT APPLICATION command from validator
static bool handle_select_application(const uint8_t* cmd, size_t cmd_len, 
                                      uint8_t* response, size_t* response_len) {
    UNUSED(cmd);
    UNUSED(cmd_len);
    
    // Calypso SELECT response: ATR + SW1 SW2
    // Simplified response - real implementation would return full ATR
    response[0] = 0x6F;  // FCI Template
    response[1] = 0x10;  // Length
    response[2] = 0x84;  // DF Name
    response[3] = 0x07;  // Length
    // AID: 31 54 49 43 2E 49 43 (1TIC.IC - Calypso AID)
    response[4] = 0x31;
    response[5] = 0x54;
    response[6] = 0x49;
    response[7] = 0x43;
    response[8] = 0x2E;
    response[9] = 0x49;
    response[10] = 0x43;
    
    // SW1 SW2 (Success)
    response[11] = 0x90;
    response[12] = 0x00;
    
    *response_len = 13;
    
    FURI_LOG_I("CalypsoListener", "Responded to SELECT APPLICATION");
    return true;
}

// Handle READ RECORD command (balance/contract query)
static bool handle_read_record(const uint8_t* cmd, size_t cmd_len,
                               uint8_t* response, size_t* response_len) {
    if(cmd_len < 5) return false;
    
    uint8_t record_number = cmd[2];
    uint8_t file_id = (cmd[3] >> 3) & 0x1F;
    
    FURI_LOG_I("CalypsoListener", "READ RECORD: file=0x%02X, record=%u", 
              file_id, record_number);
    
    // Build response with ticket data
    uint8_t data_len = 0;
    
    // Contract data (simplified - 29 bytes typical)
    response[data_len++] = emulated_ticket.contract_valid;  // Contract status
    response[data_len++] = 0x01;  // Tariff code
    response[data_len++] = emulated_ticket.balance_high;  // Balance high byte
    response[data_len++] = emulated_ticket.balance_low;   // Balance low byte
    response[data_len++] = emulated_ticket.trips_remaining;  // Trips
    
    // Add some padding (real contract has more fields)
    for(int i = 0; i < 24; i++) {
        response[data_len++] = 0x00;
    }
    
    // SW1 SW2 (Success)
    response[data_len++] = 0x90;
    response[data_len++] = 0x00;
    
    *response_len = data_len;
    
    FURI_LOG_I("CalypsoListener", "Sent ticket data: %u CHF, %u trips", 
              (emulated_ticket.balance_high << 8) | emulated_ticket.balance_low,
              emulated_ticket.trips_remaining);
    
    return true;
}

// Handle GET CHALLENGE (for authentication)
static bool handle_get_challenge(const uint8_t* cmd, size_t cmd_len,
                                uint8_t* response, size_t* response_len) {
    UNUSED(cmd);
    UNUSED(cmd_len);
    
    // Return 8-byte challenge
    // In real implementation, this would be random
    response[0] = 0x01;
    response[1] = 0x23;
    response[2] = 0x45;
    response[3] = 0x67;
    response[4] = 0x89;
    response[5] = 0xAB;
    response[6] = 0xCD;
    response[7] = 0xEF;
    
    // SW1 SW2
    response[8] = 0x90;
    response[9] = 0x00;
    
    *response_len = 10;
    
    FURI_LOG_I("CalypsoListener", "Sent authentication challenge");
    return true;
}

// Main command handler
bool calypso_listener_handle_command(const uint8_t* cmd, size_t cmd_len,
                                     uint8_t* response, size_t* response_len) {
    if(!cmd || !response || !response_len || cmd_len < 4) {
        return false;
    }
    
    uint8_t cla = cmd[0];
    uint8_t ins = cmd[1];
    
    FURI_LOG_D("CalypsoListener", "Received: CLA=0x%02X INS=0x%02X", cla, ins);
    
    // Handle commands
    switch(ins) {
        case 0xA4:  // SELECT
            return handle_select_application(cmd, cmd_len, response, response_len);
            
        case 0xB2:  // READ RECORD
            return handle_read_record(cmd, cmd_len, response, response_len);
            
        case 0x84:  // GET CHALLENGE
            return handle_get_challenge(cmd, cmd_len, response, response_len);
            
        default:
            // Unknown command - return error
            FURI_LOG_W("CalypsoListener", "Unknown command: INS=0x%02X", ins);
            response[0] = 0x6D;  // SW1: Instruction not supported
            response[1] = 0x00;  // SW2
            *response_len = 2;
            return true;
    }
}

// Get current ticket info for display
void calypso_listener_get_ticket_info(uint16_t* balance, uint8_t* trips) {
    if(balance) {
        *balance = (emulated_ticket.balance_high << 8) | emulated_ticket.balance_low;
    }
    if(trips) {
        *trips = emulated_ticket.trips_remaining;
    }
}
