#include "usrGraphicalInterface.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include <time.h>
#include <string.h>
#include "usrSettingsPage.h"



typedef enum {
    PASSWORD_MODE_SETUP,
    PASSWORD_MODE_ENTRY,
    PASSWORD_MODE_CHANGE
} password_interface_mode_t;


static void create_main_menu(void); 
static void create_password_interface(password_interface_mode_t mode);
static void password_confirm_event_cb(lv_event_t *e);
static void password_numpad_event_cb(lv_event_t *e);
static void hide_main_menu(void);
static void show_main_menu(void); 

static void update_theme_colors(void);
static void update_password_theme_colors(void);

static const char *s_tag = "usrGraphicalInterface";
static usrGraphicalInterface_t gui = {0};

void debug_nvs_contents(void);

// Color Palette - DAY MODE (Light Mode)
#define COLOR_BG_MAIN_LIGHT lv_color_make(240, 245, 250)          
#define COLOR_BG_PANEL_LIGHT lv_color_make(255, 255, 255)        
#define COLOR_TEXT_PRIMARY_LIGHT lv_color_make(44, 62, 80)        
#define COLOR_TEXT_SECONDARY_LIGHT lv_color_make(149, 165, 166)   
#define COLOR_CARD_BG_LIGHT lv_color_make(255, 255, 255)       
#define COLOR_SIDEBAR_BG_LIGHT lv_color_make(236, 240, 241)       
#define COLOR_ACCENT_BLUE_DAY lv_color_make(52, 152, 219)       
#define COLOR_ICON_BLUE lv_color_make(52, 152, 219)           
#define COLOR_ICON_ORANGE lv_color_make(251, 146, 60)
#define COLOR_ICON_GREEN lv_color_make(34, 197, 94)

// Color Palette - NIGHT MODE (Dark Mode)
#define COLOR_BG_MAIN_DARK lv_color_make(8, 15, 22)              
#define COLOR_BG_PANEL_DARK lv_color_make(12, 20, 28)             
#define COLOR_TEXT_PRIMARY_DARK lv_color_make(255, 255, 255)     
#define COLOR_TEXT_SECONDARY_DARK lv_color_make(138, 155, 179)  
#define COLOR_CARD_BG_DARK lv_color_make(15, 25, 35)          
#define COLOR_SIDEBAR_BG_DARK lv_color_make(10, 18, 26)     
#define COLOR_NEON_CYAN lv_color_make(0, 210, 255)             
#define COLOR_NEON_BLUE lv_color_make(0, 163, 224)          

// Page names for navigation - Updated with Password pages
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

// Main menu button info - Updated for new layout
typedef struct {
    const char *text;
    page_type_t target_page;
    const char *icon;
    const char *description;
} main_button_info_t;

// Updated buttons array - with icons and descriptions
static const main_button_info_t main_buttons[3] = {
    {"Toilet", PAGE_TOILET_CONTROL, LV_SYMBOL_TINT, ""},
    {"Lighting", PAGE_LIGHTING_CONTROL, LV_SYMBOL_CHARGE, ""},    
    {"Sockets", PAGE_SOCKETS_CONTROL, LV_SYMBOL_USB, ""}
};

// Button icon colors
static lv_color_t button_icon_colors[3];

// Password change state
static enum {
    PASSWORD_CHANGE_STEP_OLD,      // Eski şifreyi gir
    PASSWORD_CHANGE_STEP_NEW       // Yeni şifreyi gir
} password_change_step = PASSWORD_CHANGE_STEP_OLD;

static char old_password_temp[7] = {0};

// Main menu button objects
static lv_obj_t *main_menu_buttons[3] = {NULL};
static lv_obj_t *main_menu_labels[3] = {NULL};
static lv_obj_t *main_menu_icons[3] = {NULL};
static lv_obj_t *main_menu_descriptions[3] = {NULL};
static lv_obj_t *settings_button = NULL;
static lv_obj_t *dark_mode_button = NULL;
static lv_obj_t *main_title_label = NULL;
static lv_obj_t *main_subtitle_label = NULL;
static lv_obj_t *logo_label = NULL;
static lv_obj_t *brand_label = NULL;
static lv_obj_t *back_button = NULL;
static lv_obj_t *header_container = NULL;
static lv_obj_t *content_container = NULL;

// Password UI objects
static lv_obj_t *password_container = NULL;
static lv_obj_t *password_title = NULL;
static lv_obj_t *password_subtitle = NULL;
static lv_obj_t *password_description = NULL;
static lv_obj_t *password_dots[6] = {NULL};
static lv_obj_t *password_numpad_buttons[12] = {NULL};
static lv_obj_t *password_enter_btn = NULL;
static lv_obj_t *password_status_label = NULL;
static lv_obj_t *password_back_btn = NULL;
static lv_obj_t *password_home_btn = NULL;
static char current_password[7] = {0};
static int password_length = 0;
static bool is_password_setup_mode = false;

// Dark mode state
static bool is_dark_mode = true; // Default to dark mode

// Initialize button icon colors (called at runtime)
static void init_button_colors(void)
{
    button_icon_colors[0] = COLOR_ICON_BLUE;   
    button_icon_colors[1] = COLOR_ICON_BLUE; 
    button_icon_colors[2] = COLOR_ICON_BLUE;  
}

// Get current theme colors
static lv_color_t get_bg_main_color(void) {
    return is_dark_mode ? COLOR_BG_MAIN_DARK : COLOR_BG_MAIN_LIGHT;
}

static lv_color_t get_bg_panel_color(void) {
    return is_dark_mode ? COLOR_BG_PANEL_DARK :   COLOR_BG_PANEL_LIGHT;
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

bool usrGraphicalInterface_isDarkMode(void)
{
    return is_dark_mode;
}

void usrGraphicalInterface_setDarkMode(bool dark_mode)
{
    if (is_dark_mode == dark_mode) {
        return;
    }
    
    is_dark_mode = dark_mode;
    ESP_LOGI(s_tag, "🌓 Global theme changed to:  %s MODE", dark_mode ? "DARK" : "LIGHT");
    
    usrToiletPage_updateThemeColors();
    usrLightingPage_updateThemeColors();
    usrSocketsPage_updateThemeColors();
    usrSettingsPage_updateThemeColors();
    
    update_theme_colors();
    update_password_theme_colors();
}


// Password management functions
bool usrGraphicalInterface_checkPasswordExists(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    nvs_close(nvs_handle);
    
    bool exists = (err == ESP_OK && required_size > 1);
    ESP_LOGI(s_tag, "Password check - exists: %s, size: %d, error: %s", 
             exists ? "YES" : "NO", required_size, esp_err_to_name(err));
    return exists;
}

bool usrGraphicalInterface_savePassword(const char *password)
{
    if (!password || strlen(password) == 0) {
        ESP_LOGE(s_tag, "Invalid password");
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle for save: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_handle, NVS_PASSWORD_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error saving password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing to NVS: %s", esp_err_to_name(err));
        return false;
    }

    strncpy(gui.stored_password, password, MAX_PASSWORD_LENGTH);
    gui.stored_password[MAX_PASSWORD_LENGTH] = '\0';
    gui.password_set = true;

    ESP_LOGI(s_tag, "Password saved successfully:  '%s'", password);
    return true;
}

void usrGraphicalInterface_resetPasswordVerification(void)
{
    gui.password_verified = false;
    ESP_LOGI(s_tag, "Password verification reset");
}

void debug_nvs_contents(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS for debug: %s", esp_err_to_name(err));
        return;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    if (err == ESP_OK) {
        char *buffer = malloc(required_size);
        if (buffer) {
            nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, buffer, &required_size);
            ESP_LOGI(s_tag, "NVS Debug - Password key exists, size: %d, value: '%s'", required_size, buffer);
            free(buffer);
        }
    } else {
        ESP_LOGI(s_tag, "NVS Debug - Password key not found:  %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

bool usrGraphicalInterface_verifyPassword(const char *password)
{
    if (!password) {
        ESP_LOGE(s_tag, "Password is NULL");
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle for verification: %s", esp_err_to_name(err));
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, NULL, &required_size);
    if (err != ESP_OK || required_size == 0) {
        nvs_close(nvs_handle);
        ESP_LOGW(s_tag, "Password not found in NVS or empty:  %s", esp_err_to_name(err));
        return false;
    }

    char stored_password[MAX_PASSWORD_LENGTH + 1] = {0};
    if (required_size > sizeof(stored_password)) {
        nvs_close(nvs_handle);
        ESP_LOGE(s_tag, "Stored password size too large: %d", required_size);
        return false;
    }

    err = nvs_get_str(nvs_handle, NVS_PASSWORD_KEY, stored_password, &required_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error reading password from NVS: %s", esp_err_to_name(err));
        return false;
    }

    bool is_correct = (strcmp(password, stored_password) == 0);
    ESP_LOGI(s_tag, "Password verification:  %s", is_correct ? "SUCCESS" : "FAILED");
    
    if (is_correct) {
        strncpy(gui.stored_password, stored_password, MAX_PASSWORD_LENGTH);
        gui.stored_password[MAX_PASSWORD_LENGTH] = '\0';
        gui.password_set = true;
    }
    
    return is_correct;
}

static void password_setup_success_timer_cb(lv_timer_t *timer)
{
    if (main_title_label == NULL) {
        create_main_menu();
    }
    usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    lv_timer_del(timer);
}

static void password_entry_success_timer_cb(lv_timer_t *timer)
{
    usrGraphicalInterface_showPage(PAGE_SETTINGS);
    lv_timer_del(timer);
}

static void update_password_dots(void)
{
    for (int i = 0; i < 6; i++) {
        if (password_dots[i] != NULL) {
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
}

static void password_numpad_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_obj_t *btn = lv_event_get_target(e);
        const char *btn_text = lv_label_get_text(lv_obj_get_child(btn, 0));
        
        // Backspace
        if (strcmp(btn_text, LV_SYMBOL_BACKSPACE) == 0) {
            if (password_length > 0) {
                password_length--;
                current_password[password_length] = '\0';
                update_password_dots();
            }
        }
        // Clear (X)
        else if (strcmp(btn_text, LV_SYMBOL_CLOSE) == 0) {
            password_length = 0;
            memset(current_password, 0, sizeof(current_password));
            update_password_dots();
            if (password_status_label) {
                lv_label_set_text(password_status_label, "");
            }
        }
        // Number
        else if (password_length < 6) {
            current_password[password_length] = btn_text[0];
            password_length++;
            current_password[password_length] = '\0';
            update_password_dots();
        }
    }
}

static void password_enter_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (password_length < 6) {
            lv_label_set_text(password_status_label, "The password must be at least 6 characters long.");
            lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
            return;
        }

        if (is_password_setup_mode) {
            // Password Setup Mode
            if (usrGraphicalInterface_savePassword(current_password)) {
                lv_label_set_text(password_status_label, "Password saved successfully!");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
                
                lv_timer_t *timer = lv_timer_create(password_setup_success_timer_cb, 1500, NULL);
                lv_timer_set_repeat_count(timer, 1);
            } else {
                lv_label_set_text(password_status_label, "Password could not be saved");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
            }
        } else if (gui. current_page == PAGE_PASSWORD_CHANGE) {
            // Password Change Mode
            if (password_change_step == PASSWORD_CHANGE_STEP_OLD) {
                // Eski şifreyi doğrula
                if (usrGraphicalInterface_verifyPassword(current_password)) {
                    // Eski şifre doğru - yeni şifre adımına geç
                    strncpy(old_password_temp, current_password, 6);
                    old_password_temp[6] = '\0';
                    password_change_step = PASSWORD_CHANGE_STEP_NEW;
                    
                    // UI'ı güncelle
                    lv_label_set_text(password_title, "NEW PASSWORD");
                    lv_label_set_text(password_subtitle, "Set Your\nNew PIN");
                    lv_label_set_text(password_description, "Please enter\nyour new 6-digit\nsecurity PIN.");
                    lv_label_set_text(password_status_label, "");
                    
                    // Girişi sıfırla
                    password_length = 0;
                    memset(current_password, 0, sizeof(current_password));
                    update_password_dots();
                } else {
                    lv_label_set_text(password_status_label, "Current password is incorrect!");
                    lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
                    password_length = 0;
                    memset(current_password, 0, sizeof(current_password));
                    update_password_dots();
                }
            } else if (password_change_step == PASSWORD_CHANGE_STEP_NEW) {
                // Yeni şifreyi kaydet
                if (usrGraphicalInterface_savePassword(current_password)) {
                    lv_label_set_text(password_status_label, "Password changed successfully!");
                    lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
                    
                    // Settings sayfasına dön
                    lv_timer_t *timer = lv_timer_create(password_entry_success_timer_cb, 1500, NULL);
                    lv_timer_set_repeat_count(timer, 1);
                    
                    // Değişkenleri sıfırla
                    password_change_step = PASSWORD_CHANGE_STEP_OLD;
                    memset(old_password_temp, 0, sizeof(old_password_temp));
                } else {
                    lv_label_set_text(password_status_label, "Failed to save new password");
                    lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
                }
            }
        } else {
            // Password Entry Mode (Settings için)
            if (usrGraphicalInterface_verifyPassword(current_password)) {
                gui.password_verified = true;
                lv_label_set_text(password_status_label, "Access granted!");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(100, 255, 100), 0);
                
                lv_timer_t *timer = lv_timer_create(password_entry_success_timer_cb, 1000, NULL);
                lv_timer_set_repeat_count(timer, 1);
            } else {
                lv_label_set_text(password_status_label, "Incorrect password");
                lv_obj_set_style_text_color(password_status_label, lv_color_make(255, 100, 100), 0);
                password_length = 0;
                memset(current_password, 0, sizeof(current_password));
                update_password_dots();
            }
        }
    }
}

static void password_back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (is_password_setup_mode) {
            ESP_LOGI(s_tag, "Back from password setup - exiting application");
        } else {
            ESP_LOGI(s_tag, "Back from password entry to main menu");
            usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
        }
    }
}

static void password_home_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (gui.current_page == PAGE_PASSWORD_CHANGE) {
            // Şifre değiştirme sayfasından geri dönüyorsa Settings'e git
            password_change_step = PASSWORD_CHANGE_STEP_OLD;
            memset(old_password_temp, 0, sizeof(old_password_temp));
            usrGraphicalInterface_showPage(PAGE_SETTINGS);
        } else {
            // Normal password entry'den ana menüye git
            usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
        }
    }
}



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

    // Left panel - Text section
    lv_obj_t *left_panel = lv_obj_create(password_container);
    lv_obj_set_size(left_panel, LV_HOR_RES / 2 - 20, LV_VER_RES - 30);
    lv_obj_set_pos(left_panel, 60, 15);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_pad_all(left_panel, 20, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    password_title = lv_label_create(left_panel);
    if (mode == PASSWORD_MODE_SETUP) {
        lv_label_set_text(password_title, "SECURITY PANEL");
    } else if (mode == PASSWORD_MODE_CHANGE) {
        lv_label_set_text(password_title, "CURRENT PASSWORD");
    } else {
        lv_label_set_text(password_title, "SECURITY PANEL");
    }
    lv_obj_set_style_text_font(password_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(password_title, get_accent_color(), 0);
    lv_obj_align(password_title, LV_ALIGN_TOP_LEFT, 0, 40);
 
    // Subtitle
    password_subtitle = lv_label_create(left_panel);
    if (mode == PASSWORD_MODE_SETUP) {
        lv_label_set_text(password_subtitle, "Create \nYour System Lock");
    } else if (mode == PASSWORD_MODE_CHANGE) {
        lv_label_set_text(password_subtitle, "Verify Your\nCurrent PIN");
    } else {
        lv_label_set_text(password_subtitle, "Unlock\nYour System Lock");
    }
    lv_obj_set_style_text_font(password_subtitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(password_subtitle, get_text_primary_color(), 0);
    lv_obj_set_style_text_line_space(password_subtitle, 5, 0);
    lv_obj_align(password_subtitle, LV_ALIGN_TOP_LEFT, 0, 80);

    // Description
    password_description = lv_label_create(left_panel);
    if (mode == PASSWORD_MODE_SETUP) {
        lv_label_set_text(password_description, "Please create \nyour 6-digit security PIN\nto access settings.");
    } else if (mode == PASSWORD_MODE_CHANGE) {
        lv_label_set_text(password_description, "Please enter \nyour current 6-digit PIN\nto change it.");
    } else {
        lv_label_set_text(password_description, "Please enter \nyour 6-digit security PIN\nto access settings.");
    }
    lv_obj_set_style_text_font(password_description, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(password_description, get_text_secondary_color(), 0);
    lv_obj_set_style_text_line_space(password_description, 3, 0);
    lv_obj_set_width(password_description, LV_HOR_RES / 2 - 60);
    lv_label_set_long_mode(password_description, LV_LABEL_LONG_WRAP);
    lv_obj_align(password_description, LV_ALIGN_TOP_LEFT, 0, 170);

    // Status label
    password_status_label = lv_label_create(left_panel);
    lv_label_set_text(password_status_label, "");
    lv_obj_set_style_text_font(password_status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(password_status_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Right panel - Numpad section
    lv_obj_t *right_panel = lv_obj_create(password_container);
    lv_obj_set_size(right_panel, LV_HOR_RES / 2 - 20, LV_VER_RES - 30);
    lv_obj_set_pos(right_panel, LV_HOR_RES / 2 + 5, 15);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 15, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Password dots container
    lv_obj_t *dots_container = lv_obj_create(right_panel);
    lv_obj_set_size(dots_container, 320, 40);
    lv_obj_align(dots_container, LV_ALIGN_TOP_MID, -20, 30);
    lv_obj_set_style_bg_opa(dots_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots_container, 0, 0);
    lv_obj_set_style_pad_all(dots_container, 0, 0);
    lv_obj_set_flex_flow(dots_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dots_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create 6 password dots
    for (int i = 0; i < 6; i++) {
        password_dots[i] = lv_obj_create(dots_container);
        lv_obj_set_size(password_dots[i], 16, 16);
        lv_obj_set_style_radius(password_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(password_dots[i], get_bg_main_color(), 0);
        lv_obj_set_style_bg_opa(password_dots[i], LV_OPA_100, 0);
        lv_obj_set_style_border_width(password_dots[i], 2, 0);
        lv_obj_set_style_border_color(password_dots[i], get_text_secondary_color(), 0);
        lv_obj_set_style_border_opa(password_dots[i], LV_OPA_50, 0);
        
        if (i < 5) {
            lv_obj_set_style_pad_right(password_dots[i], 15, 0);
        }
    }

    // Numpad container
    lv_obj_t *numpad_container = lv_obj_create(right_panel);
    lv_obj_set_size(numpad_container, 280, 320);
    lv_obj_align(numpad_container, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_opa(numpad_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(numpad_container, 0, 0);
    lv_obj_set_style_pad_all(numpad_container, 0, 0);
    lv_obj_clear_flag(numpad_container, LV_OBJ_FLAG_SCROLLABLE);

    // Numpad layout:  1 2 3 / 4 5 6 / 7 8 9 / X 0 <-
    const char *numpad_map[] = {
        "1", "2", "3",
        "4", "5", "6", 
        "7", "8", "9",
        LV_SYMBOL_CLOSE, "0", LV_SYMBOL_BACKSPACE
    };

    int btn_size = 68;
    int spacing = 15;
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            int idx = row * 3 + col;
            
            password_numpad_buttons[idx] = lv_btn_create(numpad_container);
            lv_obj_set_size(password_numpad_buttons[idx], btn_size, btn_size);
            lv_obj_set_pos(password_numpad_buttons[idx], 
                          col * (btn_size + spacing), 
                          row * (btn_size + spacing));
            
            lv_obj_set_style_bg_color(password_numpad_buttons[idx], get_numpad_button_color(), 0);
            lv_obj_set_style_bg_opa(password_numpad_buttons[idx], LV_OPA_100, 0);
            lv_obj_set_style_border_width(password_numpad_buttons[idx], 0, 0);
            lv_obj_set_style_radius(password_numpad_buttons[idx], 15, 0);
            lv_obj_set_style_shadow_width(password_numpad_buttons[idx], is_dark_mode ? 0 : 5, 0);
            lv_obj_set_style_shadow_color(password_numpad_buttons[idx], lv_color_black(), 0);
            lv_obj_set_style_shadow_opa(password_numpad_buttons[idx], LV_OPA_10, 0);
            
            lv_obj_t *btn_label = lv_label_create(password_numpad_buttons[idx]);
            lv_label_set_text(btn_label, numpad_map[idx]);
            lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
            
            // Special colors for X and backspace
            if (idx == 9) { // X button
                lv_obj_set_style_text_color(btn_label, lv_color_make(239, 68, 68), 0);
            } else if (idx == 11) { // Backspace
                lv_obj_set_style_text_color(btn_label, get_accent_color(), 0);
            } else {
                lv_obj_set_style_text_color(btn_label, get_text_primary_color(), 0);
            }
            
            lv_obj_center(btn_label);
            
            lv_obj_add_event_cb(password_numpad_buttons[idx], password_numpad_event_cb, LV_EVENT_CLICKED, NULL);
        }
    }

    // Home button (top right) - Setup modunda gösterme
    if(mode != PASSWORD_MODE_SETUP){
        password_home_btn = lv_btn_create(password_container);
        lv_obj_set_size(password_home_btn, 50, 50);
        lv_obj_align(password_home_btn, LV_ALIGN_TOP_RIGHT, -60, 20);
        lv_obj_set_style_bg_color(password_home_btn, get_bg_panel_color(), 0);
        lv_obj_set_style_bg_opa(password_home_btn, LV_OPA_50, 0);
        lv_obj_set_style_border_width(password_home_btn, 1, 0); 
        lv_obj_set_style_border_color(password_home_btn, get_text_secondary_color(), 0);
        lv_obj_set_style_border_opa(password_home_btn, LV_OPA_30, 0);
        lv_obj_set_style_radius(password_home_btn, 8, 0); 
        lv_obj_set_style_shadow_width(password_home_btn, 0, 0); 
        
        lv_obj_t *home_icon = lv_label_create(password_home_btn);
        lv_label_set_text(home_icon, LV_SYMBOL_HOME);
        lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(home_icon, get_accent_color(), 0);
        lv_obj_center(home_icon);
        
        lv_obj_add_event_cb(password_home_btn, password_home_button_event_cb, LV_EVENT_CLICKED, NULL);
    }else{
        password_home_btn = NULL;
    }
    
    // Enter button
    password_enter_btn = lv_btn_create(left_panel);
    lv_obj_set_size(password_enter_btn, 280, 55);
    lv_obj_align(password_enter_btn, LV_ALIGN_BOTTOM_MID, -30, -20);
    lv_obj_set_style_bg_color(password_enter_btn, get_accent_color(), 0);
    lv_obj_set_style_bg_opa(password_enter_btn, LV_OPA_100, 0);
    lv_obj_set_style_border_width(password_enter_btn, 0, 0);
    lv_obj_set_style_radius(password_enter_btn, 12, 0);
    lv_obj_set_style_shadow_width(password_enter_btn, is_dark_mode ? 0 : 8, 0);
    lv_obj_set_style_shadow_color(password_enter_btn, get_accent_color(), 0);
    lv_obj_set_style_shadow_opa(password_enter_btn, LV_OPA_30, 0);

    lv_obj_t *enter_label = lv_label_create(password_enter_btn);
    lv_label_set_text(enter_label, "ENTER");
    lv_obj_set_style_text_font(enter_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(enter_label, lv_color_white(), 0);
    lv_obj_center(enter_label);

    lv_obj_add_event_cb(password_enter_btn, password_enter_event_cb, LV_EVENT_CLICKED, NULL);

    ESP_LOGI(s_tag, "Password interface created (mode: %d)", mode);
}


static void update_password_theme_colors(void)
{
    if (password_container == NULL) {
        return;
    }

    // Main container
    lv_obj_set_style_bg_color(password_container, get_bg_main_color(), 0);

    // Home button
    if (password_home_btn) {
        bool is_dark = usrGraphicalInterface_isDarkMode();
        
        // Lighting sayfası mantığına göre renk atamaları
        lv_obj_set_style_bg_color(password_home_btn, get_bg_panel_color(), 0);
        lv_obj_set_style_border_color(password_home_btn, get_text_secondary_color(), 0);
        
        lv_obj_t *home_icon = lv_obj_get_child(password_home_btn, 0);
        if (home_icon) {
            // İkon rengi Lighting sayfasındaki gibi ikincil metin renginde olur
            lv_obj_set_style_text_color(home_icon, get_text_secondary_color(), 0);
        }
    }

    // Title
    if (password_title) {
        lv_obj_set_style_text_color(password_title, get_accent_color(), 0);
    }

    // Subtitle
    if (password_subtitle) {
        lv_obj_set_style_text_color(password_subtitle, get_text_primary_color(), 0);
    }

    // Description
    if (password_description) {
        lv_obj_set_style_text_color(password_description, get_text_secondary_color(), 0);
    }

    // Password dots
    update_password_dots();

    // Numpad buttons
    for (int i = 0; i < 12; i++) {
        if (password_numpad_buttons[i]) {
            lv_obj_set_style_bg_color(password_numpad_buttons[i], get_numpad_button_color(), 0);
            lv_obj_set_style_shadow_width(password_numpad_buttons[i], is_dark_mode ? 0 : 5, 0);
            
            lv_obj_t *btn_label = lv_obj_get_child(password_numpad_buttons[i], 0);
            if (btn_label) {
                if (i == 9) { // X button
                    lv_obj_set_style_text_color(btn_label, lv_color_make(239, 68, 68), 0);
                } else if (i == 11) { // Backspace
                    lv_obj_set_style_text_color(btn_label, get_accent_color(), 0);
                } else {
                    lv_obj_set_style_text_color(btn_label, get_text_primary_color(), 0);
                }
            }
        }
    }

    // Enter button
    if (password_enter_btn) {
        lv_obj_set_style_bg_color(password_enter_btn, get_accent_color(), 0);
        lv_obj_set_style_shadow_width(password_enter_btn, is_dark_mode ? 0 : 8, 0);
        lv_obj_set_style_shadow_color(password_enter_btn, get_accent_color(), 0);
    }
}

static void hide_password_interface(void)
{
    if (password_container) {
        lv_obj_del(password_container);
        password_container = NULL;
        password_title = NULL;
        password_subtitle = NULL;
        password_description = NULL;
        password_status_label = NULL;
        password_enter_btn = NULL;
        password_back_btn = NULL;
        password_home_btn = NULL;
        
        for (int i = 0; i < 6; i++) {
            password_dots[i] = NULL;
        }
        for (int i = 0; i < 12; i++) {
            password_numpad_buttons[i] = NULL;
        }
    }
}

void usrGraphicalInterface_showPasswordSetup(void)
{
    hide_password_interface();
    create_password_interface(PASSWORD_MODE_SETUP);
    gui.current_page = PAGE_PASSWORD_SETUP;
}

void usrGraphicalInterface_showPasswordEntry(void)
{
    hide_password_interface();
    create_password_interface(PASSWORD_MODE_ENTRY);
    gui.current_page = PAGE_PASSWORD_ENTRY;
}

void usrGraphicalInterface_showPasswordChange(void)
{
    hide_password_interface();
    password_change_step = PASSWORD_CHANGE_STEP_OLD;
    memset(old_password_temp, 0, sizeof(old_password_temp));
    create_password_interface(PASSWORD_MODE_CHANGE);
    gui.current_page = PAGE_PASSWORD_CHANGE;
}

static void update_theme_colors(void)
{
    if (gui.main_container == NULL || gui.current_page != PAGE_MAIN_MENU) {
        return;
    }
    
    ESP_LOGI(s_tag, "Main menu theme update:  %s MODE", is_dark_mode ? "DARK" :  "LIGHT");
    
    lv_obj_set_style_bg_color(gui.main_container, 
        is_dark_mode ? lv_color_make(8, 15, 22) : lv_color_make(240, 245, 250), 0);
    
    if (header_container != NULL)
    {
        lv_obj_set_style_bg_color(header_container, 
            is_dark_mode ? lv_color_make(12, 20, 28) : lv_color_make(255, 255, 255), 0);
        lv_obj_set_style_bg_opa(header_container, 
            is_dark_mode ?  LV_OPA_90 : LV_OPA_100, 0);
        lv_obj_set_style_border_color(header_container, 
            is_dark_mode ? lv_color_make(148, 163, 184) : lv_color_make(189, 195, 199), 0);
        lv_obj_set_style_border_opa(header_container, LV_OPA_20, 0);
        
        if (logo_label) {
            lv_obj_set_style_text_color(logo_label, 
                is_dark_mode ?  lv_color_make(0, 210, 255) : lv_color_make(52, 152, 219), 0);
        }
        
        if (brand_label) {
            lv_obj_set_style_text_color(brand_label, 
                is_dark_mode ?  lv_color_make(255, 255, 255) : lv_color_make(44, 62, 80), 0);
        }
        
        lv_obj_t *subtitle = lv_obj_get_child(header_container, 2);
        if (subtitle) {
            lv_obj_set_style_text_color(subtitle, 
                is_dark_mode ? lv_color_make(138, 155, 179) : lv_color_make(149, 165, 166), 0);
        }
        
        if (dark_mode_button != NULL)
        {
            lv_obj_set_style_bg_color(dark_mode_button, 
                is_dark_mode ? lv_color_make(15, 25, 35) : lv_color_make(246, 248, 251), 0);
            lv_obj_set_style_bg_opa(dark_mode_button, 
                is_dark_mode ? LV_OPA_60 : LV_OPA_100, 0);
            lv_obj_set_style_border_color(dark_mode_button, 
                is_dark_mode ? lv_color_make(60, 90, 120) : lv_color_make(189, 195, 199), 0);
            
            lv_obj_t *dark_mode_icon = lv_obj_get_child(dark_mode_button, 0);
            if (dark_mode_icon) {
                lv_label_set_text(dark_mode_icon, 
                    is_dark_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
                lv_obj_set_style_text_color(dark_mode_icon, 
                    is_dark_mode ? lv_color_make(255, 255, 255) : lv_color_make(44, 62, 80), 0);
            }
        }
        
        if (settings_button != NULL)
        {
            lv_obj_set_style_bg_color(settings_button, 
                is_dark_mode ? lv_color_make(15, 25, 35) : lv_color_make(255, 255, 255), 0);
            lv_obj_set_style_bg_opa(settings_button, LV_OPA_50, 0);
            lv_obj_set_style_border_color(settings_button, 
                is_dark_mode ?  lv_color_make(138, 155, 179) : lv_color_make(189, 195, 199), 0);
            
            lv_obj_t *settings_icon = lv_obj_get_child(settings_button, 0);
            if (settings_icon) {
                lv_obj_set_style_text_color(settings_icon, 
                    is_dark_mode ? lv_color_make(255, 255, 255) : lv_color_make(44, 62, 80), 0);
            }
        }
    }
    
    if (content_container != NULL)
    {
        if (main_title_label) {
            lv_obj_set_style_text_color(main_title_label, 
                is_dark_mode ? lv_color_make(255, 255, 255) : lv_color_make(44, 62, 80), 0);
        }
        
        if (main_subtitle_label) {
            lv_obj_set_style_text_color(main_subtitle_label, 
                is_dark_mode ? lv_color_make(138, 155, 179) : lv_color_make(149, 165, 166), 0);
        }
        
        for (int i = 0; i < 3; i++)
        {
            if (main_menu_buttons[i] != NULL)
            {
                if (is_dark_mode)
                {
                    lv_obj_set_style_bg_color(main_menu_buttons[i], lv_color_make(15, 25, 35), 0);
                    lv_obj_set_style_bg_opa(main_menu_buttons[i], LV_OPA_50, 0);
                    lv_obj_set_style_border_width(main_menu_buttons[i], 1, 0);
                    lv_obj_set_style_border_color(main_menu_buttons[i], lv_color_make(138, 155, 179), 0);
                    lv_obj_set_style_border_opa(main_menu_buttons[i], LV_OPA_20, 0);
                    lv_obj_set_style_shadow_width(main_menu_buttons[i], 0, 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(main_menu_buttons[i], lv_color_make(255, 255, 255), 0);
                    lv_obj_set_style_bg_opa(main_menu_buttons[i], LV_OPA_100, 0);
                    lv_obj_set_style_border_width(main_menu_buttons[i], 0, 0);
                    lv_obj_set_style_shadow_width(main_menu_buttons[i], 10, 0);
                    lv_obj_set_style_shadow_color(main_menu_buttons[i], lv_color_black(), 0);
                    lv_obj_set_style_shadow_opa(main_menu_buttons[i], LV_OPA_10, 0);
                }
                
                if (main_menu_icons[i]) {
                    lv_obj_set_style_text_color(main_menu_icons[i], 
                        is_dark_mode ? lv_color_make(0, 210, 255) : lv_color_make(52, 152, 219), 0);
                }
                
                if (main_menu_labels[i]) {
                    lv_obj_set_style_text_color(main_menu_labels[i], 
                        is_dark_mode ? lv_color_make(255, 255, 255) : lv_color_make(44, 62, 80), 0);
                }
                
                if (main_menu_descriptions[i]) {
                    lv_obj_set_style_text_color(main_menu_descriptions[i], 
                        is_dark_mode ? lv_color_make(138, 155, 179) : lv_color_make(149, 165, 166), 0);
                }
            }
        }
    }
}

static void dark_mode_toggle_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        usrGraphicalInterface_setDarkMode(!is_dark_mode);
    }
}

static void main_button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        for (int i = 0; i < 3; i++)
        {
            if (main_menu_buttons[i] == btn)
            {
                usrGraphicalInterface_showPage(main_buttons[i]. target_page);
                ESP_LOGI(s_tag, "Navigation to page %d (%s)", main_buttons[i].target_page, page_names[main_buttons[i].target_page]);
                break;
            }
        }
    }
}

static void settings_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (! gui.password_verified) {
            usrGraphicalInterface_showPasswordEntry();
            ESP_LOGI(s_tag, "Password required for Settings access");
        } else {
            usrGraphicalInterface_showPage(PAGE_SETTINGS);
            ESP_LOGI(s_tag, "Navigation to Settings page (password already verified)");
        }
    }
}

static void back_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (gui.current_page == PAGE_SETTINGS) {
            gui.password_verified = false;
        }
        
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
        ESP_LOGI(s_tag, "Back to main menu");
    }
}

static void create_main_menu(void)
{
    init_button_colors();

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
    lv_obj_set_style_pad_left(header_container, 20, 0);
    lv_obj_set_style_pad_right(header_container, 20, 0);
    lv_obj_set_style_pad_top(header_container, 0, 0);
    lv_obj_set_style_pad_bottom(header_container, 0, 0);
    lv_obj_clear_flag(header_container, LV_OBJ_FLAG_SCROLLABLE);

    logo_label = lv_label_create(header_container);
    lv_label_set_text(logo_label, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(logo_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(logo_label, COLOR_ICON_BLUE, 0);
    lv_obj_align(logo_label, LV_ALIGN_LEFT_MID, 0, 0);

    brand_label = lv_label_create(header_container);
    lv_label_set_text(brand_label, "SMART SYSTEM");
    lv_obj_set_style_text_font(brand_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(brand_label, get_text_primary_color(), 0);
    lv_obj_align(brand_label, LV_ALIGN_LEFT_MID, 35, -5);

    lv_obj_t *brand_subtitle = lv_label_create(header_container);
    lv_label_set_text(brand_subtitle, "MAIN CONTROL PANEL");
    lv_obj_set_style_text_font(brand_subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(brand_subtitle, get_text_secondary_color(), 0);
    lv_obj_align(brand_subtitle, LV_ALIGN_LEFT_MID, 35, 12);

    dark_mode_button = lv_btn_create(header_container);
    lv_obj_set_size(dark_mode_button, 45, 45);
    lv_obj_align(dark_mode_button, LV_ALIGN_RIGHT_MID, -55, 0);
    lv_obj_clear_flag(dark_mode_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(dark_mode_button, 
        is_dark_mode ?  lv_color_make(15, 25, 35) : lv_color_make(246, 248, 251), 0);
    lv_obj_set_style_bg_opa(dark_mode_button, is_dark_mode ? LV_OPA_60 : LV_OPA_100, 0);
    lv_obj_set_style_border_width(dark_mode_button, 2, 0);
    lv_obj_set_style_border_color(dark_mode_button, 
        is_dark_mode ?  lv_color_make(60, 90, 120) : lv_color_make(189, 195, 199), 0);
    lv_obj_set_style_border_opa(dark_mode_button, LV_OPA_40, 0);
    lv_obj_set_style_radius(dark_mode_button, 10, 0);

    lv_obj_t *dark_mode_icon = lv_label_create(dark_mode_button);
    lv_label_set_text(dark_mode_icon, is_dark_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(dark_mode_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(dark_mode_icon, get_text_primary_color(), 0);
    lv_obj_center(dark_mode_icon);

    lv_obj_add_event_cb(dark_mode_button, dark_mode_toggle_event_cb, LV_EVENT_CLICKED, NULL);

    settings_button = lv_btn_create(header_container);
    lv_obj_set_size(settings_button, 45, 45);
    lv_obj_align(settings_button, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(settings_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(settings_button, get_card_bg_color(), 0);
    lv_obj_set_style_bg_opa(settings_button, LV_OPA_50, 0);
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

    content_container = lv_obj_create(gui.content_area);
    lv_obj_set_size(content_container, LV_HOR_RES, LV_VER_RES - 70);
    lv_obj_set_pos(content_container, 0, 70);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 15, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);

    main_title_label = lv_label_create(content_container);
    lv_label_set_text(main_title_label, "Welcome Home");
    lv_obj_set_style_text_font(main_title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(main_title_label, get_text_primary_color(), 0);
    lv_obj_align(main_title_label, LV_ALIGN_TOP_MID, 0, 30);

    main_subtitle_label = lv_label_create(content_container);
    lv_label_set_text(main_subtitle_label, is_dark_mode ? "Select a zone to manage" : "Select a control panel to manage your environment");
    lv_obj_set_style_text_font(main_subtitle_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(main_subtitle_label, get_text_secondary_color(), 0);
    lv_obj_align(main_subtitle_label, LV_ALIGN_TOP_MID, 0, 55);

    int available_width = LV_HOR_RES - 40;
    int btn_width = (available_width - 40) / 3;
    int btn_height = 150;
    int spacing = 20;
    int start_y = 120;

    for (int i = 0; i < 3; i++)
    {
        main_menu_buttons[i] = lv_obj_create(content_container);
        lv_obj_set_size(main_menu_buttons[i], btn_width, btn_height);
        
        int x_pos = 15 + (i * (btn_width + spacing));
        lv_obj_set_pos(main_menu_buttons[i], x_pos - 10, start_y);
        
        lv_obj_clear_flag(main_menu_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(main_menu_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        
        if (is_dark_mode) {
            lv_obj_set_style_bg_color(main_menu_buttons[i], lv_color_make(15, 25, 35), 0);
            lv_obj_set_style_bg_opa(main_menu_buttons[i], LV_OPA_50, 0);
        } else {
            lv_obj_set_style_bg_color(main_menu_buttons[i], lv_color_make(255, 255, 255), 0);
            lv_obj_set_style_bg_opa(main_menu_buttons[i], LV_OPA_100, 0);
        }

        lv_obj_set_style_border_width(main_menu_buttons[i], is_dark_mode ? 1 : 0, 0);
        lv_obj_set_style_border_color(main_menu_buttons[i], get_text_secondary_color(), 0);
        lv_obj_set_style_border_opa(main_menu_buttons[i], LV_OPA_20, 0);
        lv_obj_set_style_radius(main_menu_buttons[i], 20, 0);
        lv_obj_set_style_shadow_width(main_menu_buttons[i], is_dark_mode ? 0 : 10, 0);
        lv_obj_set_style_shadow_color(main_menu_buttons[i], lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(main_menu_buttons[i], LV_OPA_10, 0);
        lv_obj_set_style_pad_all(main_menu_buttons[i], 15, 0);
        
        lv_obj_set_style_transform_width(main_menu_buttons[i], -2, LV_STATE_PRESSED);
        lv_obj_set_style_transform_height(main_menu_buttons[i], -2, LV_STATE_PRESSED);

        main_menu_icons[i] = lv_label_create(main_menu_buttons[i]);
        lv_label_set_text(main_menu_icons[i], main_buttons[i].icon);
        lv_obj_set_style_text_font(main_menu_icons[i], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(main_menu_icons[i], button_icon_colors[i], 0);
        lv_obj_align(main_menu_icons[i], LV_ALIGN_CENTER, 0, -30);

        main_menu_labels[i] = lv_label_create(main_menu_buttons[i]);
        lv_label_set_text(main_menu_labels[i], main_buttons[i].text);
        lv_obj_set_style_text_font(main_menu_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(main_menu_labels[i], get_text_primary_color(), 0);
        lv_obj_align(main_menu_labels[i], LV_ALIGN_CENTER, 0, 20);

        main_menu_descriptions[i] = lv_label_create(main_menu_buttons[i]);
        lv_label_set_text(main_menu_descriptions[i], main_buttons[i].description);
        lv_obj_set_style_text_font(main_menu_descriptions[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(main_menu_descriptions[i], get_text_secondary_color(), 0);
        lv_obj_set_style_text_align(main_menu_descriptions[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(main_menu_descriptions[i], btn_width - 30);
        lv_obj_align(main_menu_descriptions[i], LV_ALIGN_BOTTOM_MID, 0, -10);

        lv_obj_add_event_cb(main_menu_buttons[i], main_button_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void hide_main_menu(void)
{
    if (header_container) lv_obj_add_flag(header_container, LV_OBJ_FLAG_HIDDEN);
    if (content_container) lv_obj_add_flag(content_container, LV_OBJ_FLAG_HIDDEN);
}

static void show_main_menu(void)
{
    if (header_container) lv_obj_clear_flag(header_container, LV_OBJ_FLAG_HIDDEN);
    if (content_container) lv_obj_clear_flag(content_container, LV_OBJ_FLAG_HIDDEN);
}

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
    
    lv_obj_t *back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, get_text_primary_color(), 0);
    lv_obj_center(back_label);
    
    lv_obj_add_event_cb(back_button, back_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
}

void usrGraphicalInterface_init(void)
{
    debug_nvs_contents();

    if (gui.is_initialized)
    {
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

    gui.password_set = usrGraphicalInterface_checkPasswordExists();
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

    if (! gui.password_set) {
        ESP_LOGI(s_tag, "No password set, showing password setup");
        usrGraphicalInterface_showPasswordSetup();
    } else {
        ESP_LOGI(s_tag, "Password exists, showing main menu");
        create_main_menu();
        gui.current_page = PAGE_MAIN_MENU;
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    }

    ESP_LOGI(s_tag, "Graphical interface initialized with new design");
}

void usrGraphicalInterface_destroy(void)
{
    if (!gui.is_initialized)
    {
        return;
    }

    if (gui.time_timer != NULL)
    {
        lv_timer_del(gui.time_timer);
        gui.time_timer = NULL;
    }

    hide_password_interface();

    usrToiletPage_destroy();
    usrLightingPage_destroy();
    usrSocketsPage_destroy();
    usrSettingsPage_destroy();

    if (gui.main_container != NULL)
    {
        lv_obj_del(gui.main_container);
    }

    memset(&gui, 0, sizeof(usrGraphicalInterface_t));

    ESP_LOGI(s_tag, "Graphical interface destroyed");
}

void usrGraphicalInterface_showPage(page_type_t page)
{
    if (! gui.is_initialized || page >= PAGE_COUNT)
    {
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
        lv_obj_add_flag(gui. main_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        usrGraphicalInterface_showPasswordEntry();
        return;
    }

    // YENİ EKLENEN
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
        lv_obj_add_flag(gui. main_container, LV_OBJ_FLAG_HIDDEN);
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
        ESP_LOGW(s_tag, "Unknown page type:  %d", page);
        return;
    }

    gui.current_page = page;
    ESP_LOGI(s_tag, "Switched to page: %s", page_names[page]);
}

void usrGraphicalInterface_updateSystemStatus(const char *status)
{
    if (gui.system_status_label != NULL && status != NULL)
    {
        lv_label_set_text(gui.system_status_label, status);
    }
}

bool usrGraphicalInterface_isInitialized(void)
{
    return gui.is_initialized;
}

void buttonMenu(void)
{
    ESP_LOGI(s_tag, "Legacy buttonMenu() called - initializing new GUI system");
    usrGraphicalInterface_init();
}

void usrGraphicalInterface_hideBackButton(void)
{
    if (back_button != NULL) {
        lv_obj_add_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(s_tag, "Back button hidden");
    }
}

void usrGraphicalInterface_showBackButton(void)
{
    if (back_button != NULL) {
        lv_obj_clear_flag(back_button, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(s_tag, "Back button shown");
    }
}