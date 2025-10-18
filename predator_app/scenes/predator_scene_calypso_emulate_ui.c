#include "../predator_i.h"
#include "../helpers/predator_calypso_listener.h"
#include <furi_hal_nfc.h>

// Calypso Ticket Emulation - REAL LISTENER MODE
// This ACTUALLY responds to TL bus validators!

static uint32_t emulate_tick = 0;
static bool validator_contacted = false;
static uint32_t last_query_time = 0;

static void calypso_emulate_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    
    canvas_clear(canvas);
    
    // Header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 10, "🎫 TL MOBILIS");
    
    // Ticket status
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "READY FOR VALIDATION");
    
    // Balance (big display)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 36, "100.00 CHF");
    
    // Trip counter
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 48, "Trips remaining: 50");
    
    // Show validator status
    if(validator_contacted) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 20, 58, "✓ VALIDATED!");
    } else {
        // Animated "waiting for validator"
        char waiting[20] = "Listening";
        uint32_t dots = (emulate_tick / 5) % 4;
        for(uint32_t i = 0; i < dots; i++) {
            waiting[9 + i] = '.';
        }
        waiting[9 + dots] = '\0';
        canvas_draw_str(canvas, 2, 58, waiting);
    }
    
    // Instructions
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 64, validator_contacted ? "[Back] Exit" : "[Place near validator]");
}

static bool calypso_emulate_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    
    return false;
}

static void calypso_emulate_timer_callback(void* context) {
    PredatorApp* app = context;
    emulate_tick++;
    
    // Check for NFC events from validator
    FuriHalNfcEvent nfc_event = furi_hal_nfc_listener_wait_event(10);  // 10ms timeout
    
    if(nfc_event & FuriHalNfcEventRxEnd) {
        // Received data from validator!
        uint8_t rx_buffer[256];
        size_t rx_bits = 0;
        
        FuriHalNfcError rx_error = furi_hal_nfc_listener_rx(rx_buffer, sizeof(rx_buffer), &rx_bits);
        
        if(rx_error == FuriHalNfcErrorNone && rx_bits > 0) {
            size_t rx_bytes = rx_bits / 8;
            
            FURI_LOG_I("Calypso", "📥 Validator query: %u bytes", (unsigned)rx_bytes);
            
            // Handle command and generate response
            uint8_t tx_buffer[256];
            size_t tx_len = 0;
            
            if(calypso_listener_handle_command(rx_buffer, rx_bytes, tx_buffer, &tx_len)) {
                // Send response back to validator
                size_t tx_bits = tx_len * 8;
                FuriHalNfcError tx_error = furi_hal_nfc_listener_tx(tx_buffer, tx_bits);
                
                if(tx_error == FuriHalNfcErrorNone) {
                    FURI_LOG_I("Calypso", "📤 Sent response: %u bytes", (unsigned)tx_len);
                    validator_contacted = true;
                    last_query_time = furi_get_tick();
                } else {
                    FURI_LOG_E("Calypso", "TX error: %d", tx_error);
                }
            }
        }
    }
    
    // Trigger redraw for animation
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void predator_scene_calypso_emulate_ui_on_enter(void* context) {
    PredatorApp* app = context;
    
    emulate_tick = 0;
    validator_contacted = false;
    last_query_time = 0;
    
    FURI_LOG_I("Calypso", "🎫 REAL EMULATION: TL Mobilis (100 CHF, 50 trips)");
    
    // Initialize emulated ticket (100.00 CHF = 10000 centimes)
    calypso_listener_init_ticket(10000, 50);
    
    // REAL NFC LISTENER MODE - Will respond to TL bus validators!
    FuriHalNfcError error = furi_hal_nfc_acquire();
    if(error == FuriHalNfcErrorNone) {
        error = furi_hal_nfc_low_power_mode_stop();
        if(error == FuriHalNfcErrorNone) {
            error = furi_hal_nfc_set_mode(FuriHalNfcModeListener, FuriHalNfcTechIso14443b);
            if(error == FuriHalNfcErrorNone) {
                FURI_LOG_I("Calypso", "✓ NFC Listener ACTIVE - Ready for validator!");
            } else {
                FURI_LOG_E("Calypso", "Failed to set listener mode: %d", error);
            }
        }
    } else {
        FURI_LOG_E("Calypso", "Failed to acquire NFC: %d", error);
    }
    
    // Create view
    View* view = view_alloc();
    view_set_context(view, app);
    view_set_draw_callback(view, calypso_emulate_draw_callback);
    view_set_input_callback(view, calypso_emulate_input_callback);
    
    view_dispatcher_add_view(app->view_dispatcher, 101, view); // Temporary ID
    view_dispatcher_switch_to_view(app->view_dispatcher, 101);
    
    // Start animation timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    app->timer = furi_timer_alloc(calypso_emulate_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
}

bool predator_scene_calypso_emulate_ui_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_calypso_emulate_ui_on_exit(void* context) {
    PredatorApp* app = context;
    
    // Stop NFC listener - REAL cleanup
    furi_hal_nfc_reset_mode();
    furi_hal_nfc_low_power_mode_start();
    furi_hal_nfc_release();
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    FURI_LOG_I("Calypso", "✓ Emulation stopped - NFC listener disabled");
}
