#ifndef _USR_GRAPHICAL_INTERFACE_H
#define _USR_GRAPHICAL_INTERFACE_H

#include "lvgl.h"
#include "usrGeneral.h"
#include "usrGeneralDefines.h"
#include "nvs_flash.h"
#include "nvs.h"

// Include all page headers
#include "usrToiletPage.h"
#include "usrLightingPage.h"
#include "usrSocketsPage.h"
#include "usrSettingsPage.h"

// Page enumeration
typedef enum
{
    PAGE_MAIN_MENU = 0,
    PAGE_TOILET_CONTROL,
    PAGE_LIGHTING_CONTROL,
    PAGE_SOCKETS_CONTROL,
    PAGE_SETTINGS,
    PAGE_PASSWORD_SETUP,
    PAGE_PASSWORD_ENTRY,
    PAGE_PASSWORD_CHANGE,
    PAGE_COUNT
} page_type_t;

// Main interface structure
typedef struct
{
    lv_obj_t *main_container;
    lv_obj_t *navigation_bar;
    lv_obj_t *nav_buttons[PAGE_COUNT];
    lv_obj_t *nav_labels[PAGE_COUNT];
    lv_obj_t *content_area;
    lv_obj_t *status_bar;
    lv_obj_t *time_label;
    lv_obj_t *system_status_label;
    page_type_t current_page;
    bool is_initialized;
    lv_timer_t *time_timer;
    
    // Password related
    bool password_set;
    char stored_password[MAX_PASSWORD_LENGTH + 1];
    bool password_verified;
} usrGraphicalInterface_t;

// Function declarations
void usrGraphicalInterface_init(void);
void usrGraphicalInterface_destroy(void);
void usrGraphicalInterface_showPage(page_type_t page);
void usrGraphicalInterface_updateSystemStatus(const char *status);
bool usrGraphicalInterface_isInitialized(void);
void usrGraphicalInterface_hideBackButton(void);
void usrGraphicalInterface_showBackButton(void);
void usrGraphicalInterface_resetPasswordVerification(void);

// Password management functions
bool usrGraphicalInterface_checkPasswordExists(void);
bool usrGraphicalInterface_savePassword(const char *password);
bool usrGraphicalInterface_verifyPassword(const char *password);
void usrGraphicalInterface_showPasswordSetup(void);
void usrGraphicalInterface_showPasswordEntry(void);
void usrGraphicalInterface_showPasswordChange(void);

// Room selection functions - CENTRAL MASTER
uint8_t usrGraphicalInterface_getSelectedRoom(void);
void usrGraphicalInterface_setSelectedRoom(uint8_t room_id);

// Legacy function for backward compatibility
void buttonMenu(void);

// Status update functions for backward compatibility
void updateEthernetStatus(uint8_t status);
void updateWifiStatus(uint8_t status);
void updateGsmStatus(uint8_t status);
void updateLoraStatus(uint8_t status);
void updateBluetoothStatus(uint8_t status);

void usrGraphicalInterface_setDarkMode(bool dark_mode);
bool usrGraphicalInterface_isDarkMode(void);

#endif /* _USR_GRAPHICAL_INTERFACE_H */