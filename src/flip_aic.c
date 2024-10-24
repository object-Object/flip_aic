#include "flip_aic.h"
#include "scenes/flip_aic_scene.h"

#include <furi.h>

bool flip_aic_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipAIC* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flip_aic_navigation_event_callback(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static FlipAIC* flip_aic_alloc() {
    FlipAIC* app = malloc(sizeof(FlipAIC));

    app->gui = furi_record_open(RECORD_GUI);

    app->scene_manager = scene_manager_alloc(&flip_aic_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flip_aic_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher,
        flip_aic_navigation_event_callback
    );

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipAICViewSubmenu,
        submenu_get_view(app->submenu)
    );

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipAICViewLoading,
        loading_get_view(app->loading)
    );

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipAICViewDialogEx,
        dialog_ex_get_view(app->dialog_ex)
    );

    app->nfc = nfc_alloc();
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    app->nfc_device = nfc_device_alloc();

    return app;
}

static void flip_aic_free(FlipAIC* app) {
    furi_assert(app);

    nfc_device_free(app->nfc_device);
    nfc_scanner_free(app->nfc_scanner);
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewDialogEx);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewLoading);
    loading_free(app->loading);

    view_dispatcher_remove_view(app->view_dispatcher, FlipAICViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);

    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t flip_aic_app(void* p) {
    UNUSED(p);

    FlipAIC* app = flip_aic_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
    view_dispatcher_run(app->view_dispatcher);

    flip_aic_free(app);

    return 0;
}
