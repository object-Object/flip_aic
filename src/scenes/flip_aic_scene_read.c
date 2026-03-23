#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3_poller.h> // 修正头文件路径

enum PollerResult {
    PollerResultSuccess,
    PollerResultFailure,
};

static NfcCommand flip_aic_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    if (event.protocol == NfcProtocolFelica) {
        FelicaPollerEvent* poller_event = event.event_data;
        switch (poller_event->type) {
        case FelicaPollerEventTypeReady:
        case FelicaPollerEventTypeIncomplete:
            view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultSuccess);
            return NfcCommandStop;
        case FelicaPollerEventTypeError:
            view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultFailure);
            return NfcCommandStop;
        case FelicaPollerEventTypeRequestAuthContext:
            poller_event->data->auth_context->skip_auth = true;
            return NfcCommandContinue;
        default:
            return NfcCommandContinue;
        }
    } else if (event.protocol == NfcProtocolIso15693_3) { 
        Iso15693_3PollerEvent* poller_event = event.event_data;
        if (poller_event->type == Iso15693_3PollerEventTypeReady) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultSuccess);
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

void flip_aic_scene_read_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    app->nfc_poller = nfc_poller_alloc(app->nfc, app->current_protocol);
    nfc_poller_start(app->nfc_poller, flip_aic_scene_read_poller_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewLoading);
}

bool flip_aic_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if (event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
        case PollerResultSuccess:
            nfc_device_set_data(
                app->nfc_device,
                app->current_protocol,
                nfc_poller_get_data(app->nfc_poller)
            );
            scene_manager_next_scene(app->scene_manager, FlipAICSceneDisplay);
            return true;

        case PollerResultFailure:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
            return true;
        }
    }
    return false;
}

void flip_aic_scene_read_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);
}