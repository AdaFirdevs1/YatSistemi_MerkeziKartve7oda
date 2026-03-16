#ifndef _USR_LIGHTING_PAGE_H_
#define _USR_LIGHTING_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

typedef enum
{
    LBUTTON_1_PRESSED = 1,
    LBUTTON_2_PRESSED,
    LBUTTON_3_PRESSED,
    LBUTTON_4_PRESSED,
    LBUTTON_5_PRESSED,
    LBUTTON_6_PRESSED
} Lbutton_event_t;

typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *header_icon;
    lv_obj_t *home_button;
    
    // Old card components (no longer used but kept for compatibility)
    lv_obj_t *buttons[6];
    lv_obj_t *button_labels[6];
    lv_obj_t *pwm_sliders[6];
    lv_obj_t *slider_labels[6];
    
    // New design components
    lv_obj_t *sidebar_btns[6];      // Left sidebar selection buttons
    int selected_index;             // Currently selected LED
    
    // Right side components (Single and Large)
    lv_obj_t *main_power_btn;
    lv_obj_t *main_slider;
    lv_obj_t *main_status_label;
    lv_obj_t *main_device_name;
    
    bool button_states[6];
    uint32_t slider_values[6];
    bool is_active;
} usrLightingPage_t;

void usrLightingPage_init(lv_obj_t *parent);
void usrLightingPage_show(void);
void usrLightingPage_updateButtonNames(void);
void usrLightingPage_hide(void);
void usrLightingPage_destroy(void);
void usrLightingPage_setStatus(const char *status);
bool usrLightingPage_isActive(void);

void usrLightingPage_button1_handler(void);
void usrLightingPage_button2_handler(void);
void usrLightingPage_button3_handler(void);
void usrLightingPage_button4_handler(void);
void usrLightingPage_button5_handler(void);
void usrLightingPage_button6_handler(void);
void usrLightingPage_updateThemeColors(void);

// CAN data processing function
void usrLightingPage_processCanData(uint32_t can_id, uint32_t data);

// LED node kontrolü (public interface)
uint32_t usrLightingPage_getLEDNodeID(void);

bool usrLightingPage_getButtonState(int channel);
uint32_t usrLightingPage_getSliderValue(int channel);

#endif /* _USR_LIGHTING_PAGE_H_ */