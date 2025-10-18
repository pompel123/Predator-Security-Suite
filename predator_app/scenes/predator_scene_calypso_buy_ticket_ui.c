#include "../predator_i.h"

// Calypso Buy Ticket Demo - For TL Lausanne demonstration
// Shows purchasing a ticket and displays it ready for validation

typedef enum {
    TicketBuyStateSelecting,
    TicketBuyStatePurchasing,
    TicketBuyStateReady
} TicketBuyState;

typedef enum {
    TicketCityLausanne,    // TL Lausanne - MOST IMPORTANT
    TicketCityParis,       // Navigo Paris
    TicketCityBrussels,    // MOBIB Brussels
    TicketCityLisbon,      // Viva Viagem Lisbon
    TicketCityGeneva,      // TPG Geneva
} TicketCity;

static TicketBuyState ticket_state = TicketBuyStateSelecting;
static TicketCity selected_city = TicketCityLausanne;
static uint32_t animation_tick = 0;
static const char* city_names[] = {
    "TL Lausanne",
    "Navigo Paris",
    "MOBIB Brussels",
    "Viva Lisboa",
    "TPG Geneva"
};
static const char* ticket_prices[] = {
    "100 CHF",
    "75.20 EUR",
    "49.00 EUR",
    "40.00 EUR",
    "70 CHF"
};

static void calypso_buy_ticket_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    switch(ticket_state) {
        case TicketBuyStateSelecting:
            // Show city selection
            canvas_draw_str(canvas, 2, 10, "🎫 Buy Calypso Ticket");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 22, "Select city:");
            
            // Show 5 cities with selection indicator
            for(uint32_t i = 0; i < 5; i++) {
                const char* prefix = (i == selected_city) ? "> " : "  ";
                char line[32];
                snprintf(line, sizeof(line), "%s%s - %s", 
                        prefix, city_names[i], ticket_prices[i]);
                canvas_draw_str(canvas, 2, 32 + (i * 8), line);
            }
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 64, "[↕] Select [OK] Buy");
            break;
            
        case TicketBuyStatePurchasing:
            // Animated purchase
            char header[32];
            snprintf(header, sizeof(header), "🎫 %s", city_names[selected_city]);
            canvas_draw_str(canvas, 2, 10, header);
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 22, "Purchasing...");
            
            // Animated dots
            char dots[5] = "   ";
            uint32_t dot_count = (animation_tick / 5) % 4;
            for(uint32_t i = 0; i < dot_count; i++) {
                dots[i] = '.';
            }
            
            canvas_draw_str(canvas, 2, 36, "Creating ticket");
            canvas_draw_str(canvas, 90, 36, dots);
            canvas_draw_str(canvas, 2, 46, "3DES signing");
            canvas_draw_str(canvas, 90, 46, dots);
            canvas_draw_str(canvas, 2, 56, "Contract ready");
            canvas_draw_str(canvas, 90, 56, dots);
            break;
            
        case TicketBuyStateReady:
            // Display purchased ticket
            char ticket_header[32];
            snprintf(ticket_header, sizeof(ticket_header), "🎫 %s", city_names[selected_city]);
            canvas_draw_str(canvas, 2, 10, ticket_header);
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 22, "Ticket purchased! ✓");
            
            // Big balance display
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 15, 36, ticket_prices[selected_city]);
            
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 48, "Trips: 50 remaining");
            canvas_draw_str(canvas, 2, 57, "Valid: 2025-12-31");
            
            canvas_draw_str(canvas, 2, 64, "[OK] Ready for validator");
            break;
    }
}

static bool calypso_buy_ticket_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type != InputTypeShort) return false;
    
    if(event->key == InputKeyBack) {
        if(ticket_state == TicketBuyStateSelecting) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    
    if(ticket_state == TicketBuyStateSelecting) {
        // Navigate city selection
        if(event->key == InputKeyUp) {
            if(selected_city > 0) {
                selected_city--;
            }
            return true;
        }
        if(event->key == InputKeyDown) {
            if(selected_city < 4) {
                selected_city++;
            }
            return true;
        }
    }
    
    if(event->key == InputKeyOk) {
        switch(ticket_state) {
            case TicketBuyStateSelecting:
                // Start purchase animation
                ticket_state = TicketBuyStatePurchasing;
                animation_tick = 0;
                
                FURI_LOG_I("Calypso", "Buying ticket for %s - %s", 
                          city_names[selected_city], ticket_prices[selected_city]);
                return true;
                
            case TicketBuyStateReady:
                // Go to emulation screen
                scene_manager_next_scene(app->scene_manager, PredatorSceneCalypsoEmulateUI);
                return true;
                
            default:
                break;
        }
    }
    
    return false;
}

static void calypso_buy_ticket_timer_callback(void* context) {
    PredatorApp* app = context;
    
    animation_tick++;
    
    // After animation, show ticket
    if(ticket_state == TicketBuyStatePurchasing && animation_tick > 30) {
        ticket_state = TicketBuyStateReady;
        
        FURI_LOG_I("Calypso", "✓ TL Mobilis ticket created: 100 CHF, 50 trips");
    }
    
    // Trigger redraw
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}

void predator_scene_calypso_buy_ticket_ui_on_enter(void* context) {
    PredatorApp* app = context;
    
    ticket_state = TicketBuyStateSelecting;
    selected_city = TicketCityLausanne;  // Default to TL Lausanne
    animation_tick = 0;
    
    FURI_LOG_I("Calypso", "Buy Ticket menu - TL Lausanne is default");
    
    // Create view
    View* view = view_alloc();
    view_set_context(view, app);
    view_set_draw_callback(view, calypso_buy_ticket_draw_callback);
    view_set_input_callback(view, calypso_buy_ticket_input_callback);
    
    view_dispatcher_add_view(app->view_dispatcher, 100, view); // Temporary ID
    view_dispatcher_switch_to_view(app->view_dispatcher, 100);
    
    // Start timer for animation
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    app->timer = furi_timer_alloc(calypso_buy_ticket_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // 100ms ticks
}

bool predator_scene_calypso_buy_ticket_ui_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_calypso_buy_ticket_ui_on_exit(void* context) {
    PredatorApp* app = context;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}
