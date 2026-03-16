#include "relay_node.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// Global variables
static relay_node_t g_node = {0};
static CAN_HandleTypeDef *g_hcan = NULL;
static UART_HandleTypeDef *g_huart = NULL;

// ========== FLASH FUNCTIONS ==========
static uint32_t readAssignedIDFromFlash(void) {
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    if (flash_base[2] == FLASH_MAGIC_NUMBER) return flash_base[0];
    return 0;
}

static uint32_t readMasterIDFromFlash(void) {
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    if (flash_base[2] == FLASH_MAGIC_NUMBER) return flash_base[1];
    return 0;
}

static uint8_t readRoomIDFromFlash(void) {
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    if (flash_base[2] == FLASH_MAGIC_NUMBER) return (uint8_t)flash_base[3];
    return 0;
}

static HAL_StatusTypeDef saveAssignedIDToFlash(uint32_t assigned_id, uint32_t master_id, uint8_t room_id) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t pageError = 0;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = ID_STORAGE_ADDRESS;
    EraseInitStruct.NbPages     = 1;

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &pageError) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
    HAL_Delay(10);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS, assigned_id) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 4, master_id) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 8, FLASH_MAGIC_NUMBER) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 12, (uint32_t)room_id) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
    HAL_FLASH_Lock();
    return HAL_OK;
}

// Relay pin mapping
static const uint16_t relay_pins[6] = {
    Output_1_Pin, Output_2_Pin, Output_3_Pin,
    Output_4_Pin, Output_5_Pin, Output_6_Pin
};

static GPIO_TypeDef* relay_ports[6] = {
    Output_1_GPIO_Port, Output_2_GPIO_Port, Output_3_GPIO_Port,
    Output_4_GPIO_Port, Output_5_GPIO_Port, Output_6_GPIO_Port
};

// ========== DEBUG FUNCTIONS ==========
void Debug_Init(UART_HandleTypeDef *huart)
{
    g_huart = huart;
    Debug_Printf("\r\n\r\n");
    Debug_Printf("========================================\r\n");
    Debug_Printf("  STM32F105 RELAY NODE - NEW PROTOCOL\r\n");
    Debug_Printf("  Node Type: RELAY (0x%02X)\r\n", MY_NODE_TYPE);
    Debug_Printf("  Protocol : Room-based 32-bit CAN ID\r\n");
    Debug_Printf("========================================\r\n");
}

void Debug_Printf(const char *format, ...)
{
#if DEBUG_ENABLE
    if (g_huart == NULL) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    HAL_UART_Transmit(g_huart, (uint8_t*)buffer, strlen(buffer), 100);
#endif
}

void Debug_PrintNodeInfo(void)
{
    const char *type_str =
        (g_node.type == NODE_TYPE_LED) ? "LED" :
        (g_node.type == NODE_TYPE_RELAY) ? "RELAY" :
        (g_node.type == NODE_TYPE_SENSOR) ? "SENSOR" : "UNKNOWN";

    Debug_Printf("\r\n========== NODE STATUS ==========\r\n");
    Debug_Printf("Unique ID    : 0x%08lX\r\n", g_node.unique_id);
    Debug_Printf("Node Type    : %s (0x%02X)\r\n", type_str, g_node.type);
    Debug_Printf("Assigned ID  : 0x%08lX\r\n", g_node.assigned_can_id);  // ← 32-bit
    Debug_Printf("Room ID      : %d\r\n", g_node.room_id);               // ← YENİ
    Debug_Printf("Node Offset  : %d\r\n", g_node.node_offset);           // ← YENİ
    Debug_Printf("Master ID    : 0x%04X\r\n", g_node.master_id);
    Debug_Printf("State        : ");

    switch(g_node.state)
    {
        case NODE_STATE_UNENROLLED:  Debug_Printf("UNENROLLED\r\n"); break;
        case NODE_STATE_WAITING_ID:  Debug_Printf("WAITING_ID\r\n"); break;
        case NODE_STATE_ENROLLED:    Debug_Printf("ENROLLED\r\n"); break;
        case NODE_STATE_ACTIVE:      Debug_Printf("ACTIVE\r\n"); break;
        default:                     Debug_Printf("UNKNOWN\r\n"); break;
    }

    Debug_Printf("Relay States : ");
    for(int i = 0; i < 6; i++)
    {
        Debug_Printf("%d:%s ", i+1, g_node.relay_states[i] ? "ON" : "OFF");
    }
    Debug_Printf("\r\n================================\r\n\r\n");
}

// ========== UNIQUE ID ==========
uint32_t RelayNode_GetUniqueID(void)
{
    uint32_t uid0 = *(uint32_t*)(0x1FFFF7E8);
    uint32_t uid1 = *(uint32_t*)(0x1FFFF7EC);
    uint32_t uid2 = *(uint32_t*)(0x1FFFF7F0);

    Debug_Printf("[UID] Raw: 0x%08lX-%08lX-%08lX\r\n", uid0, uid1, uid2);

    uint32_t unique_id = uid0 ^ uid1 ^ uid2;
    Debug_Printf("[UID] Combined: 0x%08lX\r\n", unique_id);

    return unique_id;
}

// ========== CAN SEND (EXTENDED ID DESTEĞİ) ==========
static void CAN_SendMessage(uint32_t can_id, uint8_t *data, uint8_t dlc)
{
    if (g_hcan == NULL) return;

    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;

    // ← DÜZELTİLDİ: Otomatik Extended/Standard ID belirleme
    if (can_id > 0x7FF)
    {
        // 29-bit Extended ID
        tx_header.ExtId = can_id;
        tx_header.IDE = CAN_ID_EXT;
        tx_header.StdId = 0;
    }
    else
    {
        // 11-bit Standard ID
        tx_header.StdId = can_id;
        tx_header.IDE = CAN_ID_STD;
        tx_header.ExtId = 0;
    }

    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t free_level = HAL_CAN_GetTxMailboxesFreeLevel(g_hcan);

    if (free_level == 0)
    {
        Debug_Printf("[CAN_TX] WARNING: All mailboxes full! Aborting...\r\n");
        HAL_CAN_AbortTxRequest(g_hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
        HAL_Delay(10);

        free_level = HAL_CAN_GetTxMailboxesFreeLevel(g_hcan);
        if (free_level == 0)
        {
            Debug_Printf("[CAN_TX] ERROR: Still no free mailbox!\r\n");
            HAL_CAN_Stop(g_hcan);
            HAL_Delay(10);
            HAL_CAN_Start(g_hcan);
            HAL_Delay(10);
            Debug_Printf("[CAN_TX] CAN restarted\r\n");
            return;
        }
    }

    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(g_hcan, &tx_header, data, &tx_mailbox);

    if (status == HAL_OK)
    {
        Debug_Printf("[CAN_TX] ID:0x%08lX %s DLC:%d Data:",
                     can_id,
                     (can_id > 0x7FF) ? "Ext" : "Std",
                     dlc);
        for(int i = 0; i < dlc; i++)
        {
            Debug_Printf(" %02X", data[i]);
        }
        Debug_Printf("\r\n");
    }
    else
    {
        Debug_Printf("[CAN_TX] FAILED! Status:%d Error:0x%lX\r\n",
                     status, HAL_CAN_GetError(g_hcan));
    }
}

// ========== RELAY CONTROL ==========
void RelayNode_SetRelay(uint8_t relay_num, bool state)
{
    if (relay_num >= 6) return;

    g_node.relay_states[relay_num] = state;

    GPIO_PinState pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(relay_ports[relay_num], relay_pins[relay_num], pin_state);

    Debug_Printf("[RELAY] Relay %d → %s\r\n", relay_num + 1, state ? "ON" : "OFF");
}

void RelayNode_SetAllRelays(bool state)
{
    Debug_Printf("[RELAY] ALL relays → %s\r\n", state ? "ON" : "OFF");
    for (int i = 0; i < 6; i++)
    {
        RelayNode_SetRelay(i, state);
    }
}

// ========== RESET & REQUEST ID ==========
static void ResetAndRequestID(void)
{
    Debug_Printf("\r\n[RESET] Erasing Flash and searching for new master...\r\n");

    // Flash'ı Temizle
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t pageError = 0;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = ID_STORAGE_ADDRESS;
    EraseInitStruct.NbPages     = 1;
    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&EraseInitStruct, &pageError);
    HAL_FLASH_Lock();

    // Node bilgilerini temizle
    g_node.assigned_can_id = 0;
    g_node.master_id = 0;
    g_node.room_id = 0;
    g_node.node_offset = 0;
    g_node.last_master_heartbeat_time = 0;
    g_node.state = NODE_STATE_UNENROLLED;
    g_node.assignment_part1_received = false;

    HAL_Delay(100);
    RelayNode_RequestID();
}

// ========== MASTER HEARTBEAT ==========
static void ProcessMasterHeartbeat(uint8_t *data, uint8_t dlc)
{
    if (dlc < 3) return;
    uint8_t command = data[0];
    if (command != CMD_MASTER_HEARTBEAT) return;

    uint16_t incoming_master_id = ((uint16_t)data[1] << 8) | data[2];

    // Yabancı master'dan heartbeat gelirse REDDET (LED kartındaki mantık)
    if ((g_node.state == NODE_STATE_ACTIVE || g_node.state == NODE_STATE_ENROLLED) &&
        g_node.master_id != 0 && incoming_master_id != g_node.master_id)
    {
        static uint8_t reject_count = 0;
        if (++reject_count >= 10)
        {
            reject_count = 0;
            Debug_Printf("[WARN] Ignored Master HB from 0x%04X (Locked to 0x%04X)\r\n",
                         incoming_master_id, g_node.master_id);
        }
        return;
    }

    // Kendi master'ımızdan heartbeat
    if ((g_node.state == NODE_STATE_ACTIVE || g_node.state == NODE_STATE_ENROLLED) &&
        incoming_master_id == g_node.master_id)
    {
        g_node.last_master_heartbeat_time = HAL_GetTick();
        static uint8_t hb_count = 0;
        if (++hb_count >= 10)
        {
            hb_count = 0;
            Debug_Printf("[MASTER HB] Received from Master:0x%04X\r\n", incoming_master_id);
        }
    }
}

// ========== MASTER TIMEOUT ==========
static void CheckMasterTimeout(void)
{
    if (g_node.state != NODE_STATE_ACTIVE && g_node.state != NODE_STATE_ENROLLED) return;
    if (g_node.master_id == 0 || g_node.last_master_heartbeat_time == 0) return;

    uint32_t current_time = HAL_GetTick();

    if ((current_time - g_node.last_master_heartbeat_time) > MASTER_HEARTBEAT_TIMEOUT_MS)
    {
        // RESETLEME! Sadece süreyi güncelle ve sessizce ESP'nin geri gelmesini bekle.
        Debug_Printf("\r\n[MASTER TIMEOUT] Locked Master:0x%04X, Waiting indefinitely...\r\n", g_node.master_id);
        g_node.last_master_heartbeat_time = current_time;
    }
}

// ========== NODE INIT ==========
void RelayNode_Init(CAN_HandleTypeDef *hcan, UART_HandleTypeDef *huart)
{
    g_hcan = hcan;
    Debug_Init(huart);
    Debug_Printf("[INIT] Starting Relay Node initialization...\r\n");

    memset(&g_node, 0, sizeof(relay_node_t));
    g_node.unique_id = RelayNode_GetUniqueID();
    g_node.type = MY_NODE_TYPE;

    RelayNode_SetAllRelays(false);

    // FLASH'TAN OKU
    uint32_t saved_id = readAssignedIDFromFlash();
    uint32_t saved_master_id = readMasterIDFromFlash();
    uint8_t saved_room_id = readRoomIDFromFlash();

    if (saved_id != 0 && saved_id != 0xFFFFFFFF)
    {
        g_node.assigned_can_id = saved_id;
        g_node.master_id = saved_master_id;
        g_node.room_id = saved_room_id;
        g_node.node_offset = GET_NODE_ID(saved_id);
        g_node.state = NODE_STATE_ACTIVE;

        Debug_Printf("[INIT] [LOCKED] Node bound to Master: 0x%04X\r\n", g_node.master_id);
        Debug_Printf("[INIT] CAN ID: 0x%08lX, Room: %d\r\n", g_node.assigned_can_id, g_node.room_id);
    }
    else
    {
        g_node.state = NODE_STATE_UNENROLLED;
        Debug_Printf("[INIT] No valid ID in Flash. Searching for master...\r\n");
        HAL_Delay(100);
        RelayNode_RequestID();
    }
}

// ========== ID REQUEST ==========
void RelayNode_RequestID(void)
{
    Debug_Printf("[ENROLL] Requesting ID from master...\r\n");
    Debug_Printf("[ENROLL] UID:0x%08lX, Type:RELAY (0x%02X)\r\n",
                 g_node.unique_id, MY_NODE_TYPE);

    uint8_t data[8] = {0};

    data[0] = CMD_REQUEST_ID;
    data[1] = (uint8_t)(g_node.unique_id >> 24);
    data[2] = (uint8_t)(g_node.unique_id >> 16);
    data[3] = (uint8_t)(g_node.unique_id >> 8);
    data[4] = (uint8_t)(g_node.unique_id);
    data[5] = MY_NODE_TYPE;

    CAN_SendMessage(CAN_ID_REQUEST_ID, data, 6);

    g_node.state = NODE_STATE_WAITING_ID;
    g_node.enrollment_start_time = HAL_GetTick();
    g_node.assignment_part1_received = false;
}

// ========== ID CONFIRMATION ==========
static void SendIDConfirmation(void)
{
    Debug_Printf("[ENROLL] Sending ID confirmation...\r\n");
    Debug_Printf("[ENROLL] Assigned CAN ID: 0x%08lX\r\n", g_node.assigned_can_id);
    Debug_Printf("[ENROLL] Room:%d, Offset:%d\r\n", g_node.room_id, g_node.node_offset);

    uint8_t data[8] = {0};

    data[0] = CMD_ID_CONFIRMATION;

    // UID (4 byte)
    data[1] = (uint8_t)(g_node.unique_id >> 24);
    data[2] = (uint8_t)(g_node.unique_id >> 16);
    data[3] = (uint8_t)(g_node.unique_id >> 8);
    data[4] = (uint8_t)(g_node.unique_id);

    // Assigned CAN ID (ilk 3 byte)
    data[5] = (uint8_t)(g_node.assigned_can_id >> 24);
    data[6] = (uint8_t)(g_node.assigned_can_id >> 16);
    data[7] = (uint8_t)(g_node.assigned_can_id >> 8);

    CAN_SendMessage(CAN_ID_ASSIGNMENT_ID, data, 8);

    HAL_Delay(10);

    // İkinci mesaj: Son byte
    uint8_t data2[8] = {0};
    data2[0] = CMD_ID_CONFIRMATION;
    data2[1] = (uint8_t)(g_node.assigned_can_id);

    CAN_SendMessage(CAN_ID_ASSIGNMENT_ID, data2, 2);

    g_node.state = NODE_STATE_ENROLLED;

    Debug_Printf("[ENROLL] Node successfully enrolled!\r\n");
    Debug_PrintNodeInfo();
}

// ========== HEARTBEAT ==========
void RelayNode_SendHeartbeat(void)
{
    if (g_node.state != NODE_STATE_ACTIVE && g_node.state != NODE_STATE_ENROLLED)
        return;

    uint8_t data[8] = {0};

    // ← DÜZELTİLDİ: Heartbeat artık assigned CAN ID ile gönderilir
    uint32_t heartbeat_id = g_node.assigned_can_id;

    data[0] = CMD_HEARTBEAT;
    data[1] = (uint8_t)(g_node.unique_id >> 24);
    data[2] = (uint8_t)(g_node.unique_id >> 16);
    data[3] = (uint8_t)(g_node.unique_id >> 8);
    data[4] = (uint8_t)(g_node.unique_id);
    data[5] = (uint8_t)(g_node.master_id >> 8);
    data[6] = (uint8_t)(g_node.master_id & 0xFF);

    CAN_SendMessage(heartbeat_id, data, 7);

    g_node.last_heartbeat_time = HAL_GetTick();

    if (g_node.state == NODE_STATE_ENROLLED)
    {
        g_node.state = NODE_STATE_ACTIVE;
        Debug_Printf("[STATE] Node is now ACTIVE!\r\n");
    }

    static uint8_t hb_counter = 0;
    if (++hb_counter >= 10)
    {
        hb_counter = 0;
        Debug_Printf("[HB] Sent (ID:0x%08lX)\r\n", heartbeat_id);
    }
}

// ========== PROCESS CAN MESSAGES ==========
void RelayNode_ProcessCANMessage(uint32_t can_id, uint8_t *data, uint8_t dlc, bool is_extended)
{
    if (dlc < 1) return;

    uint8_t command = data[0];

    Debug_Printf("[CAN_RX] ID:0x%08lX %s CMD:0x%02X DLC:%d\r\n",
                 can_id,
                 is_extended ? "Ext" : "Std",
                 command, dlc);

    // Master heartbeat
    if (can_id == CAN_MASTER_HEARTBEAT_ID)
    {
        ProcessMasterHeartbeat(data, dlc);
        return;
    }

    // ========== ID ASSIGNMENT (2 MESAJ - DÜZELTİLDİ) ==========
    if (can_id == CAN_ID_ASSIGNMENT_ID && command == CMD_ASSIGN_ID)
    {
        Debug_Printf("[CAN_RX] ID Assignment message received\r\n");

        // ← DÜZELTİLDİ: İLK MESAJ (DLC=8)
        if (dlc >= 8 && !g_node.assignment_part1_received)
        {
            Debug_Printf("[CAN_RX] Assignment Part 1/2 (DLC=%d)\r\n", dlc);

            // UID kontrolü
            uint32_t unique_id = ((uint32_t)data[1] << 24) |
                                ((uint32_t)data[2] << 16) |
                                ((uint32_t)data[3] << 8) |
                                data[4];

            if (unique_id != g_node.unique_id)
            {
                Debug_Printf("[CAN_RX] Not for me (Target:0x%08lX, Mine:0x%08lX)\r\n",
                             unique_id, g_node.unique_id);
                return;
            }

            // Buffer'a kaydet
            memcpy(g_node.assignment_buffer, data, 8);
            g_node.assignment_part1_received = true;

            Debug_Printf("[CAN_RX] Part 1 buffered, waiting for Part 2...\r\n");
            return;
        }
        // ← DÜZELTİLDİ: İKİNCİ MESAJ (DLC=5)
        else if (dlc >= 5 && g_node.assignment_part1_received)
        {
            Debug_Printf("[CAN_RX] Assignment Part 2/2 (DLC=%d)\r\n", dlc);

            // İlk mesajdan UID ve CAN ID'nin ilk 3 byte'ı
            uint32_t unique_id = ((uint32_t)g_node.assignment_buffer[1] << 24) |
                                ((uint32_t)g_node.assignment_buffer[2] << 16) |
                                ((uint32_t)g_node.assignment_buffer[3] << 8) |
                                g_node.assignment_buffer[4];

            uint32_t assigned_can_id = ((uint32_t)g_node.assignment_buffer[5] << 24) |
                                       ((uint32_t)g_node.assignment_buffer[6] << 16) |
                                       ((uint32_t)g_node.assignment_buffer[7] << 8);

            // İkinci mesajdan son byte + Master ID + Room ID
            assigned_can_id |= data[1];
            uint16_t new_master_id = ((uint16_t)data[2] << 8) | data[3];
            uint8_t new_room_id = data[4];

            Debug_Printf("[CAN_RX] UID Check: 0x%08lX == 0x%08lX\r\n",
                         unique_id, g_node.unique_id);

            if (unique_id != g_node.unique_id)
            {
                Debug_Printf("[CAN_RX] ERROR: UID mismatch in Part 2!\r\n");
                g_node.assignment_part1_received = false;
                return;
            }

            // Farklı master kontrolü
            if ((g_node.state == NODE_STATE_ACTIVE || g_node.state == NODE_STATE_ENROLLED) &&
				g_node.master_id != 0 && new_master_id != g_node.master_id)
			{
				Debug_Printf("\r\n[ASSIGN REJECTED] Node is locked to Master:0x%04X. Ignored Master:0x%04X\r\n",
							 g_node.master_id, new_master_id);
				g_node.assignment_part1_received = false;
				return;
			}

			// Node bilgilerini kaydet
			g_node.assigned_can_id = assigned_can_id;
			g_node.room_id = new_room_id;
			g_node.node_offset = GET_NODE_ID(assigned_can_id);
			g_node.master_id = new_master_id;

			// ONAYLANAN BİLGİLERİ HAFIZAYA YAZ
			saveAssignedIDToFlash(g_node.assigned_can_id, g_node.master_id, g_node.room_id);

            Debug_Printf("[CAN_RX] ✅ ID Assignment Complete!\r\n");
            Debug_Printf("[CAN_RX] Assigned CAN ID: 0x%08lX\r\n", g_node.assigned_can_id);
            Debug_Printf("[CAN_RX] Room ID: %d\r\n", g_node.room_id);
            Debug_Printf("[CAN_RX] Node Offset: %d\r\n", g_node.node_offset);
            Debug_Printf("[CAN_RX] Master ID: 0x%04X\r\n", g_node.master_id);

            SendIDConfirmation();

            g_node.last_master_heartbeat_time = HAL_GetTick();
            g_node.assignment_part1_received = false;  // Reset flag
        }
        else
        {
            // ← DÜZELTİLDİ: Hata durumu açıklayıcı
            Debug_Printf("[CAN_RX] ERROR: Invalid assignment message!\r\n");
            Debug_Printf("[CAN_RX]   DLC: %d, Part1_received: %d\r\n",
                         dlc, g_node.assignment_part1_received);

            // Flag'i reset et (timeout senaryosu)
            if (g_node.assignment_part1_received)
            {
                Debug_Printf("[CAN_RX] Resetting Part 1 flag (possible timeout)\r\n");
                g_node.assignment_part1_received = false;
            }
            return;
        }
    }
    // ========== REASSIGNMENT ==========
    else if (command == CMD_REASSIGN_REQUEST)
	{
		if (dlc < 5) return;

		uint32_t unique_id = ((uint32_t)data[1] << 24) |
							((uint32_t)data[2] << 16) |
							((uint32_t)data[3] << 8) |
							data[4];

		if (unique_id == g_node.unique_id)
		{
			if (g_node.state == NODE_STATE_ACTIVE || g_node.state == NODE_STATE_ENROLLED)
			{
			    // REASSIGN mesajının DLC>=7 ise master_id içeriyor mu?
			    // Standart REASSIGN 5 byte: cmd(1) + uid(4)
			    // Biz burada master_id'yi kontrol edemiyoruz çünkü mesajda yok
			    // Ama: eğer bu bizim master'ımızdan geliyorsa (ID kontrolü ile)
			    // CAN_ID_ASSIGNMENT_ID üzerinden geliyor = herkese açık broadcast
			    // Güvenli yaklaşım: aynı master'ın HB'si alındıktan sonra gelen REASSIGN'a izin ver

			    uint32_t time_since_master_hb = HAL_GetTick() - g_node.last_master_heartbeat_time;

			    if (time_since_master_hb < 30000) {
			        // Master'dan son 30sn içinde HB aldık → güvenilir
			        // REASSIGN'ı kabul et ama sadece bizim master_id ile eşleşenleri
			        // Mesajda master_id yok, ama master son 30sn aktif → kabul
			        Debug_Printf("[REASSIGN] Accepted from trusted master (last HB: %lu ms ago)\r\n",
			                     time_since_master_hb);
			        // Security block'u atla, aşağı devam et
			    } else {
			        Debug_Printf("[SECURITY BLOCK] Remote wipe rejected! Master HB timeout: %lu ms\r\n",
			                     time_since_master_hb);
			        return;
			    }
			}

			Debug_Printf("[CAN_RX] Reassignment for me! Re-enrolling...\r\n");
			ResetAndRequestID();
		}
	}
    // ========== RELAY COMMANDS ==========
    else if (can_id == g_node.assigned_can_id ||
             can_id == CAN_BROADCAST_ID ||
             can_id == CAN_CENTRAL_COMMAND_ID)
    {
        // Merkezi kontrolden gelen komutlar (Room bazlı)
        if (is_extended && can_id != CAN_BROADCAST_ID)
        {
            uint8_t target_room = GET_ROOM_ID(can_id);
            uint8_t device_type = GET_DEVICE_TYPE(can_id);
            uint16_t node_id = GET_NODE_ID(can_id);

            // Bu komut bize mi?
            if (target_room != g_node.room_id && target_room != ROOM_BROADCAST)
            {
                Debug_Printf("[CAN_RX] Not for our room (Target:%d, Ours:%d)\r\n",
                             target_room, g_node.room_id);
                return;
            }

            if (device_type != DEVICE_TYPE_RELAY && device_type != DEVICE_ALL)
            {
                Debug_Printf("[CAN_RX] Not for relays (Type:0x%02X)\r\n", device_type);
                return;
            }

            if (node_id != g_node.node_offset && node_id != NODE_ALL)
            {
                Debug_Printf("[CAN_RX] Not for our node (Target:%d, Ours:%d)\r\n",
                             node_id, g_node.node_offset);
                return;
            }
        }

        // Komut işleme
        if (g_node.state != NODE_STATE_ACTIVE && g_node.state != NODE_STATE_ENROLLED)
        {
            Debug_Printf("[CAN_RX] Relay command ignored - not enrolled\r\n");
            return;
        }

        if (command == RELAY_CMD_SET && dlc >= 3)
        {
            uint8_t relay_num = data[1];
            uint8_t state = data[2];

            Debug_Printf("[CAN_RX] RELAY_CMD_SET - Relay:%d State:%s\r\n",
                         relay_num, state ? "ON" : "OFF");

            if (relay_num >= 1 && relay_num <= 6)
            {
                RelayNode_SetRelay(relay_num - 1, state != 0);
            }
        }
        else if (command == RELAY_CMD_TOGGLE && dlc >= 2)
        {
            uint8_t relay_num = data[1];

            Debug_Printf("[CAN_RX] RELAY_CMD_TOGGLE - Relay:%d\r\n", relay_num);

            if (relay_num >= 1 && relay_num <= 6)
            {
                bool current_state = g_node.relay_states[relay_num - 1];
                RelayNode_SetRelay(relay_num - 1, !current_state);
            }
        }
        else if (command == RELAY_CMD_ALL_ON)
        {
            Debug_Printf("[CAN_RX] RELAY_CMD_ALL_ON\r\n");
            RelayNode_SetAllRelays(true);
        }
        else if (command == RELAY_CMD_ALL_OFF)
        {
            Debug_Printf("[CAN_RX] RELAY_CMD_ALL_OFF\r\n");
            RelayNode_SetAllRelays(false);
        }
        else
        {
            Debug_Printf("[CAN_RX] Unknown relay command: 0x%02X\r\n", command);
        }
    }
}


// ========== MAIN PROCESS LOOP ==========
void RelayNode_Process(void)
{
    uint32_t current_time = HAL_GetTick();

    // CAN mesajlarını oku
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];

    if (HAL_CAN_GetRxFifoFillLevel(g_hcan, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(g_hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK)
        {
            uint32_t can_id;
            bool is_extended = (rx_header.IDE == CAN_ID_EXT);

            if (is_extended)
            {
                can_id = rx_header.ExtId;
            }
            else
            {
                can_id = rx_header.StdId;
            }

            RelayNode_ProcessCANMessage(can_id, rx_data, rx_header.DLC, is_extended);
        }
    }

    // ← YENİ: Assignment Part 1 timeout kontrolü
    if (g_node.assignment_part1_received)
    {
        static uint32_t part1_receive_time = 0;

        if (part1_receive_time == 0)
        {
            part1_receive_time = current_time;
        }

        // 1 saniye timeout
        if ((current_time - part1_receive_time) > 1000)
        {
            Debug_Printf("[WARN] Assignment Part 2 timeout! Resetting flag.\r\n");
            g_node.assignment_part1_received = false;
            part1_receive_time = 0;
        }
    }

    // Master timeout kontrolü
    CheckMasterTimeout();

    // State machine
    switch (g_node.state)
    {
        case NODE_STATE_UNENROLLED:
            if (current_time > 100)
            {
                RelayNode_RequestID();
            }
            break;

        case NODE_STATE_WAITING_ID:
            if ((current_time - g_node.enrollment_start_time) > ENROLLMENT_TIMEOUT_MS)
            {
                Debug_Printf("[ENROLL] Timeout! Retrying...\r\n");
                g_node.assignment_part1_received = false;  // ← Reset flag
                g_node.state = NODE_STATE_UNENROLLED;
            }
            break;

        case NODE_STATE_ENROLLED:
        case NODE_STATE_ACTIVE:
            if ((current_time - g_node.last_heartbeat_time) >= HEARTBEAT_INTERVAL_MS)
            {
                RelayNode_SendHeartbeat();
            }
            break;
    }
}

// ========== FACTORY RESET / ERASE MEMORY ==========
void RelayNode_EraseMemory(void)
{
    HAL_StatusTypeDef status;
    uint32_t pageError = 0;

    Debug_Printf("\r\n========================================\r\n");
    Debug_Printf(" [FLASH] ERASING STORED MEMORY...\r\n");
    Debug_Printf("========================================\r\n");

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = ID_STORAGE_ADDRESS; // Önceki mesajda tanımladığımız adres
    EraseInitStruct.NbPages     = 1;

    // Flash kilidini aç, sayfayı sil ve tekrar kilitle
    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &pageError);
    HAL_FLASH_Lock();

    if (status == HAL_OK)
    {
        Debug_Printf("[FLASH] Memory erased successfully!\r\n");

        // Cihazın mevcut RAM bilgilerini de sıfırla
        g_node.assigned_can_id = 0;
        g_node.master_id = 0;
        g_node.room_id = 0;
        g_node.node_offset = 0;
        g_node.state = NODE_STATE_UNENROLLED;

        // Tüm röleleri güvenli durum olan kapalı konuma getir
        RelayNode_SetAllRelays(false);

        Debug_Printf("[FLASH] System will restart in 2 seconds...\r\n\r\n");
        HAL_Delay(2000);

        // Mikrodenetleyiciyi donanımsal olarak resetle (En temiz yöntem)
        NVIC_SystemReset();
    }
    else
    {
        Debug_Printf("[FLASH] Flash erase failed! Error code: 0x%lX\r\n", pageError);
    }
}
