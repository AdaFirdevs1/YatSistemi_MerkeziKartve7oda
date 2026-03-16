#include "usrSettingsPage.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <time.h>
#include "usrGraphicalInterface.h"

static const char *s_tag = "usrSettingsPage";
static usrSettingsPage_t settingsPage = {0};

// NVS storage keys 
#undef NVS_NAMESPACE
#define NVS_NAMESPACE "button_names"
#define NVS_HISTORY_NAMESPACE "button_history"
#define MAX_BUTTON_NAME_LENGTH 16
#define MAX_HISTORY_ENTRIES 50
#define MAX_HISTORY_TEXT_LENGTH 64


// Color Palette - NIGHT MODE (from usrLightingPage)
#define COLOR_BG_MAIN_NIGHT lv_color_make(8, 15, 22)
#define COLOR_BG_PANEL_NIGHT lv_color_make(12, 20, 28)
#define COLOR_NEON_CYAN lv_color_make(0, 210, 255)
#define COLOR_NEON_BLUE lv_color_make(0, 163, 224)
#define COLOR_TEXT_PRIMARY_NIGHT lv_color_make(255, 255, 255)
#define COLOR_TEXT_SECONDARY_NIGHT lv_color_make(138, 155, 179)
#define COLOR_SIDEBAR_BG_NIGHT lv_color_make(10, 18, 26)
#define COLOR_CARD_BG_NIGHT lv_color_make(15, 25, 35)

// Color Palette - DAY MODE (from usrLightingPage)
#define COLOR_BG_MAIN_DAY lv_color_make(240, 245, 250)
#define COLOR_BG_PANEL_DAY lv_color_make(255, 255, 255)
#define COLOR_ACCENT_BLUE_DAY lv_color_make(52, 152, 219)
#define COLOR_TEXT_PRIMARY_DAY lv_color_make(44, 62, 80)
#define COLOR_TEXT_SECONDARY_DAY lv_color_make(149, 165, 166)
#define COLOR_INACTIVE_GRAY_DAY lv_color_make(183, 189, 199)
#define COLOR_SIDEBAR_BG_DAY lv_color_make(236, 240, 241)
#define COLOR_CARD_BG_DAY lv_color_make(255, 255, 255)
#define COLOR_BORDER_DAY lv_color_make(189, 195, 199)
#define COLOR_SELECTED_BG_DAY lv_color_make(225, 236, 244)

// History entry structure
typedef struct {
    time_t timestamp;
    int section;
    int button;
    char old_name[MAX_BUTTON_NAME_LENGTH];
    char new_name[MAX_BUTTON_NAME_LENGTH];
} history_entry_t;

// Default button names
static char button_names[3][6][MAX_BUTTON_NAME_LENGTH] = {
    {"PWM-1", "PWM-2", "PWM-3", "PWM-4", "PWM-5", "PWM-6"},
    {"PWM-1", "PWM-2", "PWM-3", "PWM-4", "PWM-5", "PWM-6"},
    {"PWM-1", "PWM-2", "PWM-3", "PWM-4", "PWM-5", "PWM-6"}
};

static const char *section_names[3] = {"Toilet", "Lighting", "Sockets"};


static history_entry_t history_entries[MAX_HISTORY_ENTRIES];
static int history_count = 0;

// Forward declarations
static void section_button_event_cb(lv_event_t *e);
static void save_button_event_cb(lv_event_t *e);
static void cancel_button_event_cb(lv_event_t *e);
static void history_button_event_cb(lv_event_t *e);
static void back_to_main_event_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void textarea_event_cb(lv_event_t *e);
static void day_night_toggle_cb(lv_event_t *e);
static void home_button_event_cb(lv_event_t *e);
static void editor_home_button_event_cb(lv_event_t *e);
static void edit_password_button_event_cb(lv_event_t *e);  
static void create_section_selector(void);
static void create_button_name_editor(void);
static void create_history_page(void);
static void show_editor_for_section(int section);
static void show_history_page(void);
static void hide_editor(void);
static void hide_history_page(void);
static void load_history_from_nvs(void);
static void save_history_to_nvs(void);
static void add_history_entry(int section, int button, const char *old_name, const char *new_name);
static void update_theme_colors(void);

void usrSettingsPage_hide(void);
void usrSettingsPage_setStatus(const char *status);

// Day/Night toggle event callback
static void day_night_toggle_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED)
    {
        void *user_data = lv_event_get_user_data(e);
        bool clicked_day = (bool)(intptr_t)user_data;
        
        bool current_is_dark = usrGraphicalInterface_isDarkMode();
        bool new_is_dark = !  clicked_day; // day=false ise dark=true
        
        if (current_is_dark != new_is_dark)
        {
            // Merkezi fonksiyonu çağır - bu tüm sayfaları güncelleyecek
            usrGraphicalInterface_setDarkMode(new_is_dark);
            
            // Sadece bu sayfadaki buton görünümünü güncelle
            lv_obj_t *toggle_container = lv_obj_get_parent(lv_event_get_target(e));
            lv_obj_t *night_btn = lv_obj_get_child(toggle_container, 0);
            lv_obj_t *day_btn = lv_obj_get_child(toggle_container, 1);
            
            if (!  new_is_dark) // DAY MODE
            {
                // Night butonunu pasif yap
                lv_obj_set_style_bg_color(night_btn, lv_color_make(245, 248, 250), 0);
                lv_obj_set_style_bg_opa(night_btn, LV_OPA_100, 0);
                lv_obj_set_style_border_width(night_btn, 2, 0);
                lv_obj_set_style_border_color(night_btn, lv_color_make(183, 189, 199), 0);
                lv_obj_set_style_border_opa(night_btn, LV_OPA_60, 0);
                lv_obj_set_style_shadow_width(night_btn, 0, 0);
                
                lv_obj_t *night_label = lv_obj_get_child(night_btn, 0);
                lv_obj_set_style_text_color(night_label, lv_color_make(183, 189, 199), 0);
                
                // Day butonunu aktif yap
                lv_obj_set_style_bg_color(day_btn, lv_color_make(225, 242, 252), 0);
                lv_obj_set_style_bg_opa(day_btn, LV_OPA_100, 0);
                lv_obj_set_style_border_width(day_btn, 2, 0);
                lv_obj_set_style_border_color(day_btn, lv_color_make(52, 152, 219), 0);
                lv_obj_set_style_border_opa(day_btn, LV_OPA_100, 0);
                lv_obj_set_style_shadow_width(day_btn, 10, 0);
                lv_obj_set_style_shadow_color(day_btn, lv_color_make(52, 152, 219), 0);
                lv_obj_set_style_shadow_opa(day_btn, LV_OPA_30, 0);
                
                lv_obj_t *day_label = lv_obj_get_child(day_btn, 0);
                lv_obj_set_style_text_color(day_label, lv_color_make(52, 152, 219), 0);
            }
            else // NIGHT MODE
            {
                // Night butonunu aktif yap
                lv_obj_set_style_bg_color(night_btn, lv_color_make(20, 40, 60), 0);
                lv_obj_set_style_bg_opa(night_btn, LV_OPA_70, 0);
                lv_obj_set_style_border_width(night_btn, 3, 0);
                lv_obj_set_style_border_color(night_btn, lv_color_make(0, 210, 255), 0);
                lv_obj_set_style_border_opa(night_btn, LV_OPA_100, 0);
                lv_obj_set_style_shadow_width(night_btn, 15, 0);
                lv_obj_set_style_shadow_color(night_btn, lv_color_make(0, 210, 255), 0);
                lv_obj_set_style_shadow_opa(night_btn, LV_OPA_70, 0);
                
                lv_obj_t *night_label = lv_obj_get_child(night_btn, 0);
                lv_obj_set_style_text_color(night_label, lv_color_make(0, 210, 255), 0);
                
                // Day butonunu pasif yap
                lv_obj_set_style_bg_color(day_btn, lv_color_make(15, 30, 45), 0);
                lv_obj_set_style_bg_opa(day_btn, LV_OPA_60, 0);
                lv_obj_set_style_border_width(day_btn, 2, 0);
                lv_obj_set_style_border_color(day_btn, lv_color_make(60, 90, 120), 0);
                lv_obj_set_style_border_opa(day_btn, LV_OPA_40, 0);
                lv_obj_set_style_shadow_width(day_btn, 0, 0);
                
                lv_obj_t *day_label = lv_obj_get_child(day_btn, 0);
                lv_obj_set_style_text_color(day_label, lv_color_make(138, 155, 179), 0);
            }
        }
    }
}

// Home button event callback
static void home_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Home button clicked - returning to main menu");
        
        // Şifre doğrulamasını sıfırla
        usrGraphicalInterface_resetPasswordVerification();
        
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    }
}

// Editor home button event callback
static void editor_home_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Editor home button clicked - returning to settings main");
        hide_editor();
    }
}

// Update theme colors based on day/night mode
void usrSettingsPage_updateThemeColors(void) 
{
    bool is_day_mode = ! usrGraphicalInterface_isDarkMode();
    if (settingsPage.page_container == NULL) return;
    
    if (is_day_mode)
    {
        // ========== DAY MODE ==========
        
        // Main container
        lv_obj_set_style_bg_color(settingsPage.page_container, COLOR_BG_MAIN_DAY, 0);
        
        // Header
        if (settingsPage.header != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.header, COLOR_BG_PANEL_DAY, 0);
            lv_obj_set_style_border_color(settingsPage.header, COLOR_BORDER_DAY, 0);
            
            // Logo (first child of header)
            lv_obj_t *logo = lv_obj_get_child(settingsPage.header, 0);
            if (logo) lv_obj_set_style_text_color(logo, COLOR_TEXT_PRIMARY_DAY, 0);
        }
        
        // Edit password button
        if (settingsPage.edit_password_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.edit_password_button, 
                is_day_mode ? COLOR_SIDEBAR_BG_DAY : COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.edit_password_button, 
                is_day_mode ? COLOR_BORDER_DAY : COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_t *edit_icon = lv_obj_get_child(settingsPage.edit_password_button, 0);
            if (edit_icon) 
                lv_obj_set_style_text_color(edit_icon, 
                    is_day_mode ?  lv_color_make(38, 134, 173) : COLOR_TEXT_SECONDARY_NIGHT, 0);
        }

        // Home button
        if (settingsPage.home_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.home_button, COLOR_SIDEBAR_BG_DAY, 0);
            lv_obj_set_style_border_color(settingsPage.home_button, COLOR_BORDER_DAY, 0);
            
            lv_obj_t *home_icon = lv_obj_get_child(settingsPage.home_button, 0);
            if (home_icon) lv_obj_set_style_text_color(home_icon, lv_color_make(38, 134, 173), 0);
        }
        
        // Title
        if (settingsPage.title_label)
            lv_obj_set_style_text_color(settingsPage.title_label, COLOR_TEXT_PRIMARY_DAY, 0);
        
        // Section buttons
        for (int i = 0; i < 3; i++)
        {
            if (settingsPage.section_buttons[i] != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage.section_buttons[i], COLOR_CARD_BG_DAY, 0);
                lv_obj_set_style_bg_opa(settingsPage.section_buttons[i], LV_OPA_100, 0);
                lv_obj_set_style_border_color(settingsPage.section_buttons[i], COLOR_BORDER_DAY, 0);
                lv_obj_set_style_border_opa(settingsPage.section_buttons[i], LV_OPA_60, 0);
                lv_obj_set_style_shadow_color(settingsPage.section_buttons[i], lv_color_black(), 0);
                lv_obj_set_style_shadow_opa(settingsPage.section_buttons[i], LV_OPA_20, 0);
                
                // Icon color (first child)
                lv_obj_t *icon = lv_obj_get_child(settingsPage.section_buttons[i], 0);
                if (icon) lv_obj_set_style_text_color(icon, COLOR_ACCENT_BLUE_DAY, 0);
                
                // Title label (second child)
                if (settingsPage.section_labels[i])
                    lv_obj_set_style_text_color(settingsPage.section_labels[i], COLOR_TEXT_PRIMARY_DAY, 0);
                
                // Subtitle (third child)
                lv_obj_t *subtitle = lv_obj_get_child(settingsPage.section_buttons[i], 2);
                if (subtitle) lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_DAY, 0);
            }
        }
        
        // History button
        if (settingsPage.history_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.history_button, COLOR_ACCENT_BLUE_DAY, 0);
            lv_obj_set_style_border_color(settingsPage.history_button, COLOR_ACCENT_BLUE_DAY, 0);
            lv_obj_set_style_shadow_color(settingsPage.history_button, COLOR_ACCENT_BLUE_DAY, 0);
            
            lv_obj_t *history_icon = lv_obj_get_child(settingsPage.history_button, 0);
            if (history_icon) lv_obj_set_style_text_color(history_icon, lv_color_white(), 0);
            
            lv_obj_t *history_label = lv_obj_get_child(settingsPage.history_button, 1);
            if (history_label) lv_obj_set_style_text_color(history_label, lv_color_white(), 0);
        }
        
        // Status label
        if (settingsPage.status_label)
            lv_obj_set_style_text_color(settingsPage.status_label, COLOR_TEXT_SECONDARY_DAY, 0);
    }
    else
    {
        // ========== NIGHT MODE ==========
        
        // Main container
        lv_obj_set_style_bg_color(settingsPage.page_container, COLOR_BG_MAIN_NIGHT, 0);
        
        // Header
        if (settingsPage.header != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.header, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.header, lv_color_make(148, 163, 184), 0);
            
            // Logo
            lv_obj_t *logo = lv_obj_get_child(settingsPage.header, 0);
            if (logo) lv_obj_set_style_text_color(logo, COLOR_TEXT_PRIMARY_NIGHT, 0);
        }
        
        // Edit password button
        if (settingsPage.edit_password_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.edit_password_button, 
                is_day_mode ? COLOR_SIDEBAR_BG_DAY : COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.edit_password_button, 
                is_day_mode ? COLOR_BORDER_DAY : COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_t *edit_icon = lv_obj_get_child(settingsPage.edit_password_button, 0);
            if (edit_icon) 
                lv_obj_set_style_text_color(edit_icon, 
                    is_day_mode ?  lv_color_make(38, 134, 173) : COLOR_TEXT_SECONDARY_NIGHT, 0);
        }

        // Home button
        if (settingsPage.home_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.home_button, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_t *home_icon = lv_obj_get_child(settingsPage.home_button, 0);
            if (home_icon) lv_obj_set_style_text_color(home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
        }
        
        // Title
        if (settingsPage.title_label)
            lv_obj_set_style_text_color(settingsPage.title_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
        
        // Section buttons
        for (int i = 0; i < 3; i++)
        {
            if (settingsPage.section_buttons[i] != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage.section_buttons[i], COLOR_CARD_BG_NIGHT, 0);
                lv_obj_set_style_bg_opa(settingsPage.section_buttons[i], LV_OPA_80, 0);
                lv_obj_set_style_border_color(settingsPage.section_buttons[i], lv_color_make(30, 50, 70), 0);
                lv_obj_set_style_border_opa(settingsPage.section_buttons[i], LV_OPA_60, 0);
                lv_obj_set_style_shadow_color(settingsPage.section_buttons[i], COLOR_NEON_CYAN, 0);
                lv_obj_set_style_shadow_opa(settingsPage.section_buttons[i], LV_OPA_20, 0);
                
                // Icon color
                lv_obj_t *icon = lv_obj_get_child(settingsPage.section_buttons[i], 0);
                if (icon) lv_obj_set_style_text_color(icon, COLOR_NEON_CYAN, 0);
                
                // Title label
                if (settingsPage.section_labels[i])
                    lv_obj_set_style_text_color(settingsPage.section_labels[i], COLOR_TEXT_PRIMARY_NIGHT, 0);
                
                // Subtitle
                lv_obj_t *subtitle = lv_obj_get_child(settingsPage.section_buttons[i], 2);
                if (subtitle) lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
        }
        
        // History button
        if (settingsPage.history_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.history_button, lv_color_make(20, 40, 60), 0);
            lv_obj_set_style_border_color(settingsPage.history_button, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_shadow_color(settingsPage.history_button, COLOR_NEON_CYAN, 0);
            
            lv_obj_t *history_icon = lv_obj_get_child(settingsPage.history_button, 0);
            if (history_icon) lv_obj_set_style_text_color(history_icon, COLOR_NEON_CYAN, 0);
            
            lv_obj_t *history_label = lv_obj_get_child(settingsPage.history_button, 1);
            if (history_label) lv_obj_set_style_text_color(history_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
        }
        
        // Status label
        if (settingsPage.status_label)
            lv_obj_set_style_text_color(settingsPage.status_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
    }

    // ========== EDITOR PAGE THEME UPDATES ==========
    if (settingsPage.editor_container != NULL)
    {
        if (is_day_mode)
        {
            // Editor container
            lv_obj_set_style_bg_color(settingsPage.editor_container, COLOR_BG_MAIN_DAY, 0);
            
            // Editor header
            if (settingsPage.editor_header != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage.editor_header, COLOR_BG_PANEL_DAY, 0);
                lv_obj_set_style_border_color(settingsPage.editor_header, COLOR_BORDER_DAY, 0);
                
                // Logo
                lv_obj_t *editor_logo = lv_obj_get_child(settingsPage.editor_header, 0);
                if (editor_logo) lv_obj_set_style_text_color(editor_logo, COLOR_TEXT_PRIMARY_DAY, 0);
            }
            
            // Editor title
            if (settingsPage.editor_title)
                lv_obj_set_style_text_color(settingsPage.editor_title, COLOR_ACCENT_BLUE_DAY, 0);
            
            // Editor home button
            if (settingsPage.editor_home_button != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage. editor_home_button, COLOR_SIDEBAR_BG_DAY, 0);
                lv_obj_set_style_border_color(settingsPage.editor_home_button, COLOR_BORDER_DAY, 0);
                
                lv_obj_t *home_icon = lv_obj_get_child(settingsPage.editor_home_button, 0);
                if (home_icon) lv_obj_set_style_text_color(home_icon, lv_color_make(38, 134, 173), 0);
            }
            
            // Input labels and text areas
            for (int i = 0; i < 6; i++)
            {
                if (settingsPage.button_name_labels[i])
                    lv_obj_set_style_text_color(settingsPage.button_name_labels[i], COLOR_TEXT_SECONDARY_DAY, 0);
                
                if (settingsPage. button_name_inputs[i])
                {
                    lv_obj_set_style_bg_color(settingsPage.button_name_inputs[i], COLOR_CARD_BG_DAY, 0);
                    lv_obj_set_style_border_color(settingsPage.button_name_inputs[i], COLOR_ACCENT_BLUE_DAY, 0);
                    lv_obj_set_style_text_color(settingsPage.button_name_inputs[i], COLOR_TEXT_PRIMARY_DAY, 0);
                }
            }
            
            // Keyboard
            if (settingsPage.keyboard)
            {
                lv_obj_set_style_bg_color(settingsPage.keyboard, COLOR_SIDEBAR_BG_DAY, 0);
                lv_obj_set_style_border_color(settingsPage.keyboard, COLOR_ACCENT_BLUE_DAY, 0);
            }
            
            // Cancel button
            if (settingsPage.cancel_button)
            {
                lv_obj_set_style_bg_color(settingsPage.cancel_button, COLOR_CARD_BG_DAY, 0);
                lv_obj_set_style_border_color(settingsPage. cancel_button, COLOR_BORDER_DAY, 0);
                
                lv_obj_t *cancel_label = lv_obj_get_child(settingsPage.cancel_button, 0);
                if (cancel_label) lv_obj_set_style_text_color(cancel_label, COLOR_TEXT_PRIMARY_DAY, 0);
            }
            
            // Save button
            if (settingsPage. save_button)
            {
                lv_obj_set_style_bg_color(settingsPage.save_button, COLOR_ACCENT_BLUE_DAY, 0);
                lv_obj_set_style_border_color(settingsPage.save_button, COLOR_ACCENT_BLUE_DAY, 0);
                lv_obj_set_style_shadow_color(settingsPage.save_button, COLOR_ACCENT_BLUE_DAY, 0);
                
                lv_obj_t *save_label = lv_obj_get_child(settingsPage.save_button, 0);
                if (save_label) lv_obj_set_style_text_color(save_label, lv_color_white(), 0);
            }
        }
        else  // NIGHT MODE
        {
            // Editor container
            lv_obj_set_style_bg_color(settingsPage.editor_container, COLOR_BG_MAIN_NIGHT, 0);
            
            // Editor header
            if (settingsPage.editor_header != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage.editor_header, COLOR_BG_PANEL_NIGHT, 0);
                lv_obj_set_style_border_color(settingsPage.editor_header, lv_color_make(148, 163, 184), 0);
                
                lv_obj_t *editor_logo = lv_obj_get_child(settingsPage.editor_header, 0);
                if (editor_logo) lv_obj_set_style_text_color(editor_logo, COLOR_TEXT_PRIMARY_NIGHT, 0);
            }
            
            if (settingsPage.editor_title)
                lv_obj_set_style_text_color(settingsPage.editor_title, COLOR_NEON_CYAN, 0);
            
            if (settingsPage.editor_home_button != NULL)
            {
                lv_obj_set_style_bg_color(settingsPage. editor_home_button, COLOR_BG_PANEL_NIGHT, 0);
                lv_obj_set_style_border_color(settingsPage.editor_home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
                
                lv_obj_t *home_icon = lv_obj_get_child(settingsPage.editor_home_button, 0);
                if (home_icon) lv_obj_set_style_text_color(home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
            
            for (int i = 0; i < 6; i++)
            {
                if (settingsPage.button_name_labels[i])
                    lv_obj_set_style_text_color(settingsPage.button_name_labels[i], COLOR_TEXT_SECONDARY_NIGHT, 0);
                
                if (settingsPage. button_name_inputs[i])
                {
                    lv_obj_set_style_bg_color(settingsPage.button_name_inputs[i], COLOR_CARD_BG_NIGHT, 0);
                    lv_obj_set_style_border_color(settingsPage.button_name_inputs[i], COLOR_NEON_CYAN, 0);
                    lv_obj_set_style_text_color(settingsPage.button_name_inputs[i], lv_color_white(), 0);
                }
            }
            
            if (settingsPage.keyboard)
            {
                lv_obj_set_style_bg_color(settingsPage.keyboard, COLOR_SIDEBAR_BG_NIGHT, 0);
                lv_obj_set_style_border_color(settingsPage.keyboard, COLOR_NEON_CYAN, 0);
            }
            
            if (settingsPage.cancel_button)
            {
                lv_obj_set_style_bg_color(settingsPage.cancel_button, COLOR_CARD_BG_NIGHT, 0);
                lv_obj_set_style_border_color(settingsPage.cancel_button, COLOR_NEON_CYAN, 0);
                
                lv_obj_t *cancel_label = lv_obj_get_child(settingsPage.cancel_button, 0);
                if (cancel_label) lv_obj_set_style_text_color(cancel_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
            }
            
            if (settingsPage.save_button)
            {
                lv_obj_set_style_bg_color(settingsPage.save_button, COLOR_CARD_BG_NIGHT, 0);
                lv_obj_set_style_border_color(settingsPage.save_button, COLOR_NEON_CYAN, 0);
                lv_obj_set_style_shadow_color(settingsPage. save_button, COLOR_NEON_CYAN, 0);
                
                lv_obj_t *save_label = lv_obj_get_child(settingsPage.save_button, 0);
                if (save_label) lv_obj_set_style_text_color(save_label, COLOR_NEON_CYAN, 0);
            }
        }
    }
    
    // ========== HISTORY PAGE THEME UPDATES ========== kısmında (satır ~440 civarı)
if (settingsPage.history_container != NULL)
{
    if (is_day_mode)
    {
        // History container
        lv_obj_set_style_bg_color(settingsPage.history_container, COLOR_BG_MAIN_DAY, 0);
        
        // History header
        if (settingsPage.history_header != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.history_header, COLOR_BG_PANEL_DAY, 0);
            lv_obj_set_style_border_color(settingsPage.history_header, COLOR_BORDER_DAY, 0);
            
            // Logo
            lv_obj_t *history_logo = lv_obj_get_child(settingsPage.history_header, 0);
            if (history_logo) lv_obj_set_style_text_color(history_logo, COLOR_TEXT_PRIMARY_DAY, 0);
        }
        
        // Remove history_title update as it no longer exists
        
        // Back button
        if (settingsPage.back_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.back_button, COLOR_SIDEBAR_BG_DAY, 0);
            lv_obj_set_style_border_color(settingsPage.back_button, COLOR_BORDER_DAY, 0);
            
            lv_obj_t *back_icon = lv_obj_get_child(settingsPage.back_button, 0);
            if (back_icon) lv_obj_set_style_text_color(back_icon, lv_color_make(38, 134, 173), 0);
        }
        
        // History list
        if (settingsPage.history_list)
        {
            lv_obj_set_style_bg_color(settingsPage.history_list, COLOR_BG_MAIN_DAY, 0);
        }
    }
    else  // NIGHT MODE
    {
        lv_obj_set_style_bg_color(settingsPage. history_container, COLOR_BG_MAIN_NIGHT, 0);
        
        if (settingsPage.history_header != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.history_header, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.history_header, lv_color_make(148, 163, 184), 0);
            
            lv_obj_t *history_logo = lv_obj_get_child(settingsPage.history_header, 0);
            if (history_logo) lv_obj_set_style_text_color(history_logo, COLOR_TEXT_PRIMARY_NIGHT, 0);
        }
        
        // Remove history_title update
        
        if (settingsPage.back_button != NULL)
        {
            lv_obj_set_style_bg_color(settingsPage.back_button, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(settingsPage.back_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_t *back_icon = lv_obj_get_child(settingsPage.back_button, 0);
            if (back_icon) lv_obj_set_style_text_color(back_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
        }
        
        if (settingsPage. history_list)
        {
            lv_obj_set_style_bg_color(settingsPage.history_list, COLOR_BG_MAIN_NIGHT, 0);
        }
    }
}
    
    ESP_LOGI(s_tag, "Theme colors updated to %s mode", is_day_mode ? "DAY" : "NIGHT");
}


// Initialize NVS and load button names
void usrSettingsPage_loadButtonNames(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return;
    }

    // Load button names for all sections
    for (int section = 0; section < 3; section++) {
        for (int button = 0; button < 6; button++) {
            char key[32];
            snprintf(key, sizeof(key), "s%d_b%d", section, button);
            
            size_t required_size = MAX_BUTTON_NAME_LENGTH;
            ret = nvs_get_str(nvs_handle, key, button_names[section][button], &required_size);
            
            if (ret != ESP_OK) {
                ESP_LOGI(s_tag, "Using default name for section %d, button %d", section, button);
            }
        }
    }

    nvs_close(nvs_handle);
    
    // Load history
    load_history_from_nvs();
    
    ESP_LOGI(s_tag, "Button names and history loaded from NVS");
}

// Load history from NVS
static void load_history_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_HISTORY_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(s_tag, "History namespace not found, starting fresh");
        history_count = 0;
        return;
    }

    size_t required_size = sizeof(int);
    ret = nvs_get_blob(nvs_handle, "count", &history_count, &required_size);
    if (ret != ESP_OK) {
        history_count = 0;
    } else {
        if (history_count > MAX_HISTORY_ENTRIES) {
            history_count = MAX_HISTORY_ENTRIES;
        }
        
        if (history_count > 0) {
            required_size = sizeof(history_entry_t) * history_count;
            ret = nvs_get_blob(nvs_handle, "entries", history_entries, &required_size);
            if (ret != ESP_OK) {
                ESP_LOGE(s_tag, "Error loading history entries");
                history_count = 0;
            }
        }
    }

    nvs_close(nvs_handle);
    ESP_LOGI(s_tag, "Loaded %d history entries", history_count);
}

// Save history to NVS
static void save_history_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_HISTORY_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening history NVS handle");
        return;
    }

    ret = nvs_set_blob(nvs_handle, "count", &history_count, sizeof(int));
    if (ret == ESP_OK && history_count > 0) {
        ret = nvs_set_blob(nvs_handle, "entries", history_entries, 
                          sizeof(history_entry_t) * history_count);
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "History saved to NVS (%d entries)", history_count);
    } else {
        ESP_LOGE(s_tag, "Error saving history to NVS");
    }
}

// Add history entry
static void add_history_entry(int section, int button, const char *old_name, const char *new_name)
{
    // Shift existing entries if we're at max capacity
    if (history_count >= MAX_HISTORY_ENTRIES) {
        for (int i = 0; i < MAX_HISTORY_ENTRIES - 1; i++) {
            history_entries[i] = history_entries[i + 1];
        }
        history_count = MAX_HISTORY_ENTRIES - 1;
    }

    // Add new entry
    history_entry_t *entry = &history_entries[history_count];
    entry->timestamp = time(NULL);
    entry->section = section;
    entry->button = button;
    strncpy(entry->old_name, old_name, MAX_BUTTON_NAME_LENGTH - 1);
    strncpy(entry->new_name, new_name, MAX_BUTTON_NAME_LENGTH - 1);
    entry->old_name[MAX_BUTTON_NAME_LENGTH - 1] = '\0';
    entry->new_name[MAX_BUTTON_NAME_LENGTH - 1] = '\0';
    
    history_count++;
    save_history_to_nvs();
    
    ESP_LOGI(s_tag, "Added history entry: %s Button %d:  %s -> %s", 
             section_names[section], button + 1, old_name, new_name);
}

// Save button names to NVS
void usrSettingsPage_saveButtonNames(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle:  %s", esp_err_to_name(ret));
        return;
    }

    // Save button names for all sections
    for (int section = 0; section < 3; section++) {
        for (int button = 0; button < 6; button++) {
            char key[32];
            snprintf(key, sizeof(key), "s%d_b%d", section, button);
            
            ret = nvs_set_str(nvs_handle, key, button_names[section][button]);
            if (ret != ESP_OK) {
                ESP_LOGE(s_tag, "Error saving button name:  %s", esp_err_to_name(ret));
            }
        }
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing NVS:  %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    ESP_LOGI(s_tag, "Button names saved to NVS");
}

// Get button name
const char* usrSettingsPage_getButtonName(int section, int button)
{
    if (section >= 0 && section < 3 && button >= 0 && button < 6) {
        return button_names[section][button];
    }
    return "Invalid";
}

// Set button name
void usrSettingsPage_setButtonName(int section, int button, const char *name)
{
    if (section >= 0 && section < 3 && button >= 0 && button < 6 && name != NULL) {
        // Store old name for history
        char old_name[MAX_BUTTON_NAME_LENGTH];
        strncpy(old_name, button_names[section][button], MAX_BUTTON_NAME_LENGTH - 1);
        old_name[MAX_BUTTON_NAME_LENGTH - 1] = '\0';
        
        // Set new name
        strncpy(button_names[section][button], name, MAX_BUTTON_NAME_LENGTH - 1);
        button_names[section][button][MAX_BUTTON_NAME_LENGTH - 1] = '\0';
        
        // Add to history if names are different
        if (strcmp(old_name, name) != 0) {
            add_history_entry(section, button, old_name, name);
        }
    }
}

// History button event callback
static void history_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(s_tag, "History button clicked");
        show_history_page();
    }
}

// Back to main event callback
static void back_to_main_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(s_tag, "Back button clicked");
        hide_history_page();
    }
}

// Section button event callback
static void section_button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        // Find which section button was pressed
        for (int i = 0; i < 3; i++) {
            if (settingsPage.section_buttons[i] == btn) {
                show_editor_for_section(i);
                ESP_LOGI(s_tag, "Editing section %d (%s)", i, section_names[i]);
                break;
            }
        }
    }
}

// Save button event callback
static void save_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Get text from all text areas and save
        for (int i = 0; i < 6; i++) {
            const char *text = lv_textarea_get_text(settingsPage.button_name_inputs[i]);
            if (text && strlen(text) > 0) {
                usrSettingsPage_setButtonName(settingsPage.current_section, i, text);
            }
        }
        
        // Save to NVS
        usrSettingsPage_saveButtonNames();
        
        // Update button names on all pages
        usrToiletPage_updateButtonNames();
        usrLightingPage_updateButtonNames();
        usrSocketsPage_updateButtonNames();
        
        usrSettingsPage_setStatus("Button names saved successfully!");
        hide_editor();
        ESP_LOGI(s_tag, "Button names saved for section %d", settingsPage.current_section);
    }
}

// Cancel button event callback
static void cancel_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        hide_editor();
        usrSettingsPage_setStatus("Changes cancelled");
        ESP_LOGI(s_tag, "Button name editing cancelled");
    }
}

// Keyboard event callback
static void keyboard_event_cb(lv_event_t *e)
{
    lv_obj_t *keyboard = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    // Ready (Tik) veya Cancel (X) butonuna basılsa bile GİZLEME YAPMA
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(keyboard, NULL);
    }
}

// Text area event callback
static void textarea_event_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if (settingsPage.keyboard != NULL) {
            // Sadece hangi kutuya yazılacağını belirt, gizliliği değiştirme
            lv_keyboard_set_textarea(settingsPage.keyboard, ta);            
        }
    } 
}

// Edit password button event callback
static void edit_password_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Edit password button clicked - going to password change page");
        usrGraphicalInterface_showPage(PAGE_PASSWORD_CHANGE);
    }
}


// Create section selector interface
static void create_section_selector(void)
{
    usrGraphicalInterface_hideBackButton();
    // ==================== HEADER ====================
    settingsPage.header = lv_obj_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.header, LV_HOR_RES, 70);
    lv_obj_align(settingsPage.header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.header, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(settingsPage.header, 1, 0);
    lv_obj_set_style_border_side(settingsPage.header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(settingsPage.header, lv_color_make(148, 163, 184), 0);
    lv_obj_set_style_border_opa(settingsPage.header, LV_OPA_20, 0);
    lv_obj_set_style_radius(settingsPage.header, 0, 0);
    lv_obj_clear_flag(settingsPage.header, LV_OBJ_FLAG_SCROLLABLE);

    // Logo (SMART SYSTEM)
    lv_obj_t *logo = lv_label_create(settingsPage.header);
    lv_label_set_text(logo, "SMART SYSTEM");
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(logo, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 15, 0);

    // Edit Password button (Home butonunun solunda)
    lv_obj_t *edit_password_button = lv_btn_create(settingsPage. header);
    lv_obj_set_size(edit_password_button, 40, 40);
    lv_obj_align(edit_password_button, LV_ALIGN_RIGHT_MID, -60, 0);  // Home'dan 50px sola
    lv_obj_set_style_bg_color(edit_password_button, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(edit_password_button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(edit_password_button, 1, 0);
    lv_obj_set_style_border_color(edit_password_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(edit_password_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(edit_password_button, 8, 0);

    lv_obj_t *edit_icon = lv_label_create(edit_password_button);
    lv_label_set_text(edit_icon, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_font(edit_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(edit_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(edit_icon);

    lv_obj_add_event_cb(edit_password_button, edit_password_button_event_cb, LV_EVENT_CLICKED, NULL);

    // Home button
    settingsPage.home_button = lv_btn_create(settingsPage.header);
    lv_obj_set_size(settingsPage.home_button, 40, 40);
    lv_obj_align(settingsPage.home_button, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(settingsPage.home_button, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.home_button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(settingsPage.home_button, 1, 0);
    lv_obj_set_style_border_color(settingsPage.home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(settingsPage.home_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(settingsPage.home_button, 8, 0);
    
    lv_obj_t *home_icon = lv_label_create(settingsPage.home_button);
    lv_label_set_text(home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(home_icon);
    
    lv_obj_add_event_cb(settingsPage.home_button, home_button_event_cb, LV_EVENT_CLICKED, NULL);

    // ==================== MAIN TITLE ====================
    settingsPage.title_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.title_label, "SETTINGS");
    lv_obj_set_style_text_font(settingsPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(settingsPage.title_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(settingsPage.title_label, 3, 0);
    lv_obj_align(settingsPage.title_label, LV_ALIGN_TOP_MID, 0, 110);

    // ==================== SECTION CARDS ====================
    int card_width = 200;
    int card_height = 130;
    int card_gap = 15;
    
    // Section icons
    const char *section_icons[3] = {LV_SYMBOL_TINT, LV_SYMBOL_CHARGE, LV_SYMBOL_USB};
    
    
    for (int i = 0; i < 3; i++) {
        // Create card
        settingsPage.section_buttons[i] = lv_obj_create(settingsPage.page_container);
        lv_obj_set_size(settingsPage.section_buttons[i], card_width, card_height);
        
        // Positioning:  side by side
        int x_offset;
        int y_offset = 180;
        
        if (i == 0) {
            x_offset = -card_width - card_gap;
        } else if (i == 1) {
            x_offset = 0;
        } else {
            x_offset = card_width + card_gap;
        }
        
        lv_obj_align(settingsPage.section_buttons[i], LV_ALIGN_TOP_MID, x_offset, y_offset);
        
        // Card styling
        lv_obj_set_style_bg_color(settingsPage.section_buttons[i], COLOR_CARD_BG_NIGHT, 0);
        lv_obj_set_style_bg_opa(settingsPage.section_buttons[i], LV_OPA_80, 0);
        lv_obj_set_style_border_width(settingsPage.section_buttons[i], 2, 0);
        lv_obj_set_style_border_color(settingsPage.section_buttons[i], lv_color_make(30, 50, 70), 0);
        lv_obj_set_style_border_opa(settingsPage.section_buttons[i], LV_OPA_60, 0);
        lv_obj_set_style_radius(settingsPage.section_buttons[i], 15, 0);
        lv_obj_set_style_shadow_width(settingsPage.section_buttons[i], 15, 0);
        lv_obj_set_style_shadow_color(settingsPage.section_buttons[i], COLOR_NEON_CYAN, 0);
        lv_obj_set_style_shadow_opa(settingsPage.section_buttons[i], LV_OPA_20, 0);
        lv_obj_set_style_pad_all(settingsPage.section_buttons[i], 15, 0);
        lv_obj_clear_flag(settingsPage.section_buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(settingsPage.section_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        
        // Icon
        lv_obj_t *icon = lv_label_create(settingsPage.section_buttons[i]);
        lv_label_set_text(icon, section_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(icon, COLOR_NEON_CYAN, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);
        
        // Title
        settingsPage.section_labels[i] = lv_label_create(settingsPage.section_buttons[i]);
        char title_text[32];
        snprintf(title_text, sizeof(title_text), "%s\nBUTTON NAMES", 
                strcmp(section_names[i], "Toilet") == 0 ? "TOILET" : 
                strcmp(section_names[i], "Lighting") == 0 ? "LIGHTING" :  "SOCKETS");
        lv_label_set_text(settingsPage.section_labels[i], title_text);
        lv_obj_set_style_text_font(settingsPage.section_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(settingsPage.section_labels[i], COLOR_TEXT_PRIMARY_NIGHT, 0);
        lv_obj_set_style_text_align(settingsPage.section_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(settingsPage.section_labels[i], LV_ALIGN_CENTER, 0, 15);
        
        
        // Add event callback
        lv_obj_add_event_cb(settingsPage.section_buttons[i], section_button_event_cb, 
                            LV_EVENT_CLICKED, NULL);
    }

    // ==================== HISTORY BUTTON ====================
    settingsPage.history_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.history_button, 150, 50);
    lv_obj_align(settingsPage.history_button, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_set_style_bg_color(settingsPage.history_button, lv_color_make(20, 40, 60), 0);
    lv_obj_set_style_bg_opa(settingsPage.history_button, LV_OPA_80, 0);
    lv_obj_set_style_border_width(settingsPage.history_button, 2, 0);
    lv_obj_set_style_border_color(settingsPage.history_button, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_border_opa(settingsPage.history_button, LV_OPA_80, 0);
    lv_obj_set_style_radius(settingsPage.history_button, 25, 0);
    lv_obj_set_style_shadow_width(settingsPage.history_button, 15, 0);
    lv_obj_set_style_shadow_color(settingsPage.history_button, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_shadow_opa(settingsPage.history_button, LV_OPA_40, 0);
    
    // History icon
    lv_obj_t *history_icon = lv_label_create(settingsPage.history_button);
    lv_label_set_text(history_icon, LV_SYMBOL_SAVE);
    lv_obj_set_style_text_font(history_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(history_icon, COLOR_NEON_CYAN, 0);
    lv_obj_align(history_icon, LV_ALIGN_LEFT_MID, 15, 0);
    
    // History label
    lv_obj_t *history_label = lv_label_create(settingsPage.history_button);
    lv_label_set_text(history_label, " HISTORY");
    lv_obj_set_style_text_font(history_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(history_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(history_label, LV_ALIGN_CENTER, 10, 0);
    
    lv_obj_add_event_cb(settingsPage.history_button, history_button_event_cb, LV_EVENT_CLICKED, NULL);
}

// Create button name editor interface
static void create_button_name_editor(void)
{
    // Create editor container (Full screen with theme background)
    settingsPage.editor_container = lv_obj_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.editor_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(settingsPage.editor_container, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.editor_container, COLOR_BG_MAIN_NIGHT, 0);  // Artık siyah değil
    lv_obj_set_style_bg_opa(settingsPage.editor_container, LV_OPA_100, 0);
    lv_obj_set_style_border_width(settingsPage.editor_container, 0, 0);
    lv_obj_set_style_radius(settingsPage.editor_container, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.editor_container, 0, 0);
    lv_obj_clear_flag(settingsPage.editor_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settingsPage.editor_container, LV_OBJ_FLAG_HIDDEN);
    
    // ==================== EDITOR HEADER ====================
    settingsPage.editor_header = lv_obj_create(settingsPage.editor_container);
    lv_obj_set_size(settingsPage.editor_header, LV_HOR_RES, 70);
    lv_obj_align(settingsPage.editor_header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.editor_header, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage. editor_header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(settingsPage.editor_header, 1, 0);
    lv_obj_set_style_border_side(settingsPage.editor_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(settingsPage.editor_header, lv_color_make(148, 163, 184), 0);
    lv_obj_set_style_border_opa(settingsPage.editor_header, LV_OPA_20, 0);
    lv_obj_set_style_radius(settingsPage.editor_header, 0, 0);
    lv_obj_clear_flag(settingsPage.editor_header, LV_OBJ_FLAG_SCROLLABLE);

    


    // Editor Title (Header içinde, ortada)
    settingsPage.editor_title = lv_label_create(settingsPage.editor_header);
    lv_label_set_text(settingsPage.editor_title, "BUTTON NAMING");
    lv_obj_set_style_text_font(settingsPage.editor_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settingsPage.editor_title, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(settingsPage.editor_title, 2, 0);
    lv_obj_align(settingsPage.editor_title, LV_ALIGN_LEFT_MID, 15, 0);
    
    // Home button (Header içinde, sağda)
    settingsPage.editor_home_button = lv_btn_create(settingsPage.editor_header);
    lv_obj_set_size(settingsPage.editor_home_button, 40, 40);
    lv_obj_align(settingsPage.editor_home_button, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(settingsPage. editor_home_button, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.editor_home_button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(settingsPage.editor_home_button, 1, 0);
    lv_obj_set_style_border_color(settingsPage.editor_home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(settingsPage.editor_home_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(settingsPage.editor_home_button, 8, 0);
    
    lv_obj_t *editor_home_icon = lv_label_create(settingsPage.editor_home_button);
    lv_label_set_text(editor_home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(editor_home_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(editor_home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(editor_home_icon);
    
    lv_obj_add_event_cb(settingsPage.editor_home_button, editor_home_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    // ==================== INPUT FIELDS (2 columns, 3 rows) ====================
    int label_width = 75;       // Etiket (PWM 01 vb.) için ayrılan genişlik
    int input_width = 200;      
    int input_height = 50;
    int col_gap = 50;           // İki sütun arasındaki boşluk
    int row_gap = 5;           // Satırlar arasındaki boşluk
    int start_x_left = 100;
    int start_x_right = start_x_left + label_width + input_width + col_gap;
    int start_y = 85;

    for (int i = 0; i < 6; i++) {
        int row = i / 2;
        int col = i % 2;
        
        int x = (col == 0) ? start_x_left : start_x_right;
        int y = start_y + row * (input_height + row_gap);
        
        // Button label (PWM 01, PWM 02, etc.)
        settingsPage.button_name_labels[i] = lv_label_create(settingsPage.editor_container);
        char button_label[16];
        snprintf(button_label, sizeof(button_label), "PWM %02d", i + 1);
        lv_label_set_text(settingsPage.button_name_labels[i], button_label);
        lv_obj_set_style_text_font(settingsPage.button_name_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(settingsPage.button_name_labels[i], COLOR_TEXT_SECONDARY_NIGHT, 0);
        lv_obj_set_pos(settingsPage.button_name_labels[i], x, y + 10);
        
        // Text area for editing button name
        settingsPage.button_name_inputs[i] = lv_textarea_create(settingsPage.editor_container);
        lv_obj_set_size(settingsPage.button_name_inputs[i], input_width, input_height);
        lv_obj_set_pos(settingsPage.button_name_inputs[i], x + label_width, y);
        lv_textarea_set_one_line(settingsPage.button_name_inputs[i], true);
        lv_textarea_set_max_length(settingsPage.button_name_inputs[i], MAX_BUTTON_NAME_LENGTH - 1);
        
        // Dark theme styling for text areas
        lv_obj_set_style_bg_color(settingsPage.button_name_inputs[i], COLOR_CARD_BG_NIGHT, 0);
        lv_obj_set_style_bg_opa(settingsPage.button_name_inputs[i], LV_OPA_80, 0);
        lv_obj_set_style_border_width(settingsPage.button_name_inputs[i], 2, 0);
        lv_obj_set_style_border_color(settingsPage.button_name_inputs[i], COLOR_NEON_CYAN, 0);
        lv_obj_set_style_border_opa(settingsPage.button_name_inputs[i], LV_OPA_70, 0);
        lv_obj_set_style_radius(settingsPage. button_name_inputs[i], 8, 0);
        lv_obj_set_style_text_color(settingsPage. button_name_inputs[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(settingsPage. button_name_inputs[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_all(settingsPage.button_name_inputs[i], 8, 0);
        
        // Focus styling
        lv_obj_set_style_border_color(settingsPage.button_name_inputs[i], COLOR_NEON_CYAN, LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(settingsPage.button_name_inputs[i], LV_OPA_100, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(settingsPage.button_name_inputs[i], 10, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_color(settingsPage.button_name_inputs[i], COLOR_NEON_CYAN, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_opa(settingsPage. button_name_inputs[i], LV_OPA_50, LV_STATE_FOCUSED);
        
        lv_obj_add_event_cb(settingsPage.button_name_inputs[i], textarea_event_cb, LV_EVENT_ALL, NULL);
    }
    
    // ==================== KEYBOARD ====================
    settingsPage.keyboard = lv_keyboard_create(settingsPage.editor_container);
    lv_obj_set_size(settingsPage.keyboard, LV_HOR_RES - 90, 170);
    lv_obj_align(settingsPage.keyboard, LV_ALIGN_BOTTOM_MID, 0, -65);
    lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_set_style_bg_color(settingsPage. keyboard, COLOR_SIDEBAR_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.keyboard, LV_OPA_100, 0);
    //lv_obj_set_style_border_width(settingsPage.keyboard, 2, 0);
    //lv_obj_set_style_border_color(settingsPage.keyboard, COLOR_NEON_CYAN, 0);
    //lv_obj_set_style_border_opa(settingsPage. keyboard, LV_OPA_40, 0);
    lv_obj_set_style_radius(settingsPage.keyboard, 10, 0);
    
    lv_obj_add_event_cb(settingsPage. keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    
    // ==================== CANCEL & SAVE BUTTONS ====================
    int button_width = 130;
    int button_height = 45;
    int button_gap = 20;
    
    // Cancel button
    settingsPage. cancel_button = lv_btn_create(settingsPage.editor_container);
    lv_obj_set_size(settingsPage.cancel_button, button_width, button_height);
    lv_obj_align(settingsPage.cancel_button, LV_ALIGN_BOTTOM_LEFT, 
                 (LV_HOR_RES - button_width * 2 - button_gap) / 2, -10);
    
    lv_obj_set_style_bg_color(settingsPage. cancel_button, COLOR_CARD_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.cancel_button, LV_OPA_80, 0);
    lv_obj_set_style_border_width(settingsPage. cancel_button, 2, 0);
    lv_obj_set_style_border_color(settingsPage.cancel_button, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_border_opa(settingsPage.cancel_button, LV_OPA_60, 0);
    lv_obj_set_style_radius(settingsPage.cancel_button, 10, 0);
    lv_obj_set_style_shadow_width(settingsPage.cancel_button, 0, 0);
    
    lv_obj_set_style_bg_color(settingsPage.cancel_button, lv_color_make(20, 35, 50), LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(settingsPage.cancel_button, LV_OPA_100, LV_STATE_PRESSED);
    
    lv_obj_t *cancel_label = lv_label_create(settingsPage.cancel_button);
    lv_label_set_text(cancel_label, "CANCEL");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cancel_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_center(cancel_label);
    
    lv_obj_add_event_cb(settingsPage.cancel_button, cancel_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    // Save button
    settingsPage.save_button = lv_btn_create(settingsPage. editor_container);
    lv_obj_set_size(settingsPage.save_button, button_width, button_height);
    lv_obj_align(settingsPage.save_button, LV_ALIGN_BOTTOM_RIGHT, 
                 -(LV_HOR_RES - button_width * 2 - button_gap) / 2, -10);
    
    lv_obj_set_style_bg_color(settingsPage.save_button, COLOR_CARD_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage. save_button, LV_OPA_80, 0);
    lv_obj_set_style_border_width(settingsPage.save_button, 2, 0);
    lv_obj_set_style_border_color(settingsPage.save_button, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_border_opa(settingsPage.save_button, LV_OPA_80, 0);
    lv_obj_set_style_radius(settingsPage.save_button, 10, 0);
    lv_obj_set_style_shadow_width(settingsPage.save_button, 10, 0);
    lv_obj_set_style_shadow_color(settingsPage.save_button, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_shadow_opa(settingsPage.save_button, LV_OPA_40, 0);
    
    lv_obj_set_style_bg_color(settingsPage.save_button, lv_color_make(0, 210, 255), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(settingsPage.save_button, 15, LV_STATE_PRESSED);
    
    lv_obj_t *save_label = lv_label_create(settingsPage.save_button);
    lv_label_set_text(save_label, "SAVE");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_label, COLOR_NEON_CYAN, 0);
    lv_obj_center(save_label);
    
    lv_obj_add_event_cb(settingsPage.save_button, save_button_event_cb, LV_EVENT_CLICKED, NULL);
}


// Create history page
static void create_history_page(void)
{
    settingsPage. history_container = lv_obj_create(lv_obj_get_parent(settingsPage.page_container));
    lv_obj_set_size(settingsPage.history_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(settingsPage.history_container, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.history_container, COLOR_BG_MAIN_NIGHT, 0);
    lv_obj_set_style_border_width(settingsPage.history_container, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.history_container, 0, 0);
    lv_obj_clear_flag(settingsPage.history_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(settingsPage.history_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(settingsPage.history_container, 0, 0);

    // ==================== HISTORY HEADER ====================
    settingsPage.history_header = lv_obj_create(settingsPage.history_container);
    lv_obj_set_size(settingsPage.history_header, LV_HOR_RES, 70);
    lv_obj_align(settingsPage.history_header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.history_header, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.history_header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(settingsPage.history_header, 1, 0);
    lv_obj_set_style_border_side(settingsPage.history_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(settingsPage.history_header, lv_color_make(148, 163, 184), 0);
    lv_obj_set_style_border_opa(settingsPage.history_header, LV_OPA_20, 0);
    lv_obj_set_style_radius(settingsPage.history_header, 0, 0);
    lv_obj_clear_flag(settingsPage.history_header, LV_OBJ_FLAG_SCROLLABLE);

    // Logo 
    lv_obj_t *history_logo = lv_label_create(settingsPage.history_header);
    lv_label_set_text(history_logo, "NAMING HISTORY");
    lv_obj_set_style_text_font(history_logo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(history_logo, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(history_logo, LV_ALIGN_LEFT_MID, 15, 0);

    
    // Back button
    settingsPage.back_button = lv_btn_create(settingsPage.history_header);
    lv_obj_set_size(settingsPage.back_button, 40, 40);
    lv_obj_align(settingsPage.back_button, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(settingsPage.back_button, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage. back_button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(settingsPage.back_button, 1, 0);
    lv_obj_set_style_border_color(settingsPage.back_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(settingsPage.back_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(settingsPage.back_button, 8, 0);
    
    lv_obj_t *back_icon = lv_label_create(settingsPage.back_button);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(back_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(back_icon);
    
    lv_obj_add_event_cb(settingsPage.back_button, back_to_main_event_cb, LV_EVENT_CLICKED, NULL);
    
    // ==================== SCROLLABLE CONTENT AREA ====================
    settingsPage.history_list = lv_obj_create(settingsPage. history_container);
    lv_obj_set_size(settingsPage.history_list, LV_HOR_RES - 40, LV_VER_RES - 110);
    lv_obj_align(settingsPage.history_list, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_set_style_bg_color(settingsPage.history_list, COLOR_BG_MAIN_NIGHT, 0);
    lv_obj_set_style_bg_opa(settingsPage.history_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(settingsPage.history_list, 0, 0);
    lv_obj_set_style_radius(settingsPage.history_list, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.history_list, 0, 0);
    lv_obj_set_flex_flow(settingsPage.history_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(settingsPage.history_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(settingsPage.history_list, 0, 10);
    
    // ==================== COLUMN HEADERS ====================
    lv_obj_t *header_row = lv_obj_create(settingsPage.history_list);
    lv_obj_set_size(header_row, LV_HOR_RES - 60, 40);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(header_row, 0, 0);
    lv_obj_set_style_pad_all(header_row, 5, 0);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_SCROLLABLE);
    
    // Button ID Header
    lv_obj_t *btn_id_header = lv_label_create(header_row);
    lv_label_set_text(btn_id_header, "BUTTON ID");
    lv_obj_set_style_text_font(btn_id_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(btn_id_header, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_pos(btn_id_header, 45, 10);
    
    // Old Name Header
    lv_obj_t *old_name_header = lv_label_create(header_row);
    lv_label_set_text(old_name_header, "OLD NAME");
    lv_obj_set_style_text_font(old_name_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(old_name_header, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_pos(old_name_header, 200, 10);
    
    // New Name Header
    lv_obj_t *new_name_header = lv_label_create(header_row);
    lv_label_set_text(new_name_header, "NEW NAME");
    lv_obj_set_style_text_font(new_name_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(new_name_header, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_pos(new_name_header, 430, 10);
}

// Show history page and populate with entries
static void show_history_page(void)
{
    ESP_LOGI(s_tag, "Showing history page with %d entries", history_count);
    
    settingsPage.is_history_active = true;
    usrGraphicalInterface_hideBackButton();
    
    lv_obj_add_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
    
    // Clear existing history list items (keep only the header row)
    uint32_t child_count = lv_obj_get_child_cnt(settingsPage.history_list);
    for (int32_t i = child_count - 1; i > 0; i--) {
        lv_obj_t *child = lv_obj_get_child(settingsPage. history_list, i);
        lv_obj_del(child);
    }
    
    // Add history entries
    if (history_count == 0) {
        lv_obj_t *empty_card = lv_obj_create(settingsPage.history_list);
        lv_obj_set_size(empty_card, LV_HOR_RES - 60, 60);
        lv_obj_set_style_bg_color(empty_card, COLOR_CARD_BG_NIGHT, 0);
        lv_obj_set_style_bg_opa(empty_card, LV_OPA_80, 0);
        lv_obj_set_style_border_width(empty_card, 0, 0);
        lv_obj_set_style_radius(empty_card, 8, 0);
        lv_obj_clear_flag(empty_card, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t *empty_label = lv_label_create(empty_card);
        lv_label_set_text(empty_label, "No history entries found");
        lv_obj_set_style_text_color(empty_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_center(empty_label);
    } else {
        for (int i = history_count - 1; i >= 0; i--) {
            history_entry_t *entry = &history_entries[i];
            
            // Create card for each entry
            lv_obj_t *card = lv_obj_create(settingsPage.history_list);
            lv_obj_set_size(card, LV_HOR_RES - 60, 60);
            lv_obj_set_style_bg_color(card, COLOR_CARD_BG_NIGHT, 0);
            lv_obj_set_style_bg_opa(card, LV_OPA_80, 0);
            lv_obj_set_style_border_width(card, 0, 0);
            lv_obj_set_style_radius(card, 8, 0);
            lv_obj_set_style_pad_all(card, 15, 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
            
            // Button ID (left side - blue box)
            lv_obj_t *btn_id_box = lv_obj_create(card);
            lv_obj_set_size(btn_id_box, 70, 30);
            lv_obj_set_pos(btn_id_box, 30, 0);
            lv_obj_set_style_bg_color(btn_id_box, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_bg_opa(btn_id_box, LV_OPA_20, 0);
            lv_obj_set_style_border_width(btn_id_box, 1, 0);
            lv_obj_set_style_border_color(btn_id_box, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_border_opa(btn_id_box, LV_OPA_60, 0);
            lv_obj_set_style_radius(btn_id_box, 4, 0);
            lv_obj_clear_flag(btn_id_box, LV_OBJ_FLAG_SCROLLABLE);
            
            lv_obj_t *btn_id_label = lv_label_create(btn_id_box);
            char btn_id_text[16];
            snprintf(btn_id_text, sizeof(btn_id_text), "PWM %02d", entry->button + 1);
            lv_label_set_text(btn_id_label, btn_id_text);
            lv_obj_set_style_text_font(btn_id_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(btn_id_label, COLOR_NEON_CYAN, 0);
            lv_obj_center(btn_id_label);
            
            // Old Name (middle - gray text)
            lv_obj_t *old_name_label = lv_label_create(card);
            lv_label_set_text(old_name_label, entry->old_name);
            lv_obj_set_style_text_font(old_name_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(old_name_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
            lv_obj_set_pos(old_name_label, 190, 5);
            
            // New Name (right - cyan text)
            lv_obj_t *new_name_label = lv_label_create(card);
            lv_label_set_text(new_name_label, entry->new_name);
            lv_obj_set_style_text_font(new_name_label, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(new_name_label, COLOR_NEON_CYAN, 0);
            lv_obj_set_pos(new_name_label, 420, 5);
        }
    }
    
    lv_obj_clear_flag(settingsPage.history_container, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(s_tag, "History page displayed successfully");
}


// Hide history page
static void hide_history_page(void)
{
    ESP_LOGI(s_tag, "Hiding history page");
    
    settingsPage.is_history_active = false;

    usrGraphicalInterface_hideBackButton();
    
    lv_obj_add_flag(settingsPage.history_container, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
}

// Show editor for specific section
static void show_editor_for_section(int section)
{
    usrGraphicalInterface_hideBackButton();
    if (section < 0 || section >= 3) return;
    
    settingsPage.current_section = section;
    settingsPage.is_editor_active = true;
    
    // Update editor title based on section
    char title_text[64];
    if (section == 0) {
        snprintf(title_text, sizeof(title_text), LV_SYMBOL_SETTINGS "  TOILET PAGE BUTTON NAMING");
    } else if (section == 1) {
        snprintf(title_text, sizeof(title_text), LV_SYMBOL_SETTINGS "  LIGHTING PAGE BUTTON NAMING");
    } else {
        snprintf(title_text, sizeof(title_text), LV_SYMBOL_SETTINGS "  SOCKETS PAGE BUTTON NAMING");
    }
    lv_label_set_text(settingsPage.editor_title, title_text);
    
    // Fill text areas with current button names
    for (int i = 0; i < 6; i++) {
        lv_textarea_set_text(settingsPage.button_name_inputs[i], 
                            usrSettingsPage_getButtonName(section, i));
    }
    
    // Hide section selector and show editor
    lv_obj_add_flag(settingsPage.title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settingsPage.history_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settingsPage.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settingsPage.header, LV_OBJ_FLAG_HIDDEN);
    
    for (int i = 0; i < 3; i++) {
        lv_obj_add_flag(settingsPage.section_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(settingsPage.editor_container, LV_OBJ_FLAG_HIDDEN);

    if (settingsPage.keyboard) {
        lv_obj_clear_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // sayfa açılır açılmaz ilk kutucuğu (PWM 01) seçili hale getirmek için:
        /*
        if (settingsPage.button_name_inputs[0]) {
            lv_obj_add_state(settingsPage.button_name_inputs[0], LV_STATE_FOCUSED);
            lv_keyboard_set_textarea(settingsPage.keyboard, settingsPage.button_name_inputs[0]);
        }
        */
    }
}


// Hide editor and show section selector
static void hide_editor(void)
{
    settingsPage.is_editor_active = false;
    
    usrGraphicalInterface_hideBackButton();
    
    // Hide keyboard if visible
    //lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Hide editor and show section selector
    lv_obj_add_flag(settingsPage.editor_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settingsPage.title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settingsPage.history_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settingsPage.status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settingsPage.header, LV_OBJ_FLAG_HIDDEN);
    
    for (int i = 0; i < 3; i++) {
        lv_obj_clear_flag(settingsPage.section_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void usrSettingsPage_init(lv_obj_t *parent)
{
    if (settingsPage.page_container != NULL) {
        ESP_LOGW(s_tag, "Settings page already initialized");
        return;
    }
    
    // Parent referansını sakla
    settingsPage.parent = parent;
    
    // Load button names from NVS
    usrSettingsPage_loadButtonNames();
    
    // Main container
    settingsPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(settingsPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(settingsPage.page_container, COLOR_BG_MAIN_NIGHT, 0);
    lv_obj_set_style_border_width(settingsPage.page_container, 0, 0);
    lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(settingsPage.page_container, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.page_container, 0, 0);
    
    // Create section selector
    create_section_selector();
    
    // Create button name editor (initially hidden)
    create_button_name_editor();
    
    // Create history page (initially hidden)
    create_history_page();
    
    // Create status label
    settingsPage.status_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.status_label, "Select a section to edit button names");
    lv_obj_set_style_text_font(settingsPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settingsPage.status_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_align(settingsPage.status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Initially hide the page
    usrSettingsPage_hide();
    
    ESP_LOGI(s_tag, "Settings page initialized with modern UI and day/night mode");
}

void usrSettingsPage_show(void)
{
    if (settingsPage.page_container != NULL) {
        lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        settingsPage.is_active = true;
        
        // Make sure we're showing the section selector, not the editor or history
        hide_editor();
        hide_history_page();
        
        ESP_LOGI(s_tag, "Settings page shown");
    }
}

void usrSettingsPage_hide(void)
{
    if (settingsPage.page_container != NULL) {
        lv_obj_add_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        settingsPage.is_active = false;
        
        // Hide keyboard if visible
        if (settingsPage.keyboard) {
            lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        
        ESP_LOGI(s_tag, "Settings page hidden");
    }
}

void usrSettingsPage_destroy(void)
{
    if (settingsPage.page_container != NULL) {
        lv_obj_del(settingsPage.page_container);
        memset(&settingsPage, 0, sizeof(usrSettingsPage_t));
        ESP_LOGI(s_tag, "Settings page destroyed");
    }
}

void usrSettingsPage_setStatus(const char *status)
{
    if (settingsPage.status_label != NULL && status != NULL) {
        lv_label_set_text(settingsPage.status_label, status);
    }
}

bool usrSettingsPage_isActive(void)
{
    return settingsPage.is_active;
}

// History management functions for header file
int usrSettingsPage_getHistoryCount(void)
{
    return history_count;
}

void usrSettingsPage_clearHistory(void)
{
    history_count = 0;
    save_history_to_nvs();
    ESP_LOGI(s_tag, "History cleared");
}