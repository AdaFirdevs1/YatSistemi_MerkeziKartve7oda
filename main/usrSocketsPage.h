#ifndef _USR_SOCKETS_PAGE_H_
#define _USR_SOCKETS_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

typedef enum
{
    SBUTTON_1_PRESSED = 1,
    SBUTTON_2_PRESSED,
    SBUTTON_3_PRESSED,
    SBUTTON_4_PRESSED,
    SBUTTON_5_PRESSED,
    SBUTTON_6_PRESSED
} Sbutton_event_t;

typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *header_icon;
    lv_obj_t *home_button;
    
    // Old components (kept for compatibility)
    lv_obj_t *buttons[6];
    lv_obj_t *button_labels[6];
    lv_obj_t *pwm_sliders[6];
    lv_obj_t *slider_labels[6];
    lv_obj_t *status_label;
    
    // New design components
    lv_obj_t *sidebar_btns[6];      // Left sidebar selection buttons
    int selected_index;             // Currently selected socket
    
    // Center panel components
    lv_obj_t *main_power_btn;
    lv_obj_t *main_device_name;
    
    bool button_states[6];
    uint32_t slider_values[6];
    bool is_active;
} usrSocketsPage_t;

void usrSocketsPage_init(lv_obj_t *parent);
void usrSocketsPage_show(void);
void usrSocketsPage_updateButtonNames(void);
void usrSocketsPage_hide(void);
void usrSocketsPage_destroy(void);
void usrSocketsPage_setStatus(const char *status);
bool usrSocketsPage_isActive(void);

void usrSocketsPage_button1_handler(void);
void usrSocketsPage_button2_handler(void);
void usrSocketsPage_button3_handler(void);
void usrSocketsPage_button4_handler(void);
void usrSocketsPage_button5_handler(void);
void usrSocketsPage_button6_handler(void);
void usrSocketsPage_updateThemeColors(void);

bool usrSocketsPage_getButtonState(int channel);
uint32_t usrSocketsPage_getSliderValue(int channel);

#endif /* _USR_SOCKETS_PAGE_H_ */