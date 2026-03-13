#include "usrLightingPage.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "usrSettingsPage.h"
#include "usrCanDynamicIDMaster.h"
#include "usrGraphicalInterface.h"

static const char *s_tag = "usrLightingPage";
static usrLightingPage_t buttonPage = {0};

// Global slider position storage
static uint32_t sliderPos[6] = {0, 0, 0, 0, 0, 0};

static lv_obj_t *active_segments[10];
static lv_obj_t *bg_segments[10];


// LED komut tanımları
#define LED_CMD_TOGGLE          0x10
#define LED_CMD_SET             0x11
#define LED_CMD_SET_BRIGHTNESS  0x12

// NVS ayarları
#define NVS_NAMESPACE_BRIGHTNESS "brightness"
#define NVS_KEY_PREFIX "led_br_"
#define DEFAULT_BRIGHTNESS 500

static nvs_handle_t brightness_nvs_handle = 0;
static bool brightness_nvs_opened = false;

// Dinamik buton ismi alma fonksiyonu
static const char* get_button_name(int index)
{
    return usrSettingsPage_getButtonName(1, index); // 1 = Lighting section
}

// TEK BİR getLEDNodeID() FONKSIYONU
static uint32_t getLEDNodeID(void)
{
    uint8_t selected_room = usrGraphicalInterface_getSelectedRoom();
    
    if (selected_room == 0) {
        ESP_LOGW(s_tag, "No room selected!");
        return 0;
    }
    
    // Room aktif mi kontrol et
    if (!isRoomActive(selected_room)) {
        ESP_LOGW(s_tag, "Room %d is not active!", selected_room);
        return 0;
    }
    
    ESP_LOGD(s_tag, "Target room: %d", selected_room);
    
    // Room ID'yi return et (komut gönderirken kullanılacak)
    return selected_room;
}
// NVS fonksiyonları
static esp_err_t open_brightness_nvs(void)
{
    if (brightness_nvs_opened) return ESP_OK;
    
    esp_err_t err = nvs_open(NVS_NAMESPACE_BRIGHTNESS, NVS_READWRITE, &brightness_nvs_handle);
    if (err == ESP_OK) {
        brightness_nvs_opened = true;
        ESP_LOGI(s_tag, "Brightness NVS opened successfully");
    } else {
        ESP_LOGE(s_tag, "Failed to open brightness NVS:  %s", esp_err_to_name(err));
    }
    return err;
}

static void saveBrightnessToNVS(int channel, uint32_t value)
{
    if (open_brightness_nvs() != ESP_OK) return;
    
    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, channel);
    
    esp_err_t err = nvs_set_u32(brightness_nvs_handle, key, value);
    if (err == ESP_OK) {
        nvs_commit(brightness_nvs_handle);
        ESP_LOGI(s_tag, "Brightness saved to NVS:  LED%d = %lu", channel+1, value);
    } else {
        ESP_LOGE(s_tag, "Failed to save brightness to NVS: %s", esp_err_to_name(err));
    }
}

static uint32_t loadBrightnessFromNVS(int channel)
{
    if (open_brightness_nvs() != ESP_OK) return DEFAULT_BRIGHTNESS;
    
    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, channel);
    
    uint32_t value = DEFAULT_BRIGHTNESS;
    esp_err_t err = nvs_get_u32(brightness_nvs_handle, key, &value);
    
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "Brightness loaded from NVS: LED%d = %lu", channel+1, value);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(s_tag, "No saved brightness for LED%d, using default %d", channel+1, DEFAULT_BRIGHTNESS);
        value = DEFAULT_BRIGHTNESS;
    } else {
        ESP_LOGE(s_tag, "Error loading brightness:  %s", esp_err_to_name(err));
        value = DEFAULT_BRIGHTNESS;
    }
    
    return value;
}

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
            // İkinci child gösterge olacak (0:  label, 1: indicator)
            lv_obj_t *indicator = NULL;
            
            // Eğer indicator zaten varsa bul, yoksa oluştur
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
            
            // İkonu bul
            lv_obj_t *icon = lv_obj_get_child(indicator, 0);
            
            // Durum kontrolü
            if (buttonPage.button_states[i])
            {
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                // AÇIK - Ampul görünür ve parlak
                lv_obj_clear_flag(indicator, LV_OBJ_FLAG_HIDDEN);
                
                if (is_day_mode)
                {
                    // DAY MODE - Mavi tonlarda
                    lv_obj_set_style_bg_color(indicator, lv_color_make(225, 242, 252), 0);
                    lv_obj_set_style_bg_opa(indicator, LV_OPA_100, 0);
                    lv_obj_set_style_shadow_width(indicator, 8, 0);
                    lv_obj_set_style_shadow_color(indicator, COLOR_ACCENT_BLUE_DAY, 0);
                    lv_obj_set_style_shadow_opa(indicator, LV_OPA_50, 0);
                    
                    if (icon) lv_obj_set_style_text_color(icon, COLOR_ACCENT_BLUE_DAY, 0);
                }
                else
                {
                    // NIGHT MODE - Cyan glow
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
                // KAPALI - Ampul gizli
                lv_obj_add_flag(indicator, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}


// Update slider visual representation
static void update_slider_visual(void)
{
    int channel = buttonPage.selected_index;
    uint32_t slider_percent = sliderPos[channel] / 10;
    if (slider_percent > 100) slider_percent = 100;
    
    // Update percentage label
    char percent_text[16];
    snprintf(percent_text, sizeof(percent_text), "%lu%%", (unsigned long)slider_percent);
    lv_label_set_text(buttonPage.main_status_label, percent_text);
    
    // Update slider value
    lv_slider_set_value(buttonPage. main_slider, (int32_t)slider_percent, LV_ANIM_OFF);
    
    // Segmentleri güncelle - KISMÎ DOLULUK DESTEĞİ
    int full_segments = slider_percent / 10;
    int partial_percent = slider_percent % 10;
    
    for (int i = 0; i < 10; i++)
    {
        if (active_segments[i] != NULL)
        {
            if (i < full_segments)
            {
                // Tamamen dolu segment
                lv_obj_clear_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_bg_opa(active_segments[i], LV_OPA_100, 0);
                lv_obj_set_style_shadow_opa(active_segments[i], LV_OPA_70, 0);
            }
            else if (i == full_segments && partial_percent > 0)
            {
                // Kısmen dolu segment
                lv_obj_clear_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                lv_opa_t opacity = (lv_opa_t)((partial_percent * 255) / 10);
                if (opacity < 30) opacity = 30;
                lv_obj_set_style_bg_opa(active_segments[i], opacity, 0);
                lv_obj_set_style_shadow_opa(active_segments[i], opacity * 70 / 100, 0);
            }
            else
            {
                // Boş segment
                lv_obj_add_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

// Update theme colors based on day/night mode
void usrLightingPage_updateThemeColors(void)
{
    if (buttonPage.page_container == NULL) return;
    bool is_day_mode = !usrGraphicalInterface_isDarkMode();
    
    if (is_day_mode)
    {
        // ========== DAY MODE ==========
        
        // Main container
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
                if (buttonPage. sidebar_btns[i] != NULL)
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
                        lv_obj_set_style_border_color(buttonPage. sidebar_btns[i], COLOR_BORDER_DAY, 0);
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
            // Aktif durum
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
            // Pasif durum
            lv_obj_set_style_bg_color(buttonPage. main_power_btn, lv_color_make(246, 248, 251), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_INACTIVE_GRAY_DAY, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
            lv_obj_set_style_shadow_width(buttonPage.main_power_btn, 0, 0);
            
            lv_obj_t *power_icon = lv_obj_get_child(buttonPage.main_power_btn, 0);
            if (power_icon) lv_obj_set_style_text_color(power_icon, COLOR_INACTIVE_GRAY_DAY, 0);
        }

        // Status label güncelle
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
        
        // Right panel
        lv_obj_t *right_panel = lv_obj_get_child(buttonPage.page_container, 3);
        if (right_panel != NULL)
        {
            lv_obj_set_style_bg_color(right_panel, lv_color_make(241, 246, 250), 0);
            
            // Yüzde yazısı
            lv_obj_set_style_text_color(buttonPage.main_status_label, COLOR_ACCENT_BLUE_DAY, 0);
            
            // Arka plan segmentleri
            for (int i = 0; i < 10; i++)
            {
                if (bg_segments[i] != NULL)
                {
                    lv_obj_set_style_bg_color(bg_segments[i], COLOR_INACTIVE_GRAY_DAY, 0);
                    lv_obj_set_style_border_color(bg_segments[i], lv_color_make(200, 205, 210), 0);
                    lv_obj_set_style_border_opa(bg_segments[i], LV_OPA_30, 0);
                }
            }
            
            // Aktif segmentler
            for (int i = 0; i < 10; i++)
            {
                if (active_segments[i] != NULL)
                {
                    lv_obj_set_style_bg_color(active_segments[i], COLOR_ACCENT_BLUE_DAY, 0);
                    lv_obj_set_style_shadow_color(active_segments[i], COLOR_ACCENT_BLUE_DAY, 0);
                    lv_obj_set_style_shadow_opa(active_segments[i], LV_OPA_40, 0);
                }
            }
        }
    }
    else
    {
        // ========== NIGHT MODE ==========
        
        // Main container
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
        lv_obj_t *center_panel = lv_obj_get_child(buttonPage.page_container, 2);
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
            // Aktif durum
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
            // Pasif durum
            lv_obj_set_style_bg_color(buttonPage. main_power_btn, lv_color_make(15, 25, 35), 0);
            lv_obj_set_style_border_color(buttonPage.main_power_btn, COLOR_TEXT_SECONDARY_NIGHT, 0);
            lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
            lv_obj_set_style_shadow_width(buttonPage. main_power_btn, 0, 0);
            
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
            if (buttonPage. button_states[buttonPage.selected_index])
            {
                lv_obj_set_style_text_color(status_label, COLOR_NEON_CYAN, 0);
            }
            else
            {
                lv_obj_set_style_text_color(status_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
            }
        }
        
        // Right panel
        lv_obj_t *right_panel = lv_obj_get_child(buttonPage.page_container, 3);
        if (right_panel != NULL)
        {
            lv_obj_set_style_bg_color(right_panel, COLOR_CARD_BG_NIGHT, 0);
            lv_obj_set_style_bg_opa(right_panel, LV_OPA_50, 0);
            lv_obj_set_style_text_color(buttonPage.main_status_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
            
            // Arka plan segmentleri
            for (int i = 0; i < 10; i++)
            {
                if (bg_segments[i] != NULL)
                {
                    lv_obj_set_style_bg_color(bg_segments[i], lv_color_make(15, 25, 35), 0);
                    lv_obj_set_style_border_color(bg_segments[i], lv_color_make(20, 35, 50), 0);
                    lv_obj_set_style_border_opa(bg_segments[i], LV_OPA_40, 0);
                }
            }
            
            // Aktif segmentler
            for (int i = 0; i < 10; i++)
            {
                if (active_segments[i] != NULL)
                {
                    lv_obj_set_style_bg_color(active_segments[i], COLOR_NEON_CYAN, 0);
                    lv_obj_set_style_shadow_color(active_segments[i], COLOR_NEON_CYAN, 0);
                    lv_obj_set_style_shadow_opa(active_segments[i], LV_OPA_70, 0);
                }
            }
        }
    }
    
    update_sidebar_indicators();
    ESP_LOGI(s_tag, "Theme colors updated to %s mode", is_day_mode ? "DAY" : "NIGHT");
}

// Main slider event callback
static void main_slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *slider = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        int channel = buttonPage.selected_index;
        
        if (! buttonPage.button_states[channel])
        {
            ESP_LOGW(s_tag, "Slider change ignored - LED is OFF");
            return;
        }
        
        int32_t value = lv_slider_get_value(slider);
        sliderPos[channel] = (uint32_t)(value * 10);
        buttonPage.slider_values[channel] = sliderPos[channel];

        // Update percentage label
        char percent_text[16];
        snprintf(percent_text, sizeof(percent_text), "%lu%%", (unsigned long)value);
        lv_label_set_text(buttonPage.main_status_label, percent_text);

        // Segmentleri güncelle
        int full_segments = value / 10;
        int partial_percent = value % 10;
        
        for (int i = 0; i < 10; i++)
        {
            if (active_segments[i] != NULL)
            {
                if (i < full_segments)
                {
                    lv_obj_clear_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_style_bg_opa(active_segments[i], LV_OPA_100, 0);
                }
                else if (i == full_segments && partial_percent > 0)
                {
                    lv_obj_clear_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                    lv_opa_t opacity = (lv_opa_t)((partial_percent * 255) / 10);
                    if (opacity < 30) opacity = 30;
                    lv_obj_set_style_bg_opa(active_segments[i], opacity, 0);
                    lv_obj_set_style_shadow_opa(active_segments[i], opacity * 70 / 100, 0);
                }
                else
                {
                    lv_obj_add_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        // NVS'e kaydet
        saveBrightnessToNVS(channel, sliderPos[channel]);

        // LED parlaklık gönder
        uint32_t led_node_id = getLEDNodeID();
        
        if (led_node_id == 0) {
            ESP_LOGW(s_tag, "LED node not found, skipping brightness update");
            return;
        }
        
        uint8_t cmd[8] = {0};
        cmd[0] = LED_CMD_SET_BRIGHTNESS;
        cmd[1] = channel + 1;
        cmd[2] = (uint8_t)(sliderPos[channel] >> 8);
        cmd[3] = (uint8_t)(sliderPos[channel] & 0xFF);
        
        esp_err_t err = sendCanMessage(led_node_id, cmd, 4);
        if (err == ESP_OK) {
            ESP_LOGI(s_tag, "✓ LED%d BRIGHTNESS:  %lu/1000", channel+1, sliderPos[channel]);
        }
    }
}

// Main power button event callback
static void main_power_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int channel = buttonPage.selected_index;
        buttonPage.button_states[channel] = !buttonPage.button_states[channel];

        update_sidebar_indicators();
        
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *power_icon = lv_obj_get_child(btn, 0);
        
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
            lv_obj_set_style_text_color(power_icon, active_color, 0);
            lv_obj_set_style_shadow_width(btn, 30, 0);
            lv_obj_set_style_shadow_color(btn, active_color, 0);
            lv_obj_set_style_shadow_opa(btn, is_day_mode ? LV_OPA_40 : LV_OPA_90, 0);
            
            if (status_label)
            {
                lv_label_set_text(status_label, "ACTIVE");
                lv_obj_set_style_text_color(status_label, active_color, 0);
            }
            
            lv_obj_clear_state(buttonPage.main_slider, LV_STATE_DISABLED);
            lv_obj_add_flag(buttonPage.main_slider, LV_OBJ_FLAG_CLICKABLE);
            
            uint32_t slider_percent = sliderPos[channel] / 10;
            if (slider_percent > 100) slider_percent = 100;
            if (slider_percent == 0) slider_percent = 50;
            
            sliderPos[channel] = slider_percent * 10;
            buttonPage.slider_values[channel] = sliderPos[channel];
            
            update_slider_visual();
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
            lv_obj_set_style_text_color(power_icon, inactive_color, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            
            if (status_label)
            {
                lv_label_set_text(status_label, "INACTIVE");
                lv_obj_set_style_text_color(status_label, inactive_color, 0);
            }
            
            lv_obj_add_state(buttonPage.main_slider, LV_STATE_DISABLED);
            lv_obj_clear_flag(buttonPage.main_slider, LV_OBJ_FLAG_CLICKABLE);
            
            lv_slider_set_value(buttonPage.main_slider, 0, LV_ANIM_ON);
            lv_label_set_text(buttonPage.main_status_label, "0%");
            
            for (int i = 0; i < 10; i++)
            {
                lv_obj_add_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        
        // ======== CENTRAL MASTER LED CONTROL ========
        uint8_t room_id = usrGraphicalInterface_getSelectedRoom();
        
        if (room_id == 0)
        {
            ESP_LOGE(s_tag, "❌ No room selected!");
            usrLightingPage_setStatus("No room selected");
            return;
        }
        
        if (!isRoomActive(room_id))
        {
            ESP_LOGE(s_tag, "❌ Room %d not active!", room_id);
            usrLightingPage_setStatus("Room offline");
            return;
        }
        
        // LED kontrolünü room'a gönder
        uint8_t led_ch = channel + 1; // 1-6
        uint16_t brightness_val = buttonPage.button_states[channel] ? sliderPos[channel] : 0;
        
        sendLEDControlToRoom(room_id, led_ch, buttonPage.button_states[channel], brightness_val);
        
        ESP_LOGI(s_tag, "✓ LED command sent to Room %d: LED%d=%s, Brightness=%d", 
                 room_id, led_ch, buttonPage.button_states[channel] ? "ON" : "OFF", brightness_val);
        
        ESP_LOGI(s_tag, "Power button toggled - Channel %d: %s", 
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
                lv_obj_set_style_border_color(buttonPage.sidebar_btns[buttonPage.selected_index], lv_color_make(40, 60, 80), 0);
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
            lv_obj_t *center_panel = lv_obj_get_parent(lv_obj_get_parent(buttonPage.main_power_btn));
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
                // LED ON
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                lv_color_t active_color = is_day_mode ? COLOR_ACCENT_BLUE_DAY : COLOR_NEON_CYAN;
                lv_color_t bg_color = is_day_mode ?  lv_color_make(235, 245, 251) : lv_color_make(15, 25, 35);
                
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
                
                lv_obj_clear_state(buttonPage.main_slider, LV_STATE_DISABLED);
                lv_obj_add_flag(buttonPage.main_slider, LV_OBJ_FLAG_CLICKABLE);
                
                update_slider_visual();
            }
            else
            {
                // LED OFF
                bool is_day_mode = !usrGraphicalInterface_isDarkMode();
                lv_color_t inactive_color = is_day_mode ? COLOR_INACTIVE_GRAY_DAY : COLOR_TEXT_SECONDARY_NIGHT;
                lv_color_t bg_color = is_day_mode ? lv_color_make(246, 248, 251) : lv_color_make(15, 25, 35);
                
                lv_obj_set_style_bg_color(buttonPage.main_power_btn, bg_color, 0);
                lv_obj_set_style_border_color(buttonPage.main_power_btn, inactive_color, 0);
                lv_obj_set_style_border_opa(buttonPage.main_power_btn, LV_OPA_30, 0);
                lv_obj_set_style_border_width(buttonPage. main_power_btn, 2, 0);
                lv_obj_set_style_text_color(power_icon, inactive_color, 0);
                lv_obj_set_style_shadow_width(buttonPage. main_power_btn, 0, 0);
                
                if (status_label)
                {
                    lv_label_set_text(status_label, "INACTIVE");
                    lv_obj_set_style_text_color(status_label, inactive_color, 0);
                }
                
                lv_obj_add_state(buttonPage.main_slider, LV_STATE_DISABLED);
                lv_obj_clear_flag(buttonPage.main_slider, LV_OBJ_FLAG_CLICKABLE);
                lv_slider_set_value(buttonPage. main_slider, 0, LV_ANIM_OFF);
                lv_label_set_text(buttonPage. main_status_label, "0%");
                
                for (int i = 0; i < 10; i++)
                {
                    if (active_segments[i] != NULL)
                    {
                        lv_obj_add_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
            
            ESP_LOGI(s_tag, "Selected LED:  %s (index %d)", 
                     get_button_name(clicked_index), clicked_index);
        }
    }
    update_sidebar_indicators();
}


// Close All button callback
static void close_all_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(s_tag, "Close All button clicked");
        
        uint8_t room_id = usrGraphicalInterface_getSelectedRoom();
        
        if (room_id == 0)
        {
            ESP_LOGE(s_tag, "No room selected!");
            return;
        }
        
        // Tüm LED'leri kapat
        for (int i = 0; i < 6; i++)
        {
            if (buttonPage.button_states[i])
            {
                buttonPage.button_states[i] = false;
                sendLEDControlToRoom(room_id, i + 1, false, 0);
                ESP_LOGI(s_tag, "LED %d turned off", i + 1);
            }
        }
        
        update_sidebar_indicators();
        
        // Seçili LED'i güncelle
        usrLightingPage_updateThemeColors();
        
        ESP_LOGI(s_tag, "All LEDs turned OFF for Room %d", room_id);
    }
}

void usrLightingPage_processCanData(uint32_t can_id, uint32_t data)
{
    ESP_LOGI(s_tag, "Received CAN data:  ID=0x%03lX, Value=%lu", can_id, data);
}

void usrLightingPage_init(lv_obj_t *parent)
{
    if (buttonPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Lighting page already initialized");
        return;
    }

    // NVS'ten brightness değerlerini yükle
    ESP_LOGI(s_tag, "Loading brightness values from NVS...");
    for (int i = 0; i < 6; i++)
    {
        sliderPos[i] = loadBrightnessFromNVS(i);
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

    // Bulb icon
    buttonPage.header_icon = lv_label_create(header);
    lv_label_set_text(buttonPage. header_icon, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(buttonPage.header_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(buttonPage.header_icon, COLOR_NEON_CYAN, 0);
    lv_obj_align(buttonPage.header_icon, LV_ALIGN_LEFT_MID, 15, 0);

    // Title
    buttonPage.title_label = lv_label_create(header);
    lv_label_set_text(buttonPage.title_label, "LIGHTING");
    lv_obj_set_style_text_font(buttonPage.title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(buttonPage.title_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(buttonPage.title_label, LV_ALIGN_LEFT_MID, 55, -6);

    // Subtitle
    lv_obj_t *subtitle = lv_label_create(header);
    lv_label_set_text(subtitle, "CONTROL PANEL");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(subtitle, 1, 0);
    lv_obj_align(subtitle, LV_ALIGN_LEFT_MID, 55, 8);

    // Home button
    buttonPage.home_button = lv_btn_create(header);
    lv_obj_set_size(buttonPage. home_button, 40, 40);
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

    // Create LED 1 to LED 6 buttons
    for (int i = 0; i < 6; i++)
    {
        buttonPage.sidebar_btns[i] = lv_obj_create(sidebar);
        lv_obj_set_size(buttonPage.sidebar_btns[i], 200, 42);
        lv_obj_set_pos(buttonPage.sidebar_btns[i], 0, i * 50 + 10);
        
        lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(buttonPage.sidebar_btns[i], 2, 0);
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
    
    // Select LED 1 by default (index 0)
    buttonPage.selected_index = 0;
    lv_obj_set_style_bg_color(buttonPage. sidebar_btns[0], lv_color_make(20, 40, 60), 0);
    lv_obj_set_style_bg_opa(buttonPage.sidebar_btns[0], LV_OPA_70, 0);
    lv_obj_set_style_border_width(buttonPage.sidebar_btns[0], 3, 0);
    lv_obj_set_style_border_color(buttonPage.sidebar_btns[0], COLOR_NEON_CYAN, 0);
    lv_obj_set_style_border_opa(buttonPage.sidebar_btns[0], LV_OPA_100, 0);
    lv_obj_set_style_shadow_width(buttonPage.sidebar_btns[0], 20, 0);
    lv_obj_set_style_shadow_color(buttonPage.sidebar_btns[0], COLOR_NEON_CYAN, 0);
    lv_obj_set_style_shadow_opa(buttonPage.sidebar_btns[0], LV_OPA_70, 0);
    
    lv_obj_t *selected_label = lv_obj_get_child(buttonPage. sidebar_btns[0], 0);
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
    lv_obj_set_size(center_panel, 415, LV_VER_RES - 70);
    lv_obj_align(center_panel, LV_ALIGN_TOP_LEFT, 230, 70);
    lv_obj_set_style_bg_color(center_panel, COLOR_CARD_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(center_panel, LV_OPA_50, 0);
    lv_obj_set_style_border_width(center_panel, 0, 0);
    lv_obj_set_style_radius(center_panel, 0, 0);
    lv_obj_set_style_pad_all(center_panel, 20, 0);
    lv_obj_clear_flag(center_panel, LV_OBJ_FLAG_SCROLLABLE);

    // "LED CONTROL" label
    buttonPage.main_device_name = lv_label_create(center_panel);
    lv_label_set_text(buttonPage.main_device_name, get_button_name(0));
    lv_obj_set_style_text_font(buttonPage.main_device_name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(buttonPage.main_device_name, COLOR_NEON_CYAN, 0);
    lv_obj_set_style_text_letter_space(buttonPage. main_device_name, 2, 0);
    lv_obj_align(buttonPage. main_device_name, LV_ALIGN_TOP_MID, 0, 30);

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
    lv_obj_set_size(buttonPage. main_power_btn, 130, 130);
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

   

    // ==================== RIGHT PANEL (SLIDER) ====================
    lv_obj_t *right_panel = lv_obj_create(buttonPage.page_container);
    lv_obj_set_size(right_panel, 155, LV_VER_RES - 70);
    lv_obj_align(right_panel, LV_ALIGN_TOP_LEFT, 645, 70);
    lv_obj_set_style_bg_color(right_panel, COLOR_CARD_BG_NIGHT, 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_50, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_radius(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 20, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Percentage label
    buttonPage.main_status_label = lv_label_create(right_panel);
    lv_label_set_text(buttonPage.main_status_label, "0%");
    lv_obj_set_style_text_font(buttonPage.main_status_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(buttonPage.main_status_label, COLOR_TEXT_PRIMARY_NIGHT, 0);
    lv_obj_align(buttonPage.main_status_label, LV_ALIGN_TOP_MID, -10, 20);

    // Slider container
    lv_obj_t *slider_container = lv_obj_create(right_panel);
    lv_obj_set_size(slider_container, 60, 280);
    lv_obj_align(slider_container, LV_ALIGN_CENTER, -10, 20);
    lv_obj_set_style_bg_opa(slider_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slider_container, 0, 0);
    lv_obj_set_style_pad_all(slider_container, 0, 0);
    lv_obj_clear_flag(slider_container, LV_OBJ_FLAG_SCROLLABLE);

    // Segment parametreleri
    #define SEGMENT_COUNT 10
    #define SEGMENT_HEIGHT 24
    #define SEGMENT_GAP 4
    #define SEGMENT_WIDTH 50

    // Background segments
    for (int i = 0; i < SEGMENT_COUNT; i++)
    {
        bg_segments[i] = lv_obj_create(slider_container);
        lv_obj_set_size(bg_segments[i], SEGMENT_WIDTH, SEGMENT_HEIGHT);
        
        int y_pos = 140 - (i * (SEGMENT_HEIGHT + SEGMENT_GAP)) - (SEGMENT_HEIGHT / 2);
        lv_obj_align(bg_segments[i], LV_ALIGN_CENTER, 0, y_pos);
        
        lv_obj_set_style_bg_color(bg_segments[i], lv_color_make(15, 25, 35), 0);
        lv_obj_set_style_bg_opa(bg_segments[i], LV_OPA_100, 0);
        lv_obj_set_style_radius(bg_segments[i], 6, 0);
        lv_obj_set_style_border_width(bg_segments[i], 1, 0);
        lv_obj_set_style_border_color(bg_segments[i], lv_color_make(20, 35, 50), 0);
        lv_obj_set_style_border_opa(bg_segments[i], LV_OPA_40, 0);
        lv_obj_clear_flag(bg_segments[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }

    // Hidden slider
    buttonPage.main_slider = lv_slider_create(slider_container);
    lv_obj_set_size(buttonPage.main_slider, SEGMENT_WIDTH, 280);
    lv_obj_align(buttonPage.main_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(buttonPage.main_slider, 0, 100);
    lv_slider_set_value(buttonPage.main_slider, 0, LV_ANIM_OFF);

    // Make slider invisible
    lv_obj_set_style_bg_opa(buttonPage.main_slider, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(buttonPage.main_slider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(buttonPage.main_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(buttonPage.main_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(buttonPage.main_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(buttonPage.main_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(buttonPage.main_slider, 0, LV_PART_KNOB);

    // Active segments
    for (int i = 0; i < SEGMENT_COUNT; i++)
    {
        active_segments[i] = lv_obj_create(slider_container);
        lv_obj_set_size(active_segments[i], SEGMENT_WIDTH, SEGMENT_HEIGHT);
        
        int y_pos = 140 - (i * (SEGMENT_HEIGHT + SEGMENT_GAP)) - (SEGMENT_HEIGHT / 2);
        lv_obj_align(active_segments[i], LV_ALIGN_CENTER, 0, y_pos);
        
        lv_obj_set_style_bg_color(active_segments[i], COLOR_NEON_CYAN, 0);
        lv_obj_set_style_bg_opa(active_segments[i], LV_OPA_100, 0);
        lv_obj_set_style_radius(active_segments[i], 6, 0);
        lv_obj_set_style_border_width(active_segments[i], 0, 0);
        
        lv_obj_set_style_shadow_width(active_segments[i], 15, 0);
        lv_obj_set_style_shadow_color(active_segments[i], COLOR_NEON_CYAN, 0);
        lv_obj_set_style_shadow_opa(active_segments[i], LV_OPA_70, 0);
        lv_obj_set_style_shadow_spread(active_segments[i], 2, 0);
        
        lv_obj_clear_flag(active_segments[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_add_flag(active_segments[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Disable slider initially
    lv_obj_add_state(buttonPage.main_slider, LV_STATE_DISABLED);
    lv_obj_clear_flag(buttonPage.main_slider, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(buttonPage.main_slider, main_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // "INTENSITY" label
    lv_obj_t *intensity_label = lv_label_create(right_panel);
    lv_label_set_text(intensity_label, "");
    lv_obj_set_style_text_font(intensity_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(intensity_label, COLOR_TEXT_SECONDARY_NIGHT, 0);
    lv_obj_set_style_text_letter_space(intensity_label, 2, 0);
    lv_obj_align(intensity_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Initialize states
    for (int i = 0; i < 6; i++)
    {
        buttonPage.button_states[i] = false;
        buttonPage.slider_values[i] = 0;
    }

    usrLightingPage_hide();

    update_sidebar_indicators();

    ESP_LOGI(s_tag, "Lighting page initialized with new design");
}

void usrLightingPage_show(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_clear_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = true;
        usrLightingPage_updateButtonNames();
        ESP_LOGI(s_tag, "Lighting page shown");
    }
}

void usrLightingPage_updateButtonNames(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (buttonPage.sidebar_btns[i] != NULL)
        {
            lv_obj_t *label = lv_obj_get_child(buttonPage. sidebar_btns[i], 0);
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

void usrLightingPage_hide(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_add_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = false;
        ESP_LOGI(s_tag, "Lighting page hidden");
    }
}

void usrLightingPage_destroy(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_del(buttonPage.page_container);
        memset(&buttonPage, 0, sizeof(usrLightingPage_t));
        ESP_LOGI(s_tag, "Lighting page destroyed");
    }
}

void usrLightingPage_setStatus(const char *status)
{
    ESP_LOGI(s_tag, "Status:   %s", status ?  status : "NULL");
}

bool usrLightingPage_isActive(void)
{
    return buttonPage.is_active;
}

// Button handlers - Kept original CAN bus functionality
void usrLightingPage_button1_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-1 handler called");

    buttonPage.button_states[0] = ! buttonPage.button_states[0];
    bool state = buttonPage.button_states[0];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }
    
    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 1;
    led_cmd[2] = state ? 1 : 0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED1 SET sent to 0x%03lX:   State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED1 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 1;
        led_cmd[2] = (uint8_t)(sliderPos[0] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[0] & 0xFF);
        
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

void usrLightingPage_button2_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-2 handler called");
    buttonPage.button_states[1] = !buttonPage.button_states[1];
    bool state = buttonPage.button_states[1];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }

    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 2;
    led_cmd[2] = state ? 1 :  0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED2 SET sent to 0x%03lX:   State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED2 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 2;
        led_cmd[2] = (uint8_t)(sliderPos[1] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[1] & 0xFF);
        
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

void usrLightingPage_button3_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-3 handler called");
    buttonPage.button_states[2] = !buttonPage.button_states[2];
    bool state = buttonPage.button_states[2];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }

    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 3;
    led_cmd[2] = state ? 1 : 0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED3 SET sent to 0x%03lX:  State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED3 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 3;
        led_cmd[2] = (uint8_t)(sliderPos[2] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[2] & 0xFF);
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

void usrLightingPage_button4_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-4 handler called");
    buttonPage.button_states[3] = !buttonPage.button_states[3];
    bool state = buttonPage.button_states[3];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }

    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 4;
    led_cmd[2] = state ? 1 : 0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED4 SET sent to 0x%03lX:  State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED4 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 4;
        led_cmd[2] = (uint8_t)(sliderPos[3] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[3] & 0xFF);
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

void usrLightingPage_button5_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-5 handler called");
    buttonPage.button_states[4] = !buttonPage.button_states[4];
    bool state = buttonPage.button_states[4];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }

    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 5;
    led_cmd[2] = state ? 1 :  0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED5 SET sent to 0x%03lX:   State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED5 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 5;
        led_cmd[2] = (uint8_t)(sliderPos[4] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[4] & 0xFF);
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

void usrLightingPage_button6_handler(void)
{
    ESP_LOGI(s_tag, "LightingButton-6 handler called");
    buttonPage.button_states[5] = !buttonPage. button_states[5];
    bool state = buttonPage.button_states[5];

    uint32_t led_node_id = getLEDNodeID();
    
    if (led_node_id == 0)
    {
        ESP_LOGE(s_tag, "❌ LED kartı bulunamadı!");
        usrLightingPage_setStatus("LED kartı bağlı değil");
        return;
    }
    
    uint8_t led_cmd[8] = {0};
    led_cmd[0] = 0x11;
    led_cmd[1] = 6;
    led_cmd[2] = state ? 1 : 0;
    
    esp_err_t err = sendCanMessage(led_node_id, led_cmd, 3);
    if (err == ESP_OK) {
        ESP_LOGI(s_tag, "✓ LED6 SET sent to 0x%03lX:  State=%d", led_node_id, state);
    } else {
        ESP_LOGE(s_tag, "✗ LED6 SET FAILED");
        return;
    }
    
    if (state) {
        vTaskDelay(pdMS_TO_TICKS(10));
        memset(led_cmd, 0, 8);
        led_cmd[0] = 0x12;
        led_cmd[1] = 6;
        led_cmd[2] = (uint8_t)(sliderPos[5] >> 8);
        led_cmd[3] = (uint8_t)(sliderPos[5] & 0xFF);
        sendCanMessage(led_node_id, led_cmd, 4);
    }
    update_sidebar_indicators();
}

bool usrLightingPage_getButtonState(int channel)
{
    if (channel >= 0 && channel < 6)
        return buttonPage.button_states[channel];
    return false;
}

uint32_t usrLightingPage_getSliderValue(int channel)
{
    if (channel >= 0 && channel < 6)
        return sliderPos[channel];
    return 0;
}

uint32_t usrLightingPage_getLEDNodeID(void)
{
    return getLEDNodeID();
}