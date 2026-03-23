#include "../helpers/aic.h"
#include "../helpers/hex.h"
#include "../helpers/cardio.h"
#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>

// 增加了一个新的事件类型：再次读卡
enum FlipAICSceneDisplayEvent {
    FlipAICSceneDisplayEventMenu,
    FlipAICSceneDisplayEventReadAgain, 
};

static void flip_aic_scene_display_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    switch (result) {
    case DialogExResultLeft:
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipAICSceneDisplayEventMenu);
        break;
    case DialogExResultCenter:
        // 现在按下中间按键，触发“再次读卡”事件
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipAICSceneDisplayEventReadAgain);
        break;
    default:
        break;
    }
}

void flip_aic_scene_display_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);
    FuriString* text = furi_string_alloc();

    if (protocol == NfcProtocolFelica) {
        const FelicaData* data = nfc_device_get_data(app->nfc_device, NfcProtocolFelica);
        AICVendor vendor = aic_vendor(data);
        const char* vendor_name = aic_vendor_name_short(vendor);
        dialog_ex_set_header(app->dialog_ex, vendor_name, 0, 0, AlignLeft, AlignTop);

        uint8_t access_code[10];
        aic_access_code(data, access_code);
        furi_string_cat_str(text, "AIC: ");
        furi_string_cat_hex(text, access_code, sizeof(access_code), "", 2);

        char konami_id[17];
        aic_access_code_felica_konami(data->idm.data, konami_id);
        furi_string_cat_str(text, "\nKID: ");
        furi_string_cat_str(text, konami_id);

        furi_string_cat_str(text, "\nIDm: ");
        furi_string_cat_hex(text, data->idm.data, sizeof(data->idm.data), ":", 1);

        // 【核心修改】进入界面时，若 USB 已连接，则直接自动发送！
        if (cardio_is_connected()) {
            cardio_send_report(CardioReportIdFeliCa, data->idm.data);
        }
    } 
    else if (protocol == NfcProtocolIso15693_3) {
        const Iso15693_3Data* data = nfc_device_get_data(app->nfc_device, NfcProtocolIso15693_3);
        dialog_ex_set_header(app->dialog_ex, "e-amusement", 0, 0, AlignLeft, AlignTop);

        char access_code_str[21];
        aic_access_code_iso15693(data->uid, access_code_str);
        
        furi_string_cat_str(text, "KID: ");
        furi_string_cat_str(text, access_code_str);

        furi_string_cat_str(text, "\nUID: ");
        furi_string_cat_hex(text, data->uid, sizeof(data->uid), ":", 1);

        // 【核心修改】进入界面时，若 USB 已连接，则直接自动发送！
        if (cardio_is_connected()) {
            cardio_send_report(CardioReportIdISO15693, data->uid);
        }
    }

    dialog_ex_set_text(app->dialog_ex, furi_string_get_cstr(text), 0, 12, AlignLeft, AlignTop);
    furi_string_free(text);

    // 更新按钮文字
    dialog_ex_set_left_button_text(app->dialog_ex, "Menu");
    dialog_ex_set_center_button_text(app->dialog_ex, "Read Next"); // 提示用户点击可以连扫

    dialog_ex_set_result_callback(app->dialog_ex, flip_aic_scene_display_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewDialogEx);
}

bool flip_aic_scene_display_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == FlipAICSceneDisplayEventMenu) {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FlipAICSceneMenu);
            return true;
        } else if (event.event == FlipAICSceneDisplayEventReadAgain) {
            // 接收到再次读卡请求，直接退回到 Read 场景，重新启动 NFC 扫描
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FlipAICSceneRead);
            return true;
        }
    } else if (event.type == SceneManagerEventTypeBack) {
        // 物理返回键依然退回到主菜单
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FlipAICSceneMenu);
        return true;
    }
    return false;
}

void flip_aic_scene_display_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;
    dialog_ex_reset(app->dialog_ex);
}