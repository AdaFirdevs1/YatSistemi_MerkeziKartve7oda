#ifndef _USR_SETTINGS_PAGE_H
#define _USR_SETTINGS_PAGE_H

#include "lvgl.h"
#include <stdbool.h>

// Settings page structure
typedef struct {
    lv_obj_t *parent;
    lv_obj_t *page_container;
    
    // Section Selector (Ana Sayfa)
    lv_obj_t *header;
    lv_obj_t *title_label;
    lv_obj_t *status_label;
    lv_obj_t *history_button;
    lv_obj_t *home_button;
    lv_obj_t *edit_password_button;
    lv_obj_t *section_buttons[3];
    lv_obj_t *section_labels[3];
    
    // Editor Container
    lv_obj_t *editor_container;
    lv_obj_t *editor_header;         
    lv_obj_t *editor_title;
    lv_obj_t *editor_home_button;  
    lv_obj_t *button_name_labels[6];
    lv_obj_t *button_name_inputs[6];
    lv_obj_t *keyboard;
    lv_obj_t *save_button;
    lv_obj_t *cancel_button;
    
    // History Container
    lv_obj_t *history_container;
    lv_obj_t *history_header;         
    lv_obj_t *history_title;
    lv_obj_t *history_list;
    lv_obj_t *back_button;
    
    int current_section;
    bool is_active;
    bool is_editor_active;
    bool is_history_active;
} usrSettingsPage_t;

// Public function declarations
void usrSettingsPage_init(lv_obj_t *parent);
void usrSettingsPage_show(void);
void usrSettingsPage_hide(void);
void usrSettingsPage_destroy(void);
void usrSettingsPage_setStatus(const char *status);
bool usrSettingsPage_isActive(void);

// Button name management
void usrSettingsPage_loadButtonNames(void);
void usrSettingsPage_saveButtonNames(void);
const char* usrSettingsPage_getButtonName(int section, int button);
void usrSettingsPage_setButtonName(int section, int button, const char *name);
void usrSettingsPage_updateThemeColors(void);

// History management
int usrSettingsPage_getHistoryCount(void);
void usrSettingsPage_clearHistory(void);

#endif /* _USR_SETTINGS_PAGE_H */