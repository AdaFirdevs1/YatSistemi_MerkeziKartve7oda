#include "usrGraphicalInterface.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include <time.h>
#include <string.h>
#include "usrSettingsPage.h"

static const char *s_tag = "CentralMasterGUI";  // Sadece bir tane
static uint8_t g_selected_room_id = 0;  

// ========== FORWARD DECLARATIONS ==========
typedef enum {
    PASSWORD_MODE_SETUP,
    PASSWORD_MODE_ENTRY,
    PASSWORD_MODE_CHANGE
} password_interface_mode_t;

static void create_main_menu(void);
static void create_password_interface(password_interface_mode_t mode);
static void password_numpad_event_cb(lv_event_t *e);
static void hide_main_menu(void);
static void show_main_menu(void);
static void update_theme_colors(void);
static void update_password_theme_colors(void);
static void room_button_event_cb(lv_event_t *e);


// ========== GLOBAL VARIABLES ==========
static usrGraphicalInterface_t gui = {0};


// ========== COLOR PALETTE - DAY MODE ==========
#define COLOR_BG_MAIN_LIGHT     lv_color_make(240, 245, 250)
#define COLOR_BG_PANEL_LIGHT    lv_color_make(255, 255, 255)
#define COLOR_TEXT_PRIMARY_LIGHT  lv_color_make(44, 62, 80)
#define COLOR_TEXT_SECONDARY_LIGHT lv_color_make(149, 165, 166)
#define COLOR_CARD_BG_LIGHT     lv_color_make(255, 255, 255)
#define COLOR_ACCENT_BLUE_DAY   lv_color_make(52, 152, 219)
#define COLOR_ICON_BLUE         lv_color_make(52, 152, 219)

// ========== COLOR PALETTE - NIGHT MODE ==========
#define COLOR_BG_MAIN_DARK      lv_color_make(8, 15, 22)
#define COLOR_BG_PANEL_DARK     lv_color_make(12, 20, 28)
#define COLOR_TEXT_PRIMARY_DARK lv_color_make(255, 255, 255)
#define COLOR_TEXT_SECONDARY_DARK lv_color_make(138, 155, 179)
#define COLOR_CARD_BG_DARK      lv_color_make(15, 25, 35)
#define COLOR_SIDEBAR_BG_DARK   lv_color_make(10, 18, 26)
#define COLOR_NEON_CYAN         lv_color_make(0, 210, 255)
#define COLOR_NEON_BLUE         lv_color_make(0, 163, 224)

// ========== PAGE NAMES ==========
static const char *page_names[PAGE_COUNT] = {
    "MAIN",
    "TOILET",
    "LIGHTING",
    "SOCKETS",
    "SETTINGS",
    "PASSWORD_SETUP",
    "PASSWORD_ENTRY",
    "PASSWORD_CHANGE"
};

// ========== MAIN MENU OBJECTS ==========
static lv_obj_t *room_menu_buttons[7] = {NULL};
static lv_obj_t *room_menu_labels[7]  = {NULL};
static lv_obj_t *room_menu_icons[7]   = {NULL};

static lv_obj_t *settings_button    = NULL;
static lv_obj_t *dark_mode_button   = NULL;
static lv_obj_t *main_title_label   = NULL;
static lv_obj_t *main_subtitle_label= NULL;
static lv_obj_t *logo_label         = NULL;
static lv_obj_t *brand_label        = NULL;
static lv_obj_t *back_button        = NULL;
static lv_obj_t *header_container   = NULL;
static lv_obj_t *content_container  = NULL;

// ========== PASSWORD UI OBJECTS ==========
static lv_obj_t *password_container      = NULL;
static lv_obj_t *password_title          = NULL;
static lv_obj_t *password_subtitle       = NULL;
static lv_obj_t *password_description    = NULL;
static lv_obj_t *password_dots[6]        = {NULL};
static lv_obj_t *password_numpad_buttons[12] = {NULL};
static lv_obj_t *password_enter_btn      = NULL;
static lv_obj_t *password_status_label   = NULL;
static lv_obj_t *password_home_btn       = NULL;
static char      current_password[7]     = {0};
static int       password_length         = 0;
static bool      is_password_setup_mode  = false;

// Password change state
static enum {
    PASSWORD_CHANGE_STEP_OLD,
    PASSWORD_CHANGE_STEP_NEW
} password_change_step = PASSWORD_CHANGE_STEP_OLD;

static char old_password_temp[7] = {0};

// ========== DARK MODE ==========
static bool is_dark_mode = true;

// ========== THEME HELPER FUNCTIONS ==========
static lv_color_t get_bg_main_color(void) {
    return is_dark_mode ? COLOR_BG_MAIN_DARK : COLOR_BG_MAIN_LIGHT;
}
static lv_color_t get_bg_panel_color(void) {
    return is_dark_mode ? COLOR_BG_PANEL_DARK : COLOR_BG_PANEL_LIGHT;
}
static lv_color_t get_text_primary_color(void) {
    return is_dark_mode ? COLOR_TEXT_PRIMARY_DARK : COLOR_TEXT_PRIMARY_LIGHT;
}
static lv_color_t get_text_secondary_color(void) {
    return is_dark_mode ? COLOR_TEXT_SECONDARY_DARK : COLOR_TEXT_SECONDARY_LIGHT;
}
static lv_color_t get_card_bg_color(void) {
    return is_dark_mode ? lv_color_make(15, 25, 35) : lv_color_make(255, 255, 255);
}
static lv_color_t get_numpad_button_color(void) {
    return is_dark_mode ? lv_color_make(20, 30, 40) : lv_color_make(248, 250, 252);
}
static lv_color_t get_accent_color(void) {
    return is_dark_mode ? COLOR_NEON_CYAN : COLOR_ACCENT_BLUE_DAY;
}

// ========== PUBLIC: DARK MODE ==========
bool usrGraphicalInterface_isDarkMode(void) {
    return is_dark_mode;
}

void usrGraphicalInterface_setDarkMode(bool dark_mode)
{
    if (is_dark_mode == dark_mode) return;

    is_dark_mode = dark_mode;
    ESP_LOGI(s_tag, "Theme: %s MODE", dark_mode ? "DARK" : "LIGHT");

    usrToiletPage_updateThemeColors();
    usrLightingPage_updateThemeColors();
    usrSocketsPage_updateThemeColors();
    usrSettingsPage_updateThemeColors();

    update_theme_colors();
    update_password_theme_colors();
}

// ========== PUBLIC: ROOM SELECTION ==========
uint8_t usrGraphicalInterface_getSelectedRoom(void) {
    return g_selected_room_id;
}

void usrGraphicalInterface_setSelectedRoom(uint8_t room_id) {
    g_selected_room_id = room_id;
    ESP_LOGI(s_tag, "Selected room: %d", room_id);
}

// ========== NVS DEBUG ==========
void debug_nvs_contents(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "NVS open error: %s", esp_err_to_name(err));
        return;
    }
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    if (err == ESP_OK) {
        char *buffer = malloc(required_size);
        if (buffer) {
            nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, buffer, &required_size);
            ESP_LOGI(s_tag, "NVS password size: %d, value: '%s'", required_size, buffer);
            free(buffer);
        }
    } else {
        ESP_LOGI(s_tag, "NVS password not found: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
}

// ========== PASSWORD MANAGEMENT ==========
bool usrGraphicalInterface_checkPasswordExists(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return false;

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    nvs_close(nvs_handle);

    bool exists = (err == ESP_OK && required_size > 1);
    ESP_LOGI(s_tag, "Password exists: %s", exists ? "YES" : "NO");
    return exists;
}

bool usrGraphicalInterface_savePassword(const char *password)
{
    if (!password || strlen(password) == 0) return false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return false;

    err = nvs_set_str(nvs_handle, NVS_PASSWORD_KEY, password);
    if (err != ESP_OK) { nvs_close(nvs_handle); return false; }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (err != ESP_OK) return false;

    strncpy(gui.stored_password, password, MAX_PASSWORD_LENGTH);
    gui.stored_password[MAX_PASSWORD_LENGTH] = '\0';
    gui.password_set = true;
    ESP_LOGI(s_tag, "Password saved");
    return true;
}

bool usrGraphicalInterface_verifyPassword(const char *password)
{
    if (!password) return false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return false;

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    if (err != ESP_OK || required_size == 0) { nvs_close(nvs_handle); return false; }

    char stored[MAX_PASSWORD_LENGTH + 1] = {0};
    if (required_size > sizeof(stored)) { nvs_close(nvs_handle); return false; }

    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, stored, &required_size);
    nvs_close(nvs_handle);
    if (err != ESP_OK) return false;

    bool ok = (strcmp(password, stored) == 0);
    ESP_LOGI(s_tag, "Password verify: %s", ok ? "OK" : "FAIL");
    if (ok) {
        strncpy(gui.stored_password, stored, MAX_PASSWORD_LENGTH);
        gui.stored_password[MAX_PASSWORD_LENGTH] = '\0';
        gui.password_set = true;
    }
    return ok;
}

void usrGraphicalInterface_resetPasswordVerification(void) {
    gui.password_verified = false;
    ESP_LOGI(s_tag, "Password verification reset");
}

// ========== PASSWORD TIMER CALLBACKS ==========
static void password_setup_success_timer_cb(lv_timer_t *timer)
{
    if (main_title_label == NULL) create_main_menu();
    usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    lv_timer_del(timer);
}

static void password_entry_success_timer_cb(lv_timer_t *timer)
{
    usrGraphicalInterface_showPage(PAGE_SETTINGS);
    lv_timer_del(timer);
}

// ========== PASSWORD DOTS ==========
static void update_password_dots(void)
{
    for (int i = 0; i < 6; i++) {
        if (password_dots[i] == NULL) continue;
        if (i < password_length) {
            lv_obj_set_style_bg_color(password_dots[i], get_accent_color(), 0);
            lv_obj_set_style_bg_opa(password_dots[i], LV_OPA_100, 0);
            lv_obj_set_style_border_color(password_dots[i], get_accent_color(), 0);
        } else {
            lv_obj_set_style_bg_color(password_dots[i], get_bg_main_color(), 0);
            lv_obj_set_style_bg_opa(password_dots[i], LV_OPA_100, 0);
            lv_obj_set_style_border_color(password_dots[i], get_text_secondary_color(), 0);
        }
    }
}

// ========== PASSWORD NUMPAD EVENT ==========
static void password_numpad_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);
    const char *btn_text = lv_label_get_text(lv_obj_get_child(btn, 0));

    if (strcmp(btn_text, LV_SYMBOL_BACKSPACE) == 0) {
        if (password_length > 0) {
            password_length--;
            current_password[password_length] = '\0';
            update_password_dots();
        }
    } else if (strcmp(btn_text, LV_SYMBOL_CLOSE) == 0) {
        password_length = 0;
        memset(current_password, 0, sizeof(current_password));
        update_password_dots();
        if (password_status_label) lv_label_set_text(password_status_label, "");
    } else if (password_length < 6) {
        current_password[password_length] = btn_text[0];
        password_length++;
        current_password[password_length] = '\0';
        update_password_dots();
    }
}

// ========== PASSWORD ENTER EVENT ==========
static void password_enter_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (password_length < 6) {
        lv_label_set_text(password_status_label, "PIN must be 6 digits.");
        lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
        return;
    }

    if (is_password_setup_mode) {
        if (usrGraphicalInterface_savePassword(current_password)) {
            lv_label_set_text(password_status_label, "Password saved!");
            lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
            lv_timer_t *t = lv_timer_create(password_setup_success_timer_cb, 1500, NULL);
            lv_timer_set_repeat_count(t, 1);
        } else {
            lv_label_set_text(password_status_label, "Save failed!");
            lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
        }
    } else if (gui.current_page == PAGE_PASSWORD_CHANGE) {
        if (password_change_step == PASSWORD_CHANGE_STEP_OLD) {
            if (usrGraphicalInterface_verifyPassword(current_password)) {
                strncpy(old_password_temp, current_password, 6);
                password_change_step = PASSWORD_CHANGE_STEP_NEW;
                lv_label_set_text(password_title, "NEW PASSWORD");
                lv_label_set_text(password_subtitle, "Set Your\nNew PIN");
                lv_label_set_text(password_description, "Enter your\nnew 6-digit PIN.");
                lv_label_set_text(password_status_label, "");
                password_length = 0;
                memset(current_password, 0, sizeof(current_password));
                update_password_dots();
            } else {
                lv_label_set_text(password_status_label, "Incorrect password!");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
                password_length = 0;
                memset(current_password, 0, sizeof(current_password));
                update_password_dots();
            }
        } else {
            if (usrGraphicalInterface_savePassword(current_password)) {
                lv_label_set_text(password_status_label, "Password changed!");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
                lv_timer_t *t = lv_timer_create(password_entry_success_timer_cb, 1500, NULL);
                lv_timer_set_repeat_count(t, 1);
                password_change_step = PASSWORD_CHANGE_STEP_OLD;
                memset(old_password_temp, 0, sizeof(old_password_temp));
            } else {
                lv_label_set_text(password_status_label, "Save failed!");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
            }
        }
    } else {
        if (usrGraphicalInterface_verifyPassword(current_password)) {
            gui.password_verified = true;
            lv_label_set_text(password_status_label, "Access granted!");
            lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
            lv_timer_t *t = lv_timer_create(password_entry_success_timer_cb, 1000, NULL);
            lv_timer_set_repeat_count(t, 1);
        } else {
            lv_label_set_text(password_status_label, "Incorrect password");
            lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
            password_length = 0;
            memset(current_password, 0, sizeof(current_password));
            update_password_dots();
        }
    }
}

// ========== PASSWORD HOME BUTTON ==========
static void password_home_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (gui.current_page == PAGE_PASSWORD_CHANGE) {
        password_change_step = PASSWORD_CHANGE_STEP_OLD;
        memset(old_password_temp, 0, sizeof(old_password_temp));
        usrGraphicalInterface_showPage(PAGE_SETTINGS);
    } else {
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    }
}

// ========== CREATE PASSWORD INTERFACE ==========
static void create_password_interface(password_interface_mode_t mode)
{
    is_password_setup_mode = (mode == PASSWORD_MODE_SETUP);
    password_length = 0;
    memset(current_password, 0, sizeof(current_password));

    password_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(password_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(password_container, get_bg_main_color(), 0);
    lv_obj_set_style_border_width(password_container, 0, 0);
    lv_obj_set_style_pad_all(password_container, 0, 0);
    lv_obj_clear_flag(password_container, LV_OBJ_FLAG_SCROLLABLE);

    // Left panel
    lv_obj_t *left_panel = lv_obj_create(password_container);
    lv_obj_set_size(left_panel, LV_HOR_RES / 2 - 20, LV_VER_RES - 30);
    lv_obj_set_pos(left_panel, 60, 15);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 20, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    password_title = lv_label_create(left_panel);
    lv_label_set_text(password_title,
        (mode == PASSWORD_MODE_CHANGE) ? "CURRENT PASSWORD" : "SECURITY PANEL");
    lv_obj_set_style_text_font(password_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(password_title, get_accent_color(), 0);
    lv_obj_align(password_title, LV_ALIGN_TOP_LEFT, 0, 40);

    password_subtitle = lv_label_create(left_panel);
    if (mode == PASSWORD_MODE_SETUP)
        lv_label_set_text(password_subtitle, "Create\nYour PIN");
    else if (mode == PASSWORD_MODE_CHANGE)
        lv_label_set_text(password_subtitle, "Verify Your\nCurrent PIN");
    else
        lv_label_set_text(password_subtitle, "Unlock\nYour System");
    lv_obj_set_style_text_font(password_subtitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(password_subtitle, get_text_primary_color(), 0);
    lv_obj_align(password_subtitle, LV_ALIGN_TOP_LEFT, 0, 80);

    password_description = lv_label_create(left_panel);
    if (mode == PASSWORD_MODE_SETUP)
        lv_label_set_text(password_description, "Create a\n6-digit PIN.");
    else if (mode == PASSWORD_MODE_CHANGE)
        lv_label_set_text(password_description, "Enter current\n6-digit PIN\nto change it.");
    else
        lv_label_set_text(password_description, "Enter your\n6-digit PIN\nto access settings.");
    lv_obj_set_style_text_font(password_description, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(password_description, get_text_secondary_color(), 0);
    lv_obj_set_width(password_description, LV_HOR_RES / 2 - 60);
    lv_label_set_long_mode(password_description, LV_LABEL_LONG_WRAP);
    lv_obj_align(password_description, LV_ALIGN_TOP_LEFT, 0, 170);

    password_status_label = lv_label_create(left_panel);
    lv_label_set_text(password_status_label, "");
    lv_obj_set_style_text_font(password_status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(password_status_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Right panel
    lv_obj_t *right_panel = lv_obj_create(password_container);
    lv_obj_set_size(right_panel, LV_HOR_RES / 2 - 20, LV_VER_RES - 30);
    lv_obj_set_pos(right_panel, LV_HOR_RES / 2 + 5, 15);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 15, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Dots
    lv_obj_t *dots_container = lv_obj_create(right_panel);
    lv_obj_set_size(dots_container, 320, 40);
    lv_obj_align(dots_container, LV_ALIGN_TOP_MID, -20, 30);
    lv_obj_set_style_bg_opa(dots_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_container, 0, 0);
    lv_obj_set_style_pad_all(dots_container, 0, 0);
    lv_obj_set_flex_flow(dots_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dots_container, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 6; i++) {
        password_dots[i] = lv_obj_create(dots_container);
        lv_obj_set_size(password_dots[i], 16, 16);
        lv_obj_set_style_radius(password_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(password_dots[i], get_bg_main_color(), 0);
        lv_obj_set_style_bg_opa(password_dots[i], LV_OPA_100, 0);
        lv_obj_set_style_border_width(password_dots[i], 2, 0);
        lv_obj_set_style_border_color(password_dots[i], get_text_secondary_color(), 0);
        lv_obj_set_style_border_opa(password_dots[i], LV_OPA_50, 0);
        if (i < 5) lv_obj_set_style_pad_right(password_dots[i], 15, 0);
    }

    // Numpad
    lv_obj_t *numpad_container = lv_obj_create(right_panel);
    lv_obj_set_size(numpad_container, 280, 320);
    lv_obj_align(numpad_container, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_opa(numpad_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(numpad_container, 0, 0);
    lv_obj_set_style_pad_all(numpad_container, 0, 0);
    lv_obj_clear_flag(numpad_container, LV_OBJ_FLAG_SCROLLABLE);

    const char *numpad_map[] = {
        "1","2","3","4","5","6","7","8","9",
        LV_SYMBOL_CLOSE, "0", LV_SYMBOL_BACKSPACE
    };
    int btn_size = 68, spacing = 15;

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            password_numpad_buttons[idx] = lv_btn_create(numpad_container);
            lv_obj_set_size(password_numpad_buttons[idx], btn_size, btn_size);
            lv_obj_set_pos(password_numpad_buttons[idx],
                           col * (btn_size + spacing), row * (btn_size + spacing));
            lv_obj_set_style_bg_color(password_numpad_buttons[idx], get_numpad_button_color(), 0);
            lv_obj_set_style_border_width(password_numpad_buttons[idx], 0, 0);
            lv_obj_set_style_radius(password_numpad_buttons[idx], 15, 0);

            lv_obj_t *btn_label = lv_label_create(password_numpad_buttons[idx]);
            lv_label_set_text(btn_label, numpad_map[idx]);
            lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
            if (idx == 9)
                lv_obj_set_style_text_color(btn_label, lv_color_make(239, 68, 68), 0);
            else if (idx == 11)
                lv_obj_set_style_text_color(btn_label, get_accent_color(), 0);
            else
                lv_obj_set_style_text_color(btn_label, get_text_primary_color(), 0);
            lv_obj_center(btn_label);
            lv_obj_add_event_cb(password_numpad_buttons[idx], password_numpad_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    // Home button (setup modunda gösterme)
    if (mode != PASSWORD_MODE_SETUP) {
        password_home_btn = lv_btn_create(password_container);
        lv_obj_set_size(password_home_btn, 50, 50);
        lv_obj_align(password_home_btn, LV_ALIGN_TOP_RIGHT, -60, 20);
        lv_obj_set_style_bg_color(password_home_btn, get_bg_panel_color(), 0);
        lv_obj_set_style_bg_opa(password_home_btn, LV_OPA_50, 0);
        lv_obj_set_style_border_width(password_home_btn, 1, 0);
        lv_obj_set_style_border_color(password_home_btn, get_text_secondary_color(), 0);
        lv_obj_set_style_radius(password_home_btn, 8, 0);
        lv_obj_set_style_shadow_width(password_home_btn, 0, 0);

        lv_obj_t *home_icon = lv_label_create(password_home_btn);
        lv_label_set_text(home_icon, LV_SYMBOL_HOME);
        lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(home_icon, get_accent_color(), 0);
        lv_obj_center(home_icon);
        lv_obj_add_event_cb(password_home_btn, password_home_button_event_cb, LV_EVENT_CLICKED, NULL);
    } else {
        password_home_btn = NULL;
    }

    // Enter button
    lv_obj_t *password_enter_btn_obj = lv_btn_create(left_panel);
    lv_obj_set_size(password_enter_btn_obj, 280, 55);
    lv_obj_align(password_enter_btn_obj, LV_ALIGN_BOTTOM_MID, -30, -20);
    lv_obj_set_style_bg_color(password_enter_btn_obj, get_accent_color(), 0);
    lv_obj_set_style_border_width(password_enter_btn_obj, 0, 0);
    lv_obj_set_style_radius(password_enter_btn_obj, 12, 0);

    lv_obj_t *enter_label = lv_label_create(password_enter_btn_obj);
    lv_label_set_text(enter_label, "ENTER");
    lv_obj_set_style_text_font(enter_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(enter_label, lv_color_white(), 0);
    lv_obj_center(enter_label);
    lv_obj_add_event_cb(password_enter_btn_obj, password_enter_event_cb, LV_EVENT_CLICKED, NULL);
    password_enter_btn = password_enter_btn_obj;
}

// ========== UPDATE PASSWORD THEME ==========
static void update_password_theme_colors(void)
{
    if (password_container == NULL) return;

    lv_obj_set_style_bg_color(password_container, get_bg_main_color(), 0);

    if (password_home_btn) {
        lv_obj_set_style_bg_color(password_home_btn, get_bg_panel_color(), 0);
        lv_obj_set_style_border_color(password_home_btn, get_text_secondary_color(), 0);
        lv_obj_t *icon = lv_obj_get_child(password_home_btn, 0);
        if (icon) lv_obj_set_style_text_color(icon, get_text_secondary_color(), 0);
    }
    if (password_title)
        lv_obj_set_style_text_color(password_title, get_accent_color(), 0);
    if (password_subtitle)
        lv_obj_set_style_text_color(password_subtitle, get_text_primary_color(), 0);
    if (password_description)
        lv_obj_set_style_text_color(password_description, get_text_secondary_color(), 0);

    update_password_dots();

    for (int i = 0; i < 12; i++) {
        if (!password_numpad_buttons[i]) continue;
        lv_obj_set_style_bg_color(password_numpad_buttons[i], get_numpad_button_color(), 0);
        lv_obj_t *lbl = lv_obj_get_child(password_numpad_buttons[i], 0);
        if (lbl) {
            if (i == 9) lv_obj_set_style_text_color(lbl, lv_color_make(239, 68, 68), 0);
            else if (i == 11) lv_obj_set_style_text_color(lbl, get_accent_color(), 0);
            else lv_obj_set_style_text_color(lbl, get_text_primary_color(), 0);
        }
    }
    if (password_enter_btn)
        lv_obj_set_style_bg_color(password_enter_btn, get_accent_color(), 0);
}

// ========== HIDE PASSWORD INTERFACE ==========
static void hide_password_interface(void)
{
    if (password_container) {
        lv_obj_del(password_container);
        password_container    = NULL;
        password_title        = NULL;
        password_subtitle     = NULL;
        password_description  = NULL;
        password_status_label = NULL;
        password_enter_btn    = NULL;
        password_home_btn     = NULL;
        for (int i = 0; i < 6;  i++) password_dots[i] = NULL;
        for (int i = 0; i < 12; i++) password_numpad_buttons[i] = NULL;
    }
}

// ========== PUBLIC PASSWORD SHOW ==========
void usrGraphicalInterface_showPasswordSetup(void) {
    hide_password_interface();
    create_password_interface(PASSWORD_MODE_SETUP);
    gui.current_page = PAGE_PASSWORD_SETUP;
}

void usrGraphicalInterface_showPasswordEntry(void) {
    hide_password_interface();
    create_password_interface(PASSWORD_MODE_ENTRY);
    gui.current_page = PAGE_PASSWORD_ENTRY;
}

void usrGraphicalInterface_showPasswordChange(void) {
    hide_password_interface();
    password_change_step = PASSWORD_CHANGE_STEP_OLD;
    memset(old_password_temp, 0, sizeof(old_password_temp));
    create_password_interface(PASSWORD_MODE_CHANGE);
    gui.current_page = PAGE_PASSWORD_CHANGE;
}

// ========== MAIN MENU THEME UPDATE ==========
static void update_theme_colors(void)
{
    if (gui.main_container == NULL || gui.current_page != PAGE_MAIN_MENU) return;

    lv_obj_set_style_bg_color(gui.main_container, get_bg_main_color(), 0);

    if (header_container) {
        lv_obj_set_style_bg_color(header_container, get_bg_panel_color(), 0);
        if (logo_label)
            lv_obj_set_style_text_color(logo_label, get_accent_color(), 0);
        if (brand_label)
            lv_obj_set_style_text_color(brand_label, get_text_primary_color(), 0);
        if (settings_button) {
            lv_obj_set_style_bg_color(settings_button, get_card_bg_color(), 0);
            lv_obj_t *icon = lv_obj_get_child(settings_button, 0);
            if (icon) lv_obj_set_style_text_color(icon, get_text_primary_color(), 0);
        }
    }

    if (content_container) {
        if (main_title_label)
            lv_obj_set_style_text_color(main_title_label, get_text_primary_color(), 0);
        if (main_subtitle_label)
            lv_obj_set_style_text_color(main_subtitle_label, get_text_secondary_color(), 0);

        for (int i = 0; i < 7; i++) {
            if (!room_menu_buttons[i]) continue;
            lv_obj_set_style_bg_color(room_menu_buttons[i], get_card_bg_color(), 0);
            lv_obj_set_style_border_color(room_menu_buttons[i], get_accent_color(), 0);
            if (room_menu_icons[i])
                lv_obj_set_style_text_color(room_menu_icons[i], get_accent_color(), 0);
            if (room_menu_labels[i])
                lv_obj_set_style_text_color(room_menu_labels[i], get_text_primary_color(), 0);
        }
    }
}

// ========== ROOM BUTTON EVENT ==========
static void room_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);
    for (int i = 0; i < 7; i++) {
        if (room_menu_buttons[i] == btn) {
            usrGraphicalInterface_setSelectedRoom(i + 1);
            usrGraphicalInterface_showPage(PAGE_LIGHTING_CONTROL);
            break;
        }
    }
}

// ========== DARK MODE TOGGLE ==========
static void dark_mode_toggle_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
        usrGraphicalInterface_setDarkMode(!is_dark_mode);
}

// ========== SETTINGS BUTTON ==========
static void settings_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (!gui.password_verified) {
        usrGraphicalInterface_showPasswordEntry();
    } else {
        usrGraphicalInterface_showPage(PAGE_SETTINGS);
    }
}

// ========== BACK BUTTON ==========
static void back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (gui.current_page == PAGE_SETTINGS)
        gui.password_verified = false;

    usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
}

// ========== CREATE MAIN MENU ==========
static void create_main_menu(void)
{
    // Header
    header_container = lv_obj_create(gui.content_area);
    lv_obj_set_size(header_container, LV_HOR_RES, 70);
    lv_obj_set_pos(header_container, 0, 0);
    lv_obj_set_style_bg_color(header_container, get_bg_panel_color(), 0);
    lv_obj_set_style_bg_opa(header_container, LV_OPA_90, 0);
    lv_obj_set_style_border_width(header_container, 1, 0);
    lv_obj_set_style_border_side(header_container, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header_container, get_text_secondary_color(), 0);
    lv_obj_set_style_border_opa(header_container, LV_OPA_20, 0);
    lv_obj_set_style_radius(header_container, 0, 0);
    lv_obj_clear_flag(header_container, LV_OBJ_FLAG_SCROLLABLE);

    logo_label = lv_label_create(header_container);
    lv_label_set_text(logo_label, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(logo_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(logo_label, get_accent_color(), 0);
    lv_obj_align(logo_label, LV_ALIGN_LEFT_MID, 15, 0);

    brand_label = lv_label_create(header_container);
    lv_label_set_text(brand_label, "CENTRAL MASTER");
    lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brand_label, get_text_primary_color(), 0);
    lv_obj_align(brand_label, LV_ALIGN_LEFT_MID, 50, -5);

    lv_obj_t *subtitle_hdr = lv_label_create(header_container);
    lv_label_set_text(subtitle_hdr, "ROOM CONTROL CENTER");
    lv_obj_set_style_text_font(subtitle_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle_hdr, get_text_secondary_color(), 0);
    lv_obj_align(subtitle_hdr, LV_ALIGN_LEFT_MID, 50, 12);

    // Dark mode toggle
    dark_mode_button = lv_btn_create(header_container);
    lv_obj_set_size(dark_mode_button, 45, 45);
    lv_obj_align(dark_mode_button, LV_ALIGN_RIGHT_MID, -60, 0);
    lv_obj_set_style_bg_color(dark_mode_button, get_card_bg_color(), 0);
    lv_obj_set_style_border_width(dark_mode_button, 1, 0);
    lv_obj_set_style_radius(dark_mode_button, 10, 0);

    lv_obj_t *dm_icon = lv_label_create(dark_mode_button);
    lv_label_set_text(dm_icon, is_dark_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(dm_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dm_icon, get_text_primary_color(), 0);
    lv_obj_center(dm_icon);
    lv_obj_add_event_cb(dark_mode_button, dark_mode_toggle_event_cb, LV_EVENT_CLICKED, NULL);

    // Settings button
    settings_button = lv_btn_create(header_container);
    lv_obj_set_size(settings_button, 45, 45);
    lv_obj_align(settings_button, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(settings_button, get_card_bg_color(), 0);
    lv_obj_set_style_border_width(settings_button, 1, 0);
    lv_obj_set_style_border_color(settings_button, get_text_secondary_color(), 0);
    lv_obj_set_style_border_opa(settings_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(settings_button, 10, 0);

    lv_obj_t *settings_icon = lv_label_create(settings_button);
    lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settings_icon, get_text_primary_color(), 0);
    lv_obj_center(settings_icon);
    lv_obj_add_event_cb(settings_button, settings_button_event_cb, LV_EVENT_CLICKED, NULL);

    // Content container
    content_container = lv_obj_create(gui.content_area);
    lv_obj_set_size(content_container, LV_HOR_RES, LV_VER_RES - 70);
    lv_obj_set_pos(content_container, 0, 70);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);

    main_title_label = lv_label_create(content_container);
    lv_label_set_text(main_title_label, "Select a Room");
    lv_obj_set_style_text_font(main_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(main_title_label, get_text_primary_color(), 0);
    lv_obj_align(main_title_label, LV_ALIGN_TOP_MID, 0, 20);

    main_subtitle_label = lv_label_create(content_container);
    lv_label_set_text(main_subtitle_label, "Choose a room to manage");
    lv_obj_set_style_text_font(main_subtitle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(main_subtitle_label, get_text_secondary_color(), 0);
    lv_obj_align(main_subtitle_label, LV_ALIGN_TOP_MID, 0, 52);

    // 7 oda butonu (3 + 4 layout)
    int btn_w = 220, btn_h = 100, gap = 15;

    for (int i = 0; i < 7; i++) {
        room_menu_buttons[i] = lv_obj_create(content_container);
        lv_obj_set_size(room_menu_buttons[i], btn_w, btn_h);

        int row = (i < 3) ? 0 : 1;
        int col = (i < 3) ? i : (i - 3);
        int row_count = (row == 0) ? 3 : 4;
        int total_w = row_count * btn_w + (row_count - 1) * gap;
        int start_x = (LV_HOR_RES - total_w) / 2;
        int x = start_x + col * (btn_w + gap);
        int y = 80 + row * (btn_h + 20);
        lv_obj_set_pos(room_menu_buttons[i], x, y);

        lv_obj_add_flag(room_menu_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(room_menu_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(room_menu_buttons[i], get_card_bg_color(), 0);
        lv_obj_set_style_radius(room_menu_buttons[i], 15, 0);
        lv_obj_set_style_border_width(room_menu_buttons[i], 2, 0);
        lv_obj_set_style_border_color(room_menu_buttons[i], get_accent_color(), 0);
        lv_obj_set_style_border_opa(room_menu_buttons[i], LV_OPA_30, 0);
        lv_obj_set_style_shadow_width(room_menu_buttons[i], is_dark_mode ? 0 : 8, 0);
        lv_obj_set_style_shadow_color(room_menu_buttons[i], lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(room_menu_buttons[i], LV_OPA_10, 0);

        room_menu_icons[i] = lv_label_create(room_menu_buttons[i]);
        lv_label_set_text(room_menu_icons[i], LV_SYMBOL_HOME);
        lv_obj_set_style_text_font(room_menu_icons[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(room_menu_icons[i], get_accent_color(), 0);
        lv_obj_align(room_menu_icons[i], LV_ALIGN_CENTER, 0, -18);

        room_menu_labels[i] = lv_label_create(room_menu_buttons[i]);
        char room_txt[16];
        snprintf(room_txt, sizeof(room_txt), "Room %d", i + 1);
        lv_label_set_text(room_menu_labels[i], room_txt);
        lv_obj_set_style_text_font(room_menu_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(room_menu_labels[i], get_text_primary_color(), 0);
        lv_obj_align(room_menu_labels[i], LV_ALIGN_CENTER, 0, 18);

        lv_obj_add_event_cb(room_menu_buttons[i], room_button_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

// ========== HIDE / SHOW MAIN MENU ==========
static void hide_main_menu(void) {
    if (header_container)  lv_obj_add_flag(header_container,  LV_OBJ_FLAG_HIDDEN);
    if (content_container) lv_obj_add_flag(content_container, LV_OBJ_FLAG_HIDDEN);
}

static void show_main_menu(void) {
    if (header_container)  lv_obj_clear_flag(header_container,  LV_OBJ_FLAG_HIDDEN);
    if (content_container) lv_obj_clear_flag(content_container, LV_OBJ_FLAG_HIDDEN);
}

// ========== BACK BUTTON CREATE ==========
static void create_back_button(void)
{
    back_button = lv_btn_create(lv_scr_act());
    lv_obj_set_size(back_button, 85, 45);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_obj_clear_flag(back_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(back_button, get_bg_panel_color(), 0);
    lv_obj_set_style_bg_opa(back_button, LV_OPA_60, 0);
    lv_obj_set_style_border_width(back_button, 2, 0);
    lv_obj_set_style_border_color(back_button, get_text_secondary_color(), 0);
    lv_obj_set_style_border_opa(back_button, LV_OPA_40, 0);
    lv_obj_set_style_radius(back_button, 12, 0);

    lv_obj_t *back_lbl = lv_label_create(back_button);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_lbl, get_text_primary_color(), 0);
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_button, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
}

// ========== INIT ==========
void usrGraphicalInterface_init(void)
{
    debug_nvs_contents();

    if (gui.is_initialized) {
        ESP_LOGW(s_tag, "GUI already initialized");
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    memset(&gui, 0, sizeof(usrGraphicalInterface_t));
    gui.password_set      = usrGraphicalInterface_checkPasswordExists();
    gui.password_verified = false;

    gui.main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gui.main_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(gui.main_container, 0, 0);
    lv_obj_set_style_bg_color(gui.main_container, get_bg_main_color(), 0);
    lv_obj_set_style_border_width(gui.main_container, 0, 0);
    lv_obj_set_style_pad_all(gui.main_container, 0, 0);
    lv_obj_set_style_radius(gui.main_container, 0, 0);
    lv_obj_clear_flag(gui.main_container, LV_OBJ_FLAG_SCROLLABLE);

    gui.content_area = lv_obj_create(gui.main_container);
    lv_obj_set_size(gui.content_area, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(gui.content_area, 0, 0);
    lv_obj_set_style_bg_opa(gui.content_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gui.content_area, 0, 0);
    lv_obj_set_style_pad_all(gui.content_area, 0, 0);
    lv_obj_set_style_radius(gui.content_area, 0, 0);
    lv_obj_clear_flag(gui.content_area, LV_OBJ_FLAG_SCROLLABLE);

    create_back_button();

    usrToiletPage_init(lv_scr_act());
    usrLightingPage_init(lv_scr_act());
    usrSocketsPage_init(lv_scr_act());
    usrSettingsPage_init(lv_scr_act());

    gui.is_initialized = true;

    if (!gui.password_set) {
        usrGraphicalInterface_showPasswordSetup();
    } else {
        create_main_menu();
        gui.current_page = PAGE_MAIN_MENU;
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    }

    ESP_LOGI(s_tag, "Central Master GUI initialized");
}

// ========== DESTROY ==========
void usrGraphicalInterface_destroy(void)
{
    if (!gui.is_initialized) return;

    if (gui.time_timer) { lv_timer_del(gui.time_timer); gui.time_timer = NULL; }

    hide_password_interface();
    usrToiletPage_destroy();
    usrLightingPage_destroy();
    usrSocketsPage_destroy();
    usrSettingsPage_destroy();

    if (gui.main_container) lv_obj_del(gui.main_container);
    memset(&gui, 0, sizeof(usrGraphicalInterface_t));
    ESP_LOGI(s_tag, "GUI destroyed");
}

// ========== SHOW PAGE ==========
void usrGraphicalInterface_showPage(page_type_t page)
{
    if (!gui.is_initialized || page >= PAGE_COUNT) {
        ESP_LOGW(s_tag, "Invalid page or GUI not initialized");
        return;
    }

    if (page == PAGE_PASSWORD_SETUP) {
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrGraphicalInterface_showPasswordSetup();
        return;
    }
    if (page == PAGE_PASSWORD_ENTRY) {
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrGraphicalInterface_showPasswordEntry();
        return;
    }
    if (page == PAGE_PASSWORD_CHANGE) {
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrGraphicalInterface_showPasswordChange();
        return;
    }

    hide_password_interface();
    usrToiletPage_hide();
    usrLightingPage_hide();
    usrSocketsPage_hide();
    usrSettingsPage_hide();

    switch (page)
    {
    case PAGE_MAIN_MENU:
        show_main_menu();
        lv_obj_clear_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrGraphicalInterface_updateSystemStatus("Main Menu Active");
        break;

    case PAGE_TOILET_CONTROL:
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrToiletPage_show();
        usrGraphicalInterface_updateSystemStatus("Toilet Control Active");
        break;

    case PAGE_LIGHTING_CONTROL:
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrLightingPage_show();
        usrGraphicalInterface_updateSystemStatus("Lighting Control Active");
        break;

    case PAGE_SOCKETS_CONTROL:
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrSocketsPage_show();
        usrGraphicalInterface_updateSystemStatus("Sockets Control Active");
        break;

    case PAGE_SETTINGS:
        hide_main_menu();
        lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrSettingsPage_show();
        lv_obj_move_foreground(back_button);
        usrGraphicalInterface_updateSystemStatus("Settings Active");
        break;

    default:
        ESP_LOGW(s_tag, "Unknown page: %d", page);
        return;
    }

    gui.current_page = page;
    ESP_LOGI(s_tag, "Page: %s", page_names[page]);
}

// ========== MISC PUBLIC FUNCTIONS ==========
void usrGraphicalInterface_updateSystemStatus(const char *status) {
    if (gui.system_status_label && status)
        lv_label_set_text(gui.system_status_label, status);
}

bool usrGraphicalInterface_isInitialized(void) {
    return gui.is_initialized;
}

void buttonMenu(void) {
    usrGraphicalInterface_init();
}

void usrGraphicalInterface_hideBackButton(void) {
    if (back_button) lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
}

void usrGraphicalInterface_showBackButton(void) {
    if (back_button) lv_obj_clear_flag(back_button, LV_OBJ_FLAG_HIDDEN);
}