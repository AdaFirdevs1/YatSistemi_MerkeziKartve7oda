#ifndef _USR_TOILET_PAGE_H_
#define _USR_TOILET_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

typedef enum
{
    TBUTTON_1_PRESSED = 1,
    TBUTTON_2_PRESSED,
    TBUTTON_3_PRESSED,
    TBUTTON_4_PRESSED,
    TBUTTON_5_PRESSED,
    TBUTTON_6_PRESSED
} Tbutton_event_t;

typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *header_icon;
    lv_obj_t *home_button;
    
    // Card components
    lv_obj_t *cards[6];
    lv_obj_t *card_buttons[6];      // Icon buttons (clickable)
    lv_obj_t *button_labels[6];     // Button name labels
    lv_obj_t *icon_labels[6];       // Icon labels on buttons
    lv_obj_t *pwm_sliders[6];       // PWM sliders
    lv_obj_t *slider_labels[6];     // "Açık/Kapalı" labels
    lv_obj_t *slider_percent_labels[6]; // Percentage labels
    
    lv_obj_t *sidebar_btns[6];      // Sol taraftaki seçim butonları
    int selected_index;             // Şu an sağda hangi cihaz görünüyor?
    
    // Sağ taraf bileşenleri (Tekil ve Büyük)
    lv_obj_t *main_power_btn;
    lv_obj_t *main_slider;
    lv_obj_t *main_status_label;
    lv_obj_t *main_device_name;
    
    bool button_states[6];
    uint32_t slider_values[6];
    bool is_active;
} usrToiletPage_t;

void usrToiletPage_init(lv_obj_t *parent);
void usrToiletPage_show(void);
void usrToiletPage_updateButtonNames(void);
void usrToiletPage_hide(void);
void usrToiletPage_destroy(void);
void usrToiletPage_setStatus(const char *status);
bool usrToiletPage_isActive(void);

void usrToiletPage_button1_handler(void);
void usrToiletPage_button2_handler(void);
void usrToiletPage_button3_handler(void);
void usrToiletPage_button4_handler(void);
void usrToiletPage_button5_handler(void);
void usrToiletPage_button6_handler(void);
void usrToiletPage_updateThemeColors(void);
static void slider_draw_event_cb(lv_event_t *e);

bool usrToiletPage_getButtonState(int channel);
uint32_t usrToiletPage_getSliderValue(int channel);

#endif /* _USR_TOILET_PAGE_H_ */