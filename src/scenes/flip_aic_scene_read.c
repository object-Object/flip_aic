#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3_poller.h>

enum PollerResult {
    PollerResultSuccess,
    PollerResultFailure,
    PollerResultSwitch, // 協定切換事件
};

// 計時器回調：每隔 600 毫秒自動觸發一次切換
static void flip_aic_scene_read_timer_callback(void* context) {
    FlipAIC* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultSwitch);
}

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
            return NfcCommandContinue;
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
        } else if (poller_event->type == Iso15693_3PollerEventTypeError) {
            return NfcCommandContinue;
        }
    }
    return NfcCommandContinue;
}

void flip_aic_scene_read_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    // 分配並啟動 NFC 讀取
    app->nfc_poller = nfc_poller_alloc(app->nfc, app->current_protocol);
    nfc_poller_start(app->nfc_poller, flip_aic_scene_read_poller_callback, app);

    // 分配並啟動計時器 (每 600ms 發送一次切換訊號)
    app->timer = furi_timer_alloc(flip_aic_scene_read_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_ms_to_ticks(600));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewLoading);
}

bool flip_aic_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if (event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
        case PollerResultSuccess:
            // 讀取成功時，立刻停用計時器防止干擾
            furi_timer_stop(app->timer);
            nfc_device_set_data(app->nfc_device, app->current_protocol, nfc_poller_get_data(app->nfc_poller));
            scene_manager_next_scene(app->scene_manager, FlipAICSceneDisplay);
            return true;

        case PollerResultFailure:
            furi_timer_stop(app->timer);
            scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
            return true;

        case PollerResultSwitch:
            // 接收到計時器訊號，執行切換
            if (app->nfc_poller) {
                nfc_poller_stop(app->nfc_poller);
                nfc_poller_free(app->nfc_poller);
                
                // 翻轉目前的協定
                app->current_protocol = (app->current_protocol == NfcProtocolFelica) ? NfcProtocolIso15693_3 : NfcProtocolFelica;
                
                // 重新啟動另一個協定的掃描
                app->nfc_poller = nfc_poller_alloc(app->nfc, app->current_protocol);
                nfc_poller_start(app->nfc_poller, flip_aic_scene_read_poller_callback, app);
            }
            return true;
        }
    }
    return false;
}

void flip_aic_scene_read_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    // 安全清理計時器與 NFC 資源
    if (app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }

    if (app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
}