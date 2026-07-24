#include <furi.h>
#include <furi_hal_pwm.h>
#include <furi_hal_resources.h>
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

/* Toggle the IR LED on or off */
static void toggle_ir(IrToggleApp* app) {
    if(app->ir_is_on) {
        furi_hal_pwm_stop(FuriHalPwmOutputIdTim1PA7);
        app->ir_is_on = false;
    } else {
        furi_hal_pwm_start(FuriHalPwmOutputIdTim1PA7, 38000, 50);
        app->ir_is_on = true;
    }
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
                toggle_ir(app);
                furi_mutex_release(app->mutex);
                view_port_update(app->view_port);
            } else if(event.key == InputKeyBack) {
                app->running = false;
            }
        }
    }

    /* Cleanup — ensure IR is off */
    if(app->ir_is_on) {
        furi_hal_pwm_stop(FuriHalPwmOutputIdTim1PA7);
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
