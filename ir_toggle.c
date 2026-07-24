#include <furi.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>

/* App state */
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    bool ir_is_on;
    bool running;
} IrToggleApp;

/*
 * ISR callback — provides timing data for the IR DMA engine.
 * Runs in interrupt context, so no blocking calls.
 * Returns Ok with level=1 to keep the 38kHz carrier running continuously.
 * Returns LastDone when the app has toggled IR off.
 */
static FuriHalInfraredTxGetDataState tx_isr_callback(void* ctx, uint32_t* duration, bool* level) {
    IrToggleApp* app = (IrToggleApp*)ctx;

    if(app->ir_is_on) {
        *level = true;
        *duration = 50000; /* 50 ms carrier-on chunks, repeats indefinitely */
        return FuriHalInfraredTxGetDataStateOk;
    } else {
        *level = false;
        *duration = 0;
        return FuriHalInfraredTxGetDataStateLastDone;
    }
}

/* Draw callback — runs in GUI thread */
static void draw_callback(Canvas* canvas, void* ctx) {
    IrToggleApp* app = ctx;

    canvas_clear(canvas);

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_set_font(canvas, FontPrimary);
    if(app->ir_is_on) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "IR: ON");
    } else {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "IR: OFF");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, "OK: Toggle   BACK: Exit");

    furi_mutex_release(app->mutex);
}

/* Input callback — enqueues events for the main loop */
static void input_callback(InputEvent* event, void* ctx) {
    IrToggleApp* app = ctx;
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

/* Turn IR on: configure internal LEDs, register ISR, start 38 kHz carrier */
static void ir_turn_on(IrToggleApp* app) {
    furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    furi_hal_infrared_async_tx_set_data_isr_callback(tx_isr_callback, app);
    furi_hal_infrared_async_tx_start(38000, 0.5f);
}

/*
 * Turn IR off.
 * stop() is non-blocking — it sets a flag so the next ISR invocation
 * returns LastDone and the DMA engine frees resources asynchronously.
 * Do NOT call wait_termination() after stop() — both free resources.
 */
static void ir_turn_off(void) {
    furi_hal_infrared_async_tx_stop();
}

/* Entry point */
int32_t ir_toggle_main(void* p) {
    UNUSED(p);

    IrToggleApp* app = malloc(sizeof(IrToggleApp));
    app->ir_is_on = false;
    app->running = true;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    /* Set up GUI */
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* Main event loop */
    InputEvent event;
    while(app->running) {
        FuriStatus status =
            furi_message_queue_get(app->event_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) continue;

        if(event.type == InputTypePress) {
            if(event.key == InputKeyOk) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                if(app->ir_is_on) {
                    app->ir_is_on = false; /* ISR sees this, returns LastDone next firing */
                    ir_turn_off();
                } else {
                    app->ir_is_on = true; /* ISR must see this before TX starts */
                    ir_turn_on(app);
                }
                furi_mutex_release(app->mutex);
                view_port_update(app->view_port);
            } else if(event.key == InputKeyBack) {
                app->running = false;
            }
        }
    }

    /* Cleanup — ensure IR is fully off before freeing resources */
    if(app->ir_is_on) {
        app->ir_is_on = false;
        furi_hal_infrared_async_tx_stop();
        /* Give the DMA ISR time to fire and free resources (1 chunk ≤ 50 ms) */
        furi_delay_ms(60);
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
