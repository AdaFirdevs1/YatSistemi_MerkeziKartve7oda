#include "usrSocketsPage.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "usrSettingsPage.h"
#include "usrCanDynamicIDMaster.h"
#include "usrGraphicalInterface.h"

static const char *s_tag = "usrSocketsPage";
static usrSocketsPage_t buttonPage = {0};

// Röle komut tanımları (LED ile aynı mantık)
#define RELAY_CMD_TOGGLE          0x20
#define RELAY_CMD_SET             0x21


// Color Palette - NIGHT MODE
#define COLOR_BG_MAIN_NIGHT lv_color_make(8, 15, 22)
#define COLOR_BG_PANEL_NIGHT lv_color_make(12, 20, 28)
#define COLOR_NEON_CYAN lv_color_make(0, 210, 255)
#define COLOR_NEON_BLUE lv_color_make(0, 163, 224)
#define COLOR_TEXT_PRIMARY_NIGHT lv_color_make(255, 255, 255)
#define COLOR_TEXT_SECONDARY_NIGHT lv_color_make(138, 155, 179)
#define COLOR_SIDEBAR_BG_NIGHT lv_color_make(10, 18, 26)
#define COLOR_CARD_BG_NIGHT lv_color_make(15, 25, 35)

// Color Palette - DAY MODE
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

// Forward declarations
static void close_all_btn_cb(lv_event_t *e);
static void update_sidebar_indicators(void);

// Dinamik buton ismi alma fonksiyonu
static const char* get_button_name(int index)
{
    return usrSettingsPage_getButtonName(2, index); // 2 = Sockets section
}

// Röle kartının dinamik ID'sini al
static uint32_t getRelayNodeID(void)
{
    return getNodeIDByType(NODE_TYPE_RELAY);
}

// Home button event callback
static void home_button_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Home button clicked - returning to main menu");
        usrGraphicalInterface_showPage(PAGE_MAIN_MENU);
    }
}

// Sidebar gösterge güncelleme fonksiyonu
static void update_sidebar_indicators(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (buttonPage. sidebar_btns[i] != NULL)
        {
            lv_obj_t *indicator = NULL;
            
            if (lv_obj_get_child_cnt(buttonPage.sidebar_btns[i]) >= 2)
            {
                indicator = lv_obj_get_child(buttonPage.sidebar_btns[i], 1);
            }
            else
            {
                // Indicator oluştur
                indicator = lv_obj_create(buttonPage.sidebar_btns[i]);
                lv_obj_set_size(indicator, 18, 18);
                lv_obj_align(indicator, LV_ALIGN_RIGHT_MID, 7, 0);
                lv_obj_set_style_radius(indicator, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_border_width(indicator, 0, 0);
                lv_obj_set_style_pad_all(indicator, 0, 0);
                lv_obj_clear_flag(indicator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
                
                // İkon ekle
                lv_obj_t *icon = lv_label_create(indicator);
                lv_label_set_text(icon, LV_SYMBOL_CHARGE);
                lv_obj_set_style_text_font(icon, &lv_font_montserrat_12, 0);
                lv_obj_center(icon);
            }
            
            lv_obj_t *icon = lv_obj_get_child(indicator, 0);
            
            if (buttonPage.button_states[i])
            {
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                // AÇIK
                lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
                
                if (is_day_mode)
                {
                    lv_obj_set_style_bg_color(indicator, lv_color_make(225, 242, 252), 0);
                    lv_obj_set_style_bg_opa(indicator, LV_OPA_100, 0);
                    lv_obj_set_style_shadow_width(indicator, 8, 0);
                    lv_obj_set_style_shadow_color(indicator, COLOR_ACCENT_BLUE_DAY, 0);
                    lv_obj_set_style_shadow_opa(indicator, LV_OPA_50, 0);
                    
                    if (icon) lv_obj_set_style_text_color(icon, COLOR_ACCENT_BLUE_DAY, 0);
                }
                else
                {
                    lv_obj_set_style_bg_color(indicator, lv_color_make(20, 40, 60), 0);
                    lv_obj_set_style_bg_opa(indicator, LV_OPA_80, 0);
                    lv_obj_set_style_shadow_width(indicator, 12, 0);
                    lv_obj_set_style_shadow_color(indicator, COLOR_NEON_CYAN, 0);
                    lv_obj_set_style_shadow_opa(indicator, LV_OPA_80, 0);
                    lv_obj_set_style_shadow_spread(indicator, 2, 0);
                    
                    if (icon) lv_obj_set_style_text_color(icon, COLOR_NEON_CYAN, 0);
                }
            }
            else
            {
                // KAPALI
                lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}


// Update theme colors based on day/night mode
void usrSocketsPage_updateThemeColors(void)
{
    bool is_day_mode = !usrGraphicalInterface_isDarkMode();
    if (buttonPage.page_container == NULL) return;
    
    if (is_day_mode)
    {
        // ========== DAY MODE ==========
        
        lv_obj_set_style_bg_color(buttonPage.page_container, COLOR_BG_MAIN_DAY, 0);
        
        // Header
        lv_obj_t *header = lv_obj_get_child(buttonPage.page_container, 0);
        if (header != NULL)
        {
            lv_obj_set_style_bg_color(header, COLOR_BG_PANEL_DAY, 0);
            lv_obj_set_style_bg_opa(header, LV_OPA_100, 0);
            lv_obj_set_style_text_color(buttonPage.header_icon, COLOR_ACCENT_BLUE_DAY, 0);
            lv_obj_set_style_text_color(buttonPage.title_label, COLOR_TEXT_PRIMARY_DAY, 0);
            
            lv_obj_t *subtitle = lv_obj_get_child(header, 2);
            if (subtitle) lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_DAY, 0);
            
            lv_obj_set_style_bg_color(buttonPage.home_button, COLOR_SIDEBAR_BG_DAY, 0);
            lv_obj_set_style_border_color(buttonPage.home_button, COLOR_BORDER_DAY, 0);
            
            lv_obj_t *home_icon = lv_obj_get_child(buttonPage.home_button, 0);
            if (home_icon) lv_obj_set_style_text_color(home_icon, lv_color_make(38, 134, 173), 0);
        }
        
        // Sidebar
        lv_obj_t *sidebar = lv_obj_get_child(buttonPage.page_container, 1);
        if (sidebar != NULL)
        {
            lv_obj_set_style_bg_color(sidebar, lv_color_make(241, 246, 250), 0);
            lv_obj_set_style_bg_opa(sidebar, LV_OPA_100, 0);
            
            for (int i = 0; i < 6; i++)
            {
                if (buttonPage.sidebar_btns[i] != NULL)
                {
                    lv_obj_t *label = lv_obj_get_child(buttonPage.sidebar_btns[i], 0);
                    
                    if (i == buttonPage.selected_index)
                    {
                        lv_obj_set_style_bg_color(buttonPage.sidebar_btns[i], lv_color_make(225, 236, 244), 0);
                        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_100, 0);
                        lv_obj_set_style_border_color(buttonPage.sidebar_btns[i], COLOR_ACCENT_BLUE_DAY, 0);
                        lv_obj_set_style_shadow_opa(buttonPage.sidebar_btns[i], LV_OPA_30, 0);
                        if (label) lv_obj_set_style_text_color(label, COLOR_ACCENT_BLUE_DAY, 0);
                    }
                    else
                    {
                        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_TRANSP, 0);
                        lv_obj_set_style_border_color(buttonPage.sidebar_btns[i], COLOR_BORDER_DAY, 0);
                        lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[i], 0, 0);
                        if (label) lv_obj_set_style_text_color(label, COLOR_TEXT_SECONDARY_DAY, 0);
                    }
                }
            }
            
            lv_obj_t *close_btn = lv_obj_get_child(sidebar, 6);
            if (close_btn)
            {
                lv_obj_set_style_bg_color(close_btn, COLOR_SIDEBAR_BG_DAY, 0);
                lv_obj_set_style_border_color(close_btn, COLOR_BORDER_DAY, 0);
                lv_obj_t *close_label = lv_obj_get_child(close_btn, 0);
                if (close_label) lv_obj_set_style_text_color(close_label, COLOR_TEXT_SECONDARY_DAY, 0);
            }
        }
        
        // Center panel
        lv_obj_t *center_panel = lv_obj_get_child(buttonPage.page_container, 2);
        if (center_panel != NULL)
        {
            lv_obj_set_style_bg_color(center_panel, lv_color_make(241, 246, 250), 0);
            lv_obj_set_style_text_color(buttonPage.main_device_name, COLOR_ACCENT_BLUE_DAY, 0);
            
            lv_obj_t *power_outer = lv_obj_get_child(center_panel, 1);
            if (power_outer) lv_obj_set_style_border_color(power_outer, COLOR_BORDER_DAY, 0);
        }
        
        if (buttonPage.button_states[buttonPage.selected_index])
        {
            lv_obj_set_style_bg_color(buttonPage.main_power_btn, lv_color_make(235, 245, 251), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_ACCENT_BLUE_DAY, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_100, 0);
            lv_obj_set_style_shadow_color(buttonPage.main_power_btn, COLOR_ACCENT_BLUE_DAY, 0);
            lv_obj_set_style_shadow_opa(buttonPage.main_power_btn, LV_OPA_40, 0);
            
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);
            if (power_icon) lv_obj_set_style_text_color(power_icon, COLOR_ACCENT_BLUE_DAY, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(buttonPage. main_power_btn, lv_color_make(246, 248, 251), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_INACTIVE_GRAY_DAY, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
            lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 0, 0);
            
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage. main_power_btn, 0);
            if (power_icon) lv_obj_set_style_text_color(power_icon, COLOR_INACTIVE_GRAY_DAY, 0);
        }

        // Status label
        lv_obj_t *status_label = NULL;
        for (int i = 0; i < lv_obj_get_child_cnt(center_panel); i++)
        {
            lv_obj_t *child = lv_obj_get_child(center_panel, i);
            if (lv_obj_check_type(child, &lv_label_class))
            {
                const char *text = lv_label_get_text(child);
                if (text && (strcmp(text, "ACTIVE") == 0 || strcmp(text, "INACTIVE") == 0))
                {
                    status_label = child;
                    break;
                }
            }
        }

        if (status_label)
        {
            if (buttonPage.button_states[buttonPage.selected_index])
            {
                lv_obj_set_style_text_color(status_label, COLOR_ACCENT_BLUE_DAY, 0);
            }
            else
            {
                lv_obj_set_style_text_color(status_label, COLOR_TEXT_SECONDARY_DAY, 0);
            }
        }
    }
    else
    {
        // ========== NIGHT MODE ==========
        
        lv_obj_set_style_bg_color(buttonPage. page_container, COLOR_BG_MAIN_NIGHT, 0);
        
        // Header
        lv_obj_t *header = lv_obj_get_child(buttonPage.page_container, 0);
        if (header != NULL)
        {
            lv_obj_set_style_bg_color(header, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_bg_opa(header, LV_OPA_90, 0);
            lv_obj_set_style_text_color(buttonPage.header_icon, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_text_color(buttonPage.title_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
            
            lv_obj_t *subtitle = lv_obj_get_child(header, 2);
            if (subtitle) lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_set_style_bg_color(buttonPage.home_button, COLOR_BG_PANEL_NIGHT, 0);
            lv_obj_set_style_border_color(buttonPage.home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
            
            lv_obj_t *home_icon = lv_obj_get_child(buttonPage.home_button, 0);
            if (home_icon) lv_obj_set_style_text_color(home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
        }
        
        // Sidebar
        lv_obj_t *sidebar = lv_obj_get_child(buttonPage.page_container, 1);
        if (sidebar != NULL)
        {
            lv_obj_set_style_bg_color(sidebar, COLOR_SIDEBAR_BG_NIGHT, 0);
            lv_obj_set_style_bg_opa(sidebar, LV_OPA_80, 0);
            
            for (int i = 0; i < 6; i++)
            {
                if (buttonPage.sidebar_btns[i] != NULL)
                {
                    lv_obj_t *label = lv_obj_get_child(buttonPage.sidebar_btns[i], 0);
                    
                    if (i == buttonPage.selected_index)
                    {
                        lv_obj_set_style_bg_color(buttonPage. sidebar_btns[i], lv_color_make(20, 40, 60), 0);
                        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_70, 0);
                        lv_obj_set_style_border_color(buttonPage.sidebar_btns[i], COLOR_NEON_CYAN, 0);
                        lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[i], 20, 0);
                        lv_obj_set_style_shadow_color(buttonPage.sidebar_btns[i], COLOR_NEON_CYAN, 0);
                        lv_obj_set_style_shadow_opa(buttonPage.sidebar_btns[i], LV_OPA_70, 0);
                        if (label) lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY_NIGHT, 0);
                    }
                    else
                    {
                        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_TRANSP, 0);
                        lv_obj_set_style_border_color(buttonPage.sidebar_btns[i], lv_color_make(40, 60, 80), 0);
                        lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[i], 0, 0);
                        if (label) lv_obj_set_style_text_color(label, COLOR_TEXT_SECONDARY_NIGHT, 0);
                    }
                }
            }
            
            lv_obj_t *close_btn = lv_obj_get_child(sidebar, 6);
            if (close_btn)
            {
                lv_obj_set_style_bg_color(close_btn, lv_color_make(15, 25, 35), 0);
                lv_obj_set_style_border_color(close_btn, lv_color_make(60, 80, 100), 0);
                lv_obj_t *close_label = lv_obj_get_child(close_btn, 0);
                if (close_label) lv_obj_set_style_text_color(close_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
        }
        
        // Center panel
        lv_obj_t *center_panel = lv_obj_get_child(buttonPage. page_container, 2);
        if (center_panel != NULL)
        {
            lv_obj_set_style_bg_color(center_panel, COLOR_CARD_BG_NIGHT, 0);
            lv_obj_set_style_bg_opa(center_panel, LV_OPA_50, 0);
            lv_obj_set_style_text_color(buttonPage.main_device_name, COLOR_NEON_CYAN, 0);
            
            lv_obj_t *power_outer = lv_obj_get_child(center_panel, 1);
            if (power_outer) lv_obj_set_style_border_color(power_outer, lv_color_make(30, 50, 70), 0);
        }

        if (buttonPage.button_states[buttonPage.selected_index])
        {
            lv_obj_set_style_bg_color(buttonPage.main_power_btn, lv_color_make(15, 25, 35), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_100, 0);
            lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 30, 0);
            lv_obj_set_style_shadow_color(buttonPage.main_power_btn, COLOR_NEON_CYAN, 0);
            lv_obj_set_style_shadow_opa(buttonPage.main_power_btn, LV_OPA_90, 0);
            
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);
            if (power_icon) lv_obj_set_style_text_color(power_icon, COLOR_NEON_CYAN, 0);
        }
        else
        {
            lv_obj_set_style_bg_color(buttonPage.main_power_btn, lv_color_make(15, 25, 35), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_TEXT_SECONDARY_NIGHT, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
            lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 0, 0);
            
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);
            if (power_icon) lv_obj_set_style_text_color(power_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
        }

        // Status label
        lv_obj_t *status_label = NULL;
        for (int i = 0; i < lv_obj_get_child_cnt(center_panel); i++)
        {
            lv_obj_t *child = lv_obj_get_child(center_panel, i);
            if (lv_obj_check_type(child, &lv_label_class))
            {
                const char *text = lv_label_get_text(child);
                if (text && (strcmp(text, "ACTIVE") == 0 || strcmp(text, "INACTIVE") == 0))
                {
                    status_label = child;
                    break;
                }
            }
        }

        if (status_label)
        {
            if (buttonPage.button_states[buttonPage.selected_index])
            {
                lv_obj_set_style_text_color(status_label, COLOR_NEON_CYAN, 0);
            }
            else
            {
                lv_obj_set_style_text_color(status_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
        }
    }
    
    update_sidebar_indicators();
    ESP_LOGI(s_tag, "Theme colors updated to %s mode", is_day_mode ? "DAY" : "NIGHT");
}

// Main power button event callback
static void main_power_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int channel = buttonPage.selected_index;
        buttonPage.button_states[channel] = !buttonPage.button_states[channel];
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *icon = lv_obj_get_child(btn, 0);
        
        // Find status label
        lv_obj_t *center_panel = lv_obj_get_parent(lv_obj_get_parent(btn));
        lv_obj_t *status_label = NULL;
        
        for (int i = 0; i < lv_obj_get_child_cnt(center_panel); i++)
        {
            lv_obj_t *child = lv_obj_get_child(center_panel, i);
            if (lv_obj_check_type(child, &lv_label_class))
            {
                const char *text = lv_label_get_text(child);
                if (text && (strcmp(text, "ACTIVE") == 0 || strcmp(text, "INACTIVE") == 0))
                {
                    status_label = child;
                    break;
                }
            }
        }
        
        if (buttonPage.button_states[channel])
        {
            // Turn ON
            bool is_day_mode = !usrGraphicalInterface_isDarkMode();
            lv_color_t active_color = is_day_mode ? COLOR_ACCENT_BLUE_DAY : COLOR_NEON_CYAN;
            lv_color_t bg_color = is_day_mode ? lv_color_make(235, 245, 251) : lv_color_make(15, 25, 35);
            
            lv_obj_set_style_bg_color(btn, bg_color, 0);
            lv_obj_set_style_border_color(btn, active_color, 0);
            lv_obj_set_style_border_opa(btn, LV_OPA_100, 0);
            lv_obj_set_style_border_width(btn, 3, 0);
            lv_obj_set_style_text_color(icon, active_color, 0);
            lv_obj_set_style_shadow_width(btn, 30, 0);
            lv_obj_set_style_shadow_color(btn, active_color, 0);
            lv_obj_set_style_shadow_opa(btn, is_day_mode ? LV_OPA_40 : LV_OPA_90, 0);
            
            if (status_label)
            {
                lv_label_set_text(status_label, "ACTIVE");
                lv_obj_set_style_text_color(status_label, active_color, 0);
            }
        }
        else
        {
            // Turn OFF
            bool is_day_mode = !usrGraphicalInterface_isDarkMode();
            lv_color_t inactive_color = is_day_mode ? COLOR_INACTIVE_GRAY_DAY : COLOR_TEXT_SECONDARY_NIGHT;
            lv_color_t bg_color = is_day_mode ? lv_color_make(246, 248, 251) : lv_color_make(15, 25, 35);
            
            lv_obj_set_style_bg_color(btn, bg_color, 0);
            lv_obj_set_style_border_color(btn, inactive_color, 0);
            lv_obj_set_style_border_opa(btn, LV_OPA_30, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_set_style_text_color(icon, inactive_color, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            
            if (status_label)
            {
                lv_label_set_text(status_label, "INACTIVE");
                lv_obj_set_style_text_color(status_label, inactive_color, 0);
            }
        }
        
        // CAN bus communication - RELAY control (ORIGINAL LOGIC)
        uint32_t relay_node_id = getRelayNodeID();
        
        if (relay_node_id == 0)
        {
            ESP_LOGE(s_tag, "❌ Relay kartı bulunamadı!");
            usrSocketsPage_setStatus("Relay kartı bağlı değil");
            return;
        }
        
        uint8_t relay_cmd[8] = {0};
        relay_cmd[0] = RELAY_CMD_SET;
        relay_cmd[1] = channel + 1;
        relay_cmd[2] = buttonPage.button_states[channel] ? 1 : 0;
        
        esp_err_t err = sendCanMessage(relay_node_id, relay_cmd, 3);
        if (err == ESP_OK) {
            ESP_LOGI(s_tag, "✓ RELAY%d SET sent to 0x%03lX:  State=%d", 
                     channel+1, relay_node_id, buttonPage.button_states[channel]);
        } else {
            ESP_LOGE(s_tag, "✗ RELAY%d SET FAILED", channel+1);
            return;
        }
        
        update_sidebar_indicators();
        
        ESP_LOGI(s_tag, "Power button toggled - Channel %d:  %s", 
                 channel, buttonPage.button_states[channel] ? "ON" : "OFF");
    }
}

// Sidebar button click event
static void sidebar_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        
        int clicked_index = -1;
        for (int i = 0; i < 6; i++)
        {
            if (buttonPage.sidebar_btns[i] == btn)
            {
                clicked_index = i;
                break;
            }
        }
        
        if (clicked_index >= 0 && clicked_index != buttonPage.selected_index)
        {
            bool is_day_mode = !usrGraphicalInterface_isDarkMode();
            // Deselect old button
            lv_obj_t *old_label = lv_obj_get_child(buttonPage.sidebar_btns[buttonPage.selected_index], 0);
            
            lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[buttonPage.selected_index], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(buttonPage.sidebar_btns[buttonPage.selected_index], 2, 0);
            
            if (is_day_mode)
            {
                lv_obj_set_style_border_color(buttonPage.sidebar_btns[buttonPage.selected_index], COLOR_BORDER_DAY, 0);
                lv_obj_set_style_border_opa(buttonPage.sidebar_btns[buttonPage.selected_index], LV_OPA_40, 0);
                if (old_label) lv_obj_set_style_text_color(old_label, COLOR_TEXT_SECONDARY_DAY, 0);
            }
            else
            {
                lv_obj_set_style_border_color(buttonPage. sidebar_btns[buttonPage. selected_index], lv_color_make(40, 60, 80), 0);
                lv_obj_set_style_border_opa(buttonPage.sidebar_btns[buttonPage.selected_index], LV_OPA_40, 0);
                if (old_label) lv_obj_set_style_text_color(old_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
            lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[buttonPage.selected_index], 0, 0);
            
            // Select new button
            buttonPage.selected_index = clicked_index;
            lv_obj_t *new_label = lv_obj_get_child(btn, 0);
            
            if (is_day_mode)
            {
                lv_obj_set_style_bg_color(btn, lv_color_make(225, 236, 244), 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_100, 0);
                lv_obj_set_style_border_width(btn, 3, 0);
                lv_obj_set_style_border_color(btn, COLOR_ACCENT_BLUE_DAY, 0);
                lv_obj_set_style_border_opa(btn, LV_OPA_100, 0);
                lv_obj_set_style_shadow_width(btn, 10, 0);
                lv_obj_set_style_shadow_color(btn, COLOR_ACCENT_BLUE_DAY, 0);
                lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
                if (new_label) lv_obj_set_style_text_color(new_label, COLOR_ACCENT_BLUE_DAY, 0);
            }
            else
            {
                lv_obj_set_style_bg_color(btn, lv_color_make(20, 40, 60), 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_70, 0);
                lv_obj_set_style_border_width(btn, 3, 0);
                lv_obj_set_style_border_color(btn, COLOR_NEON_CYAN, 0);
                lv_obj_set_style_border_opa(btn, LV_OPA_100, 0);
                lv_obj_set_style_shadow_width(btn, 20, 0);
                lv_obj_set_style_shadow_color(btn, COLOR_NEON_CYAN, 0);
                lv_obj_set_style_shadow_opa(btn, LV_OPA_70, 0);
                if (new_label) lv_obj_set_style_text_color(new_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
            }
            
            // Update center panel
            lv_label_set_text(buttonPage. main_device_name, get_button_name(clicked_index));
            
            // Update power button state
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);
            lv_obj_t *center_panel = lv_obj_get_parent(lv_obj_get_parent(buttonPage. main_power_btn));
            lv_obj_t *status_label = NULL;
            
            for (int i = 0; i < lv_obj_get_child_cnt(center_panel); i++)
            {
                lv_obj_t *child = lv_obj_get_child(center_panel, i);
                if (lv_obj_check_type(child, &lv_label_class))
                {
                    const char *text = lv_label_get_text(child);
                    if (text && (strcmp(text, "ACTIVE") == 0 || strcmp(text, "INACTIVE") == 0))
                    {
                        status_label = child;
                        break;
                    }
                }
            }
            
            if (buttonPage.button_states[clicked_index])
            {
                // Socket ON
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                lv_color_t active_color = is_day_mode ? COLOR_ACCENT_BLUE_DAY : COLOR_NEON_CYAN;
                lv_color_t bg_color = is_day_mode ? lv_color_make(235, 245, 251) : lv_color_make(15, 25, 35);
                
                lv_obj_set_style_bg_color(buttonPage.main_power_btn, bg_color, 0);
                lv_obj_set_style_border_color(buttonPage.main_power_btn, active_color, 0);
                lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_100, 0);
                lv_obj_set_style_border_width(buttonPage.main_power_btn, 3, 0);
                lv_obj_set_style_text_color(power_icon, active_color, 0);
                lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 30, 0);
                lv_obj_set_style_shadow_color(buttonPage.main_power_btn, active_color, 0);
                lv_obj_set_style_shadow_opa(buttonPage.main_power_btn, is_day_mode ? LV_OPA_40 : LV_OPA_90, 0);
                
                if (status_label)
                {
                    lv_label_set_text(status_label, "ACTIVE");
                    lv_obj_set_style_text_color(status_label, active_color, 0);
                }
            }
            else
            {
                // Socket OFF
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                lv_color_t inactive_color = is_day_mode ?  COLOR_INACTIVE_GRAY_DAY : COLOR_TEXT_SECONDARY_NIGHT;
                lv_color_t bg_color = is_day_mode ? lv_color_make(246, 248, 251) : lv_color_make(15, 25, 35);
                
                lv_obj_set_style_bg_color(buttonPage.main_power_btn, bg_color, 0);
                lv_obj_set_style_border_color(buttonPage.main_power_btn, inactive_color, 0);
                lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
                lv_obj_set_style_border_width(buttonPage.main_power_btn, 2, 0);
                lv_obj_set_style_text_color(power_icon, inactive_color, 0);
                lv_obj_set_style_shadow_width(buttonPage. main_power_btn, 0, 0);
                
                if (status_label)
                {
                    lv_label_set_text(status_label, "INACTIVE");
                    lv_obj_set_style_text_color(status_label, inactive_color, 0);
                }
            }
            
            ESP_LOGI(s_tag, "Selected Socket: %s (index %d)", 
                     get_button_name(clicked_index), clicked_index);
        }
    }
}

// Close All button callback
static void close_all_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Close All button clicked - turning off all relays");
        
        uint32_t relay_node_id = getRelayNodeID();
        
        if (relay_node_id == 0)
        {
            ESP_LOGE(s_tag, "❌ Relay kartı bulunamadı!");
            usrSocketsPage_setStatus("Relay kartı bağlı değil");
            return;
        }
        
        // Turn off all relays
        for (int i = 0; i < 6; i++)
        {
            if (buttonPage.button_states[i])
            {
                buttonPage.button_states[i] = false;
                
                uint8_t relay_cmd[8] = {0};
                relay_cmd[0] = RELAY_CMD_SET;
                relay_cmd[1] = i + 1;
                relay_cmd[2] = 0;  // OFF
                
                esp_err_t err = sendCanMessage(relay_node_id, relay_cmd, 3);
                if (err == ESP_OK) {
                    ESP_LOGI(s_tag, "✓ RELAY%d SET=OFF sent", i+1);
                } else {
                    ESP_LOGE(s_tag, "✗ RELAY%d SET FAILED", i+1);
                }
            }
        }
        
        // UI update
        int channel = buttonPage.selected_index;
        lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);

        bool is_day_mode = !usrGraphicalInterface_isDarkMode();
        
        lv_color_t inactive_color = is_day_mode ? COLOR_INACTIVE_GRAY_DAY : COLOR_TEXT_SECONDARY_NIGHT;
        lv_color_t bg_color = is_day_mode ?  lv_color_make(246, 248, 251) : lv_color_make(15, 25, 35);
        
        lv_obj_set_style_bg_color(buttonPage.main_power_btn, bg_color, 0);
        lv_obj_set_style_border_color(buttonPage.main_power_btn, inactive_color, 0);
        lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
        lv_obj_set_style_border_width(buttonPage.main_power_btn, 2, 0);
        lv_obj_set_style_text_color(power_icon, inactive_color, 0);
        lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 0, 0);
        
        // Update status label
        lv_obj_t *center_panel = lv_obj_get_parent(lv_obj_get_parent(buttonPage. main_power_btn));
        for (int i = 0; i < lv_obj_get_child_cnt(center_panel); i++)
        {
            lv_obj_t *child = lv_obj_get_child(center_panel, i);
            if (lv_obj_check_type(child, &lv_label_class))
            {
                const char *text = lv_label_get_text(child);
                if (text && (strcmp(text, "ACTIVE") == 0 || strcmp(text, "INACTIVE") == 0))
                {
                    lv_label_set_text(child, "INACTIVE");
                    lv_obj_set_style_text_color(child, inactive_color, 0);
                    break;
                }
            }
        }
        
        update_sidebar_indicators();
        
        ESP_LOGI(s_tag, "All relays turned OFF instantly");
    }
}

void usrSocketsPage_init(lv_obj_t *parent)
{
    if (buttonPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Sockets page already initialized");
        return;
    }


    // Main container
    buttonPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(buttonPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(buttonPage.page_container, COLOR_BG_MAIN_NIGHT, 0);
    lv_obj_set_style_border_width(buttonPage.page_container, 0, 0);
    lv_obj_clear_flag(buttonPage.page_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(buttonPage.page_container, 0, 0);
    lv_obj_set_style_pad_all(buttonPage.page_container, 0, 0);

    // ==================== HEADER ====================
    lv_obj_t *header = lv_obj_create(buttonPage.page_container);
    lv_obj_set_size(header, LV_HOR_RES, 70);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(header, lv_color_make(148, 163, 184), 0);
    lv_obj_set_style_border_opa(header, LV_OPA_20, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Power plug icon
    buttonPage.header_icon = lv_label_create(header);
    lv_label_set_text(buttonPage.header_icon, LV_SYMBOL_USB);
    lv_obj_set_style_text_font(buttonPage.header_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(buttonPage.header_icon, COLOR_NEON_CYAN, 0);
    lv_obj_align(buttonPage.header_icon, LV_ALIGN_LEFT_MID, 15, 0);

    // Title
    buttonPage.title_label = lv_label_create(header);
    lv_label_set_text(buttonPage. title_label, "SOCKETS");
    lv_obj_set_style_text_font(buttonPage.title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(buttonPage.title_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(buttonPage.title_label, LV_ALIGN_LEFT_MID, 55, -6);

    // Subtitle
    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, "RELAY CONTROL");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(subtitle, 1, 0);
    lv_obj_align(subtitle, LV_ALIGN_LEFT_MID, 55, 8);

    // Home button
    buttonPage.home_button = lv_btn_create(header);
    lv_obj_set_size(buttonPage.home_button, 40, 40);
    lv_obj_align(buttonPage.home_button, LV_ALIGN_RIGHT_MID, -45, 0);
    lv_obj_set_style_bg_color(buttonPage.home_button, COLOR_BG_PANEL_NIGHT, 0);
    lv_obj_set_style_bg_opa(buttonPage.home_button, LV_OPA_50, 0);
    lv_obj_set_style_border_width(buttonPage.home_button, 1, 0);
    lv_obj_set_style_border_color(buttonPage.home_button, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(buttonPage.home_button, LV_OPA_30, 0);
    lv_obj_set_style_radius(buttonPage.home_button, 8, 0);
    
    lv_obj_t *home_icon = lv_label_create(buttonPage.home_button);
    lv_label_set_text(home_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(home_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(home_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(home_icon);
    
    lv_obj_add_event_cb(buttonPage.home_button, home_button_event_cb, LV_EVENT_CLICKED, NULL);

    // ==================== LEFT SIDEBAR ====================
    lv_obj_t *sidebar = lv_obj_create(buttonPage.page_container);
    lv_obj_set_size(sidebar, 230, LV_VER_RES - 70);
    lv_obj_align(sidebar, LV_ALIGN_TOP_LEFT, 0, 70);
    lv_obj_set_style_bg_color(sidebar, COLOR_SIDEBAR_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(sidebar, 0, 0);
    lv_obj_set_style_radius(sidebar, 0, 0);
    lv_obj_set_style_pad_all(sidebar, 20, 0);
    lv_obj_set_style_pad_row(sidebar, 12, 0);
    lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    // Create Socket 1 to Socket 6 buttons
    for (int i = 0; i < 6; i++)
    {
        buttonPage.sidebar_btns[i] = lv_obj_create(sidebar);
        lv_obj_set_size(buttonPage.sidebar_btns[i], 200, 42);
        lv_obj_set_pos(buttonPage.sidebar_btns[i], 0, i * 50 + 10);
        
        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(buttonPage. sidebar_btns[i], 2, 0);
        lv_obj_set_style_border_color(buttonPage.sidebar_btns[i], lv_color_make(40, 60, 80), 0);
        lv_obj_set_style_border_opa(buttonPage.sidebar_btns[i], LV_OPA_40, 0);
        lv_obj_set_style_radius(buttonPage.sidebar_btns[i], 22, 0);
        lv_obj_clear_flag(buttonPage.sidebar_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(buttonPage.sidebar_btns[i], LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_t *label = lv_label_create(buttonPage.sidebar_btns[i]);
        lv_label_set_text(label, get_button_name(i));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, COLOR_TEXT_SECONDARY_NIGHT, 0);
        lv_obj_center(label);
        
        lv_obj_add_event_cb(buttonPage.sidebar_btns[i], sidebar_btn_cb, LV_EVENT_CLICKED, NULL);
    }
    
    // Select Socket 1 by default
    buttonPage.selected_index = 0;
    lv_obj_set_style_bg_color(buttonPage. sidebar_btns[0], lv_color_make(20, 40, 60), 0);
    lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[0], LV_OPA_70, 0);
    lv_obj_set_style_border_width(buttonPage.sidebar_btns[0], 3, 0);
    lv_obj_set_style_border_color(buttonPage. sidebar_btns[0], COLOR_NEON_CYAN, 0);
    lv_obj_set_style_border_opa(buttonPage.sidebar_btns[0], LV_OPA_100, 0);
    lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[0], 20, 0);
    lv_obj_set_style_shadow_color(buttonPage.sidebar_btns[0], COLOR_NEON_CYAN, 0);
    lv_obj_set_style_shadow_opa(buttonPage.sidebar_btns[0], LV_OPA_70, 0);
    
    lv_obj_t *selected_label = lv_obj_get_child(buttonPage.sidebar_btns[0], 0);
    lv_obj_set_style_text_color(selected_label, COLOR_TEXT_PRIMARY_NIGHT, 0);

    // Close All button
    lv_obj_t *close_all_btn = lv_btn_create(sidebar);
    lv_obj_set_size(close_all_btn, 140, 42);
    lv_obj_align(close_all_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(close_all_btn, lv_color_make(15, 25, 35), 0);
    lv_obj_set_style_bg_opa(close_all_btn, LV_OPA_60, 0);
    lv_obj_set_style_border_width(close_all_btn, 2, 0);
    lv_obj_set_style_border_color(close_all_btn, lv_color_make(60, 80, 100), 0);
    lv_obj_set_style_border_opa(close_all_btn, LV_OPA_50, 0);
    lv_obj_set_style_radius(close_all_btn, 22, 0);
    
    lv_obj_t *close_label = lv_label_create(close_all_btn);
    lv_label_set_text(close_label, "Close All");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(close_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(close_label);
    
    lv_obj_add_event_cb(close_all_btn, close_all_btn_cb, LV_EVENT_CLICKED, NULL);

    // ==================== CENTER PANEL ====================
    lv_obj_t *center_panel = lv_obj_create(buttonPage.page_container);
    lv_obj_set_size(center_panel, 570, LV_VER_RES - 70);
    lv_obj_align(center_panel, LV_ALIGN_TOP_LEFT, 230, 70);
    lv_obj_set_style_bg_color(center_panel, COLOR_CARD_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(center_panel, LV_OPA_50, 0);
    lv_obj_set_style_border_width(center_panel, 0, 0);
    lv_obj_set_style_radius(center_panel, 0, 0);
    lv_obj_set_style_pad_all(center_panel, 20, 0);
    lv_obj_clear_flag(center_panel, LV_OBJ_FLAG_SCROLLABLE);

    // "SOCKET NAME" label
    buttonPage.main_device_name = lv_label_create(center_panel);
    lv_label_set_text(buttonPage.main_device_name, get_button_name(0));
    lv_obj_set_style_text_font(buttonPage.main_device_name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(buttonPage.main_device_name, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_text_letter_space(buttonPage.main_device_name, 2, 0);
    lv_obj_align(buttonPage.main_device_name, LV_ALIGN_TOP_MID, 0, 30);

    // Power button outer ring
    lv_obj_t *power_outer_ring = lv_obj_create(center_panel);
    lv_obj_set_size(power_outer_ring, 160, 160);
    lv_obj_align(power_outer_ring, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(power_outer_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(power_outer_ring, 2, 0);
    lv_obj_set_style_border_color(power_outer_ring, lv_color_make(30, 50, 70), 0);
    lv_obj_set_style_border_opa(power_outer_ring, LV_OPA_50, 0);
    lv_obj_set_style_radius(power_outer_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(power_outer_ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // Power button
    buttonPage.main_power_btn = lv_btn_create(power_outer_ring);
    lv_obj_set_size(buttonPage.main_power_btn, 130, 130);
    lv_obj_center(buttonPage.main_power_btn);
    lv_obj_set_style_bg_color(buttonPage.main_power_btn, lv_color_make(15, 25, 35), 0);
    lv_obj_set_style_bg_opa(buttonPage.main_power_btn, LV_OPA_80, 0);
    lv_obj_set_style_border_width(buttonPage.main_power_btn, 2, 0);
    lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
    lv_obj_set_style_radius(buttonPage.main_power_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 0, 0);
    
    lv_obj_t *power_icon = lv_label_create(buttonPage.main_power_btn);
    lv_label_set_text(power_icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(power_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(power_icon, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_center(power_icon);
    
    lv_obj_add_event_cb(buttonPage.main_power_btn, main_power_btn_cb, LV_EVENT_CLICKED, NULL);

    // Status label ("ACTIVE" / "INACTIVE")
    lv_obj_t *status_label = lv_label_create(center_panel);
    lv_label_set_text(status_label, "INACTIVE");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(status_label, 2, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 95);

    
    // Initialize states
    for (int i = 0; i < 6; i++)
    {
        buttonPage. button_states[i] = false;
        buttonPage.slider_values[i] = 0;
    }

    usrSocketsPage_hide();

    update_sidebar_indicators();

    ESP_LOGI(s_tag, "Sockets page initialized with new design");
}

void usrSocketsPage_show(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_clear_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = true;
        usrSocketsPage_updateButtonNames();
        ESP_LOGI(s_tag, "Sockets page shown");
    }
}

void usrSocketsPage_updateButtonNames(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (buttonPage.sidebar_btns[i] != NULL)
        {
            lv_obj_t *label = lv_obj_get_child(buttonPage.sidebar_btns[i], 0);
            if (label != NULL)
            {
                lv_label_set_text(label, get_button_name(i));
            }
        }
    }
    
    if (buttonPage.main_device_name != NULL)
    {
        lv_label_set_text(buttonPage. main_device_name, get_button_name(buttonPage.selected_index));
    }
    
    ESP_LOGI(s_tag, "Button names updated");
}

void usrSocketsPage_hide(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_add_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = false;
        ESP_LOGI(s_tag, "Sockets page hidden");
    }
}

void usrSocketsPage_destroy(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_del(buttonPage.page_container);
        memset(&buttonPage, 0, sizeof(usrSocketsPage_t));
        ESP_LOGI(s_tag, "Sockets page destroyed");
    }
}

void usrSocketsPage_setStatus(const char *status)
{
    ESP_LOGI(s_tag, "Status:  %s", status ?  status : "NULL");
}

bool usrSocketsPage_isActive(void)
{
    return buttonPage.is_active;
}

// Button handlers - ORIGINAL CAN BUS FUNCTIONALITY PRESERVED
void usrSocketsPage_button1_handler(void)
{
    ESP_LOGI(s_tag, "Socket-1 handler called");

    buttonPage.button_states[0] = ! buttonPage.button_states[0];
    bool state = buttonPage.button_states[0];

    uint32_t relay_node_id = getRelayNodeID();
    
    if (relay_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ Relay kartı bulunamadı!");
        usrSocketsPage_setStatus("Relay kartı bağlı değil");
        return;
    }

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 1;
    relay_cmd[2] = state ? 1 : 0;
    
    esp_err_t err = sendCanMessage(relay_node_id, relay_cmd, 3);
    
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ Relay1 command sent to 0x%03lX:  %s", 
                 relay_node_id, state ? "ON" : "OFF");
    } else {
        ESP_LOGE(s_tag, "✗ Relay1 command FAILED");
    }
    update_sidebar_indicators();
}

void usrSocketsPage_button2_handler(void)
{
    ESP_LOGI(s_tag, "Socket-2 handler called");
    buttonPage.button_states[1] = !buttonPage.button_states[1];
    bool state = buttonPage.button_states[1];

    uint32_t relay_node_id = getRelayNodeID();
    if (relay_node_id == 0) {
        ESP_LOGE(s_tag, "❌ Relay kartı bulunamadı!");
        return;
    }

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 2;
    relay_cmd[2] = state ? 1 :  0;

    sendCanMessage(relay_node_id, relay_cmd, 3);
    ESP_LOGI(s_tag, "✓ Relay2 → 0x%03lX", relay_node_id);
    update_sidebar_indicators();
}

void usrSocketsPage_button3_handler(void)
{
    ESP_LOGI(s_tag, "Socket-3 handler called");
    buttonPage.button_states[2] = !buttonPage.button_states[2];
    bool state = buttonPage.button_states[2];

    uint32_t relay_node_id = getRelayNodeID();
    if (relay_node_id == 0) return;

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 3;
    relay_cmd[2] = state ?  1 : 0;

    sendCanMessage(relay_node_id, relay_cmd, 3);
    update_sidebar_indicators();
}

void usrSocketsPage_button4_handler(void)
{
    ESP_LOGI(s_tag, "Socket-4 handler called");
    buttonPage.button_states[3] = !buttonPage.button_states[3];
    bool state = buttonPage.button_states[3];

    uint32_t relay_node_id = getRelayNodeID();
    if (relay_node_id == 0) return;

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 4;
    relay_cmd[2] = state ? 1 : 0;

    sendCanMessage(relay_node_id, relay_cmd, 3);
    update_sidebar_indicators();
}

void usrSocketsPage_button5_handler(void)
{
    ESP_LOGI(s_tag, "Socket-5 handler called");
    buttonPage.button_states[4] = !buttonPage.button_states[4];
    bool state = buttonPage. button_states[4];

    uint32_t relay_node_id = getRelayNodeID();
    if (relay_node_id == 0) return;

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 5;
    relay_cmd[2] = state ? 1 : 0;

    sendCanMessage(relay_node_id, relay_cmd, 3);
    update_sidebar_indicators();
}

void usrSocketsPage_button6_handler(void)
{
    ESP_LOGI(s_tag, "Socket-6 handler called");
    buttonPage.button_states[5] = !buttonPage.button_states[5];
    bool state = buttonPage.button_states[5];

    uint32_t relay_node_id = getRelayNodeID();
    if (relay_node_id == 0) return;

    uint8_t relay_cmd[8] = {0};
    relay_cmd[0] = RELAY_CMD_SET;
    relay_cmd[1] = 6;
    relay_cmd[2] = state ? 1 : 0;

    sendCanMessage(relay_node_id, relay_cmd, 3);
    update_sidebar_indicators();
}

bool usrSocketsPage_getButtonState(int channel)
{
    if (channel >= 0 && channel < 6)
        return buttonPage.button_states[channel];
    return false;
}

uint32_t usrSocketsPage_getSliderValue(int channel)
{
    if (channel >= 0 && channel < 6)
        return buttonPage.slider_values[channel];
    return 0;
}