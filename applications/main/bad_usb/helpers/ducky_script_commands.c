#include <furi_hal.h>
#include <lib/toolbox/strint.h>
#include "ducky_script.h"
#include "ducky_script_i.h"

typedef int32_t (*DuckyCmdCallback)(BadUsbScript* bad_usb, const char* line, int32_t param);

typedef struct {
    char* name;
    DuckyCmdCallback callback;
    int32_t param;
} DuckyCmd;

static int32_t ducky_fnc_delay(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint32_t delay_val = 0;
    bool state = ducky_get_number(line, &delay_val);
    if((state) && (delay_val > 0)) {
        return (int32_t)delay_val;
    }

    return ducky_error(bad_usb, "Invalid number %s", line);
}

static int32_t ducky_fnc_defdelay(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_usb->defdelay);
    if(!state) {
        return ducky_error(bad_usb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_strdelay(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_usb->stringdelay);
    if(!state) {
        return ducky_error(bad_usb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_defstrdelay(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_usb->defstringdelay);
    if(!state) {
        return ducky_error(bad_usb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_string(BadUsbScript* bad_usb, const char* line, int32_t param) {
    line = &line[ducky_get_command_len(line) + 1];
    furi_string_set_str(bad_usb->string_print, line);
    if(param == 1) {
        furi_string_cat(bad_usb->string_print, "\n");
    }

    if(bad_usb->stringdelay == 0 &&
       bad_usb->defstringdelay == 0) { // stringdelay not set - run command immediately
        bool state = ducky_string(bad_usb, furi_string_get_cstr(bad_usb->string_print));
        if(!state) {
            return ducky_error(bad_usb, "Invalid string %s", line);
        }
    } else { // stringdelay is set - run command in thread to keep handling external events
        return SCRIPT_STATE_STRING_START;
    }

    return 0;
}

static int32_t ducky_fnc_repeat(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_usb->repeat_cnt);
    if((!state) || (bad_usb->repeat_cnt == 0)) {
        return ducky_error(bad_usb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_sysrq(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_usb, line, true);
    bad_usb->hid->kb_press(bad_usb->hid_inst, KEY_MOD_LEFT_ALT | HID_KEYBOARD_PRINT_SCREEN);
    bad_usb->hid->kb_press(bad_usb->hid_inst, key);
    bad_usb->hid->release_all(bad_usb->hid_inst);
    return 0;
}

static int32_t ducky_fnc_altchar(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    ducky_numlock_on(bad_usb);
    bool state = ducky_altchar(bad_usb, line);
    if(!state) {
        return ducky_error(bad_usb, "Invalid altchar %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_altstring(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    ducky_numlock_on(bad_usb);
    bool state = ducky_altstring(bad_usb, line);
    if(!state) {
        return ducky_error(bad_usb, "Invalid altstring %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_hold(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_usb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_usb, "No keycode defined for %s", line);
    }
    bad_usb->key_hold_nb++;
    if(bad_usb->key_hold_nb > (HID_KB_MAX_KEYS - 1)) {
        return ducky_error(bad_usb, "Too many keys are hold");
    }
    bad_usb->hid->kb_press(bad_usb->hid_inst, key);
    return 0;
}

static int32_t ducky_fnc_release(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_usb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_usb, "No keycode defined for %s", line);
    }
    if(bad_usb->key_hold_nb == 0) {
        return ducky_error(bad_usb, "No keys are hold");
    }
    bad_usb->key_hold_nb--;
    bad_usb->hid->kb_release(bad_usb->hid_inst, key);
    return 0;
}

static int32_t ducky_fnc_media(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_media_keycode_by_name(line);
    if(key == HID_CONSUMER_UNASSIGNED) {
        return ducky_error(bad_usb, "No keycode defined for %s", line);
    }
    bad_usb->hid->consumer_press(bad_usb->hid_inst, key);
    bad_usb->hid->consumer_release(bad_usb->hid_inst, key);
    return 0;
}

static int32_t ducky_fnc_globe(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_usb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_usb, "No keycode defined for %s", line);
    }

    bad_usb->hid->consumer_press(bad_usb->hid_inst, HID_CONSUMER_FN_GLOBE);
    bad_usb->hid->kb_press(bad_usb->hid_inst, key);
    bad_usb->hid->kb_release(bad_usb->hid_inst, key);
    bad_usb->hid->consumer_release(bad_usb->hid_inst, HID_CONSUMER_FN_GLOBE);
    return 0;
}

static int32_t ducky_fnc_waitforbutton(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    UNUSED(bad_usb);
    UNUSED(line);

    return SCRIPT_STATE_WAIT_FOR_BTN;
}

static int32_t ducky_fnc_mouse_leftclick(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    UNUSED(line);

    bad_usb->hid->mouse_press(bad_usb->hid_inst, HID_MOUSE_BTN_LEFT);
    bad_usb->hid->mouse_release(bad_usb->hid_inst, HID_MOUSE_BTN_LEFT);
    return 0;
}

static int32_t ducky_fnc_mouse_rightclick(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    UNUSED(line);

    bad_usb->hid->mouse_press(bad_usb->hid_inst, HID_MOUSE_BTN_RIGHT);
    bad_usb->hid->mouse_release(bad_usb->hid_inst, HID_MOUSE_BTN_RIGHT);
    return 0;
}

static int32_t ducky_fnc_mouse_middleclick(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    UNUSED(line);

    bad_usb->hid->mouse_press(bad_usb->hid_inst, HID_MOUSE_BTN_WHEEL);
    bad_usb->hid->mouse_release(bad_usb->hid_inst, HID_MOUSE_BTN_WHEEL);
    return 0;
}

static int32_t ducky_fnc_mouse_scroll(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[strcspn(line, " ") + 1];
    line = &line[strcspn(line, " ") + 1];
    uint32_t mouse_scroll_count = 0;
    bool state = ducky_get_number(line, &mouse_scroll_count); // will break since numbers can be negative

    if (!state) {
        return ducky_error(bad_usb, "Invalid number %s", line);
    }

    bad_usb->hid->mouse_scroll(bad_usb->hid_inst, mouse_scroll_count);
    return 0; 
}

static int32_t ducky_fnc_mouse_move(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    line = &line[strcspn(line, " ") + 1];
    line = &line[strcspn(line, " ") + 1];
    int32_t mouse_move_x = 0;
    int32_t mouse_move_y = 0;

    if (strint_to_int32(line, NULL, &mouse_move_x, 10) != StrintParseNoError) {
        return ducky_error(bad_usb, "Invalid Number %s", line);
    }

    line = &line[strcspn(line, " ") + 1];

    if (strint_to_int32(line, NULL, &mouse_move_y, 10) != StrintParseNoError) {
        return ducky_error(bad_usb, "Invalid Number %s", line);
    }

    bad_usb->hid->mouse_move(bad_usb->hid_inst, mouse_move_x, mouse_move_y);

    return 0;
}

static const DuckyCmd ducky_mouse_commands[] = {
    {"LEFTCLICK", ducky_fnc_mouse_leftclick, 0},
    {"RIGHTCLICK", ducky_fnc_mouse_rightclick, 0},
    {"MIDDLECLICK", ducky_fnc_mouse_middleclick, 0},
    {"SCROLL", ducky_fnc_mouse_scroll, 0},
    {"MOVE", ducky_fnc_mouse_move, -1},
};

static int32_t ducky_fnc_mouse(BadUsbScript* bad_usb, const char* line, int32_t param) {
    UNUSED(param);
    FURI_LOG_D("Worker", "ducky_fnc_mouse");

    const char* mouse_cmd = &line[strcspn(line, " ") + 1];
    size_t mouse_cmd_len = strcspn(mouse_cmd, " ");

    for (size_t i = 0; i < COUNT_OF(ducky_mouse_commands); i++) {
        size_t cmd_cmp_len = strlen(ducky_mouse_commands[i].name);

        if (mouse_cmd_len != cmd_cmp_len) {
            continue;
        }

        if (strncmp(ducky_mouse_commands[i].name, mouse_cmd, mouse_cmd_len) == 0) {
            if (ducky_mouse_commands[i].callback == NULL) {
                return 0;
            } else {
                return (ducky_mouse_commands[i].callback)(bad_usb, line, ducky_mouse_commands[i].param);
            }
        }
    }

    return SCRIPT_STATE_CMD_UNKNOWN; 
}

static const DuckyCmd ducky_commands[] = {
    {"REM", NULL, -1},
    {"ID", NULL, -1},
    {"DELAY", ducky_fnc_delay, -1},
    {"STRING", ducky_fnc_string, 0},
    {"STRINGLN", ducky_fnc_string, 1},
    {"DEFAULT_DELAY", ducky_fnc_defdelay, -1},
    {"DEFAULTDELAY", ducky_fnc_defdelay, -1},
    {"STRINGDELAY", ducky_fnc_strdelay, -1},
    {"STRING_DELAY", ducky_fnc_strdelay, -1},
    {"DEFAULT_STRING_DELAY", ducky_fnc_defstrdelay, -1},
    {"DEFAULTSTRINGDELAY", ducky_fnc_defstrdelay, -1},
    {"REPEAT", ducky_fnc_repeat, -1},
    {"SYSRQ", ducky_fnc_sysrq, -1},
    {"ALTCHAR", ducky_fnc_altchar, -1},
    {"ALTSTRING", ducky_fnc_altstring, -1},
    {"ALTCODE", ducky_fnc_altstring, -1},
    {"HOLD", ducky_fnc_hold, -1},
    {"RELEASE", ducky_fnc_release, -1},
    {"WAIT_FOR_BUTTON_PRESS", ducky_fnc_waitforbutton, -1},
    {"MEDIA", ducky_fnc_media, -1},
    {"GLOBE", ducky_fnc_globe, -1},
    {"MOUSE", ducky_fnc_mouse, -1},
};

#define TAG "BadUsb"

#define WORKER_TAG TAG "Worker"

int32_t ducky_execute_cmd(BadUsbScript* bad_usb, const char* line) {
    size_t cmd_word_len = strcspn(line, " ");
    for(size_t i = 0; i < COUNT_OF(ducky_commands); i++) {
        size_t cmd_compare_len = strlen(ducky_commands[i].name);

        if(cmd_compare_len != cmd_word_len) {
            continue;
        }

        if(strncmp(line, ducky_commands[i].name, cmd_compare_len) == 0) {
            if(ducky_commands[i].callback == NULL) {
                return 0;
            } else {
                return (ducky_commands[i].callback)(bad_usb, line, ducky_commands[i].param);
            }
        }
    }

    return SCRIPT_STATE_CMD_UNKNOWN;
}
