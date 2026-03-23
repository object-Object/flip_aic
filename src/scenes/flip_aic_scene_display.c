#include "../helpers/aic.h"
#include "../helpers/hex.h"
#include "../helpers/cardio.h"
#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h> 

enum FlipAICSceneDisplayEvent {
    FlipAICSceneDisplayEventMenu,
};

static void flip_aic_scene_display_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    switch (result) {
    case DialogExResultLeft:
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipAICSceneDisplayEventMenu);
        break;
    case DialogExResultCenter:
        if (cardio_is_connected()) {
            NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);
            if (protocol == NfcProtocolFelica) {
                const FelicaData* data = nfc_device_get_data(app->nfc_device, NfcProtocolFelica);
                cardio_send_report(CardioReportIdFeliCa, data->idm.data);
            } else if (protocol == NfcProtocolIso15693_3) { // 修正宏
                const Iso15693_3Data* data = nfc_device_get_data(app->nfc_device, NfcProtocolIso15693_3);
                cardio_send_report(CardioReportIdISO15693, data->uid);
            }
        }
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
        furi_string_cat_str(text, "Access code:\n");
        furi_string_cat_hex(text, access_code, sizeof(access_code), " ", 2);

        furi_string_cat_str(text, "\nIDm: ");
        furi_string_cat_hex(text, data->idm.data, sizeof(data->idm.data), ":", 1);
    } 
    else if (protocol == NfcProtocolIso15693_3) { // 修正宏
        const Iso15693_3Data* data = nfc_device_get_data(app->nfc_device, NfcProtocolIso15693_3);
        dialog_ex_set_header(app->dialog_ex, "e-amusement", 0, 0, AlignLeft, AlignTop);

        char access_code_str[21];
        aic_access_code_iso15693(data->uid, access_code_str);
        
        furi_string_cat_str(text, "Access code:\n");
        furi_string_cat_str(text, access_code_str);

        furi_string_cat_str(text, "\nUID: ");
        furi_string_cat_hex(text, data->uid, sizeof(data->uid), ":", 1);
    }

    dialog_ex_set_text(app->dialog_ex, furi_string_get_cstr(text), 0, 12, AlignLeft, AlignTop);
    furi_string_free(text);

    dialog_ex_set_left_button_text(app->dialog_ex, "Menu");
    dialog_ex_set_center_button_text(app->dialog_ex, "CardIO");

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
        }
    } else if (event.type == SceneManagerEventTypeBack) {
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