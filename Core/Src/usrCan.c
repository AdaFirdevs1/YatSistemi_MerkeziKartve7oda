#include "usrGeneral.h"
#include "usrCanIDList.h"
#include "time.h"
#include "usrLedControl.h"

#define CAN_BUS_LINE    hcan1

#define ID_STORAGE_ADDRESS ((uint32_t)0x0801FC00)
#define FLASH_MAGIC_NUMBER 0xDEADBEEF

// Global variables
uint32_t g_node_unique_id = 0;
uint32_t g_my_assigned_id = 0;
uint8_t  g_my_room_id = 0;
uint16_t g_my_node_offset = 0;
uint32_t g_master_id = 0;
uint8_t g_node_state = NODE_STATE_UNASSIGNED;
node_type_t g_node_type = NODE_TYPE_LED;

bool g_assignment_part1_received = false;
uint8_t g_assignment_buffer[8] = {0};

static uint32_t last_heartbeat_time = 0;
static uint32_t id_request_time = 0;
static uint8_t id_request_retry_count = 0;
static uint32_t last_can_status_check = 0;
static uint32_t last_master_heartbeat_time = 0;

// ========== FUNCTION PROTOTYPES (DÜZELTİLDİ) ==========
static void parseIncomingValue(uint32_t m_ID, uint32_t m_value, uint8_t* data, uint8_t dlc, bool is_extended);
static void handleDynamicIDProtocol(uint32_t m_ID, uint8_t* data, uint8_t dlc, bool is_extended);
static void requestNodeID(void);
static void sendHeartbeat(void);
static void processIDAssignment(uint8_t* data, uint8_t dlc);  // ← dlc eklendi
static uint32_t generateUniqueNodeID(void);
static void checkCANStatus(void);
static void attemptCANRecovery(void);
static void eraseIDAndRestartAssignment(void);
static uint32_t readAssignedIDFromFlash(void);
static HAL_StatusTypeDef saveAssignedIDToFlash(uint32_t assigned_id, uint32_t master_id, uint8_t room_id);  // ← room_id eklendi
static uint32_t readMasterIDFromFlash(void);
static uint8_t readRoomIDFromFlash(void);  // ← eklendi
static void handleMasterHeartbeat(uint8_t* data, uint8_t dlc);
static void checkMasterTimeout(void);

void initNodeID(void)
{
    g_node_unique_id = generateUniqueNodeID();

    char buffer[256];

    snprintf(buffer, sizeof(buffer),
             "\r\n\r\n"
             "================================================\r\n"
             "       STM32F105 LED NODE - NEW PROTOCOL        \r\n"
             "================================================\r\n"
             " Node UID      : 0x%08lX\r\n"
             " Node Type     : LED (0x%02X)\r\n"
             " Protocol      : Room-based 32-bit CAN ID\r\n"
             " System Clock  : %lu MHz\r\n"
             "================================================\r\n\r\n",
             g_node_unique_id,
             MY_NODE_TYPE,
             HAL_RCC_GetSysClockFreq() / 1000000);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    uint32_t saved_id = readAssignedIDFromFlash();
    uint32_t saved_master_id = readMasterIDFromFlash();
    uint8_t saved_room_id = readRoomIDFromFlash();

    snprintf(buffer, sizeof(buffer),
             "\r\n Reading from Flash:\r\n"
             "   CAN ID       : 0x%08lX\r\n"
             "   Master ID    : 0x%04lX\r\n"
             "   Room ID      : %d\r\n",
             saved_id, saved_master_id, saved_room_id);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    // FLASH DOLUYSA MASTER'A KİLİTLEN
	if (saved_id != 0 && saved_id != 0xFFFFFFFF)
	{
		g_my_assigned_id = saved_id;
		g_master_id = saved_master_id;
		g_my_room_id = saved_room_id;
		g_my_node_offset = GET_NODE_ID(saved_id);
		g_node_state = NODE_STATE_ACTIVE;

		snprintf(buffer, sizeof(buffer),
				 "\r\n"
				 "================================================\r\n"
				 " [LOCKED] NODE BOUND TO MASTER                  \r\n"
				 "================================================\r\n"
				 " Node UID      : 0x%08lX                       \r\n"
				 " CAN ID        : 0x%08lX                       \r\n"
				 " Master ID     : 0x%04lX                       \r\n"
				 " State         : ACTIVE (Waiting for Master)   \r\n"
				 "================================================\r\n\r\n",
				 g_node_unique_id, g_my_assigned_id, (unsigned long)g_master_id);
		HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
	}
	else
	{
		g_node_state = NODE_STATE_UNASSIGNED;
		snprintf(buffer, sizeof(buffer),
				 "\r\n"
				 "================================================\r\n"
				 " [WAIT] SEARCHING FOR AVAILABLE MASTER         \r\n"
				 "================================================\r\n"
				 " Node UID      : 0x%08lX                       \r\n"
				 " State         : READY TO CONNECT              \r\n"
				 " Strategy      : First responding master       \r\n"
				 "================================================\r\n\r\n",
				 g_node_unique_id);
		HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
	}


    checkCANStatus();
}

uint32_t generateUniqueNodeID(void)
{
    uint32_t* uid = (uint32_t*)UID_BASE;
    return uid[0] ^ uid[1] ^ uid[2];
}

static void checkCANStatus(void)
{
    char buffer[256];

    HAL_CAN_StateTypeDef state = HAL_CAN_GetState(&CAN_BUS_LINE);
    uint32_t error = HAL_CAN_GetError(&CAN_BUS_LINE);
    uint32_t tx_mailboxes_free = HAL_CAN_GetTxMailboxesFreeLevel(&CAN_BUS_LINE);

    uint32_t esr = CAN_BUS_LINE.Instance->ESR;
    uint8_t rec = (esr >> 24) & 0xFF;
    uint8_t tec = (esr >> 16) & 0xFF;
    uint8_t lec = (esr >> 4) & 0x07;

    bool has_error = (esr & (CAN_ESR_BOFF | CAN_ESR_EPVF | CAN_ESR_EWGF)) != 0;

    if (has_error)
    {
        snprintf(buffer, sizeof(buffer),
                 "\r\n"
                 "================================================\r\n"
                 " [WARNING] CAN STATUS ALERT                    \r\n"
                 "================================================\r\n"
                 " State         : %d (0=Reset, 2=Listening)     \r\n"
                 " Error Code    : 0x%08lX                       \r\n"
                 " Free Mailbox  : %lu/3                         \r\n"
                 " RX Errors     : %d                            \r\n"
                 " TX Errors     : %d                            \r\n"
                 " Last Error    : %d                            \r\n",
                 state, error, tx_mailboxes_free, rec, tec, lec);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

        if (esr & CAN_ESR_BOFF)
        {
            snprintf(buffer, sizeof(buffer), " Status: BUS-OFF detected!\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        }

        if (esr & CAN_ESR_EPVF)
        {
            snprintf(buffer, sizeof(buffer), " Status: Error Passive\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        }

        if (esr & CAN_ESR_EWGF)
        {
            snprintf(buffer, sizeof(buffer), " Status: Error Warning (TEC=%d)\r\n", tec);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        }

        snprintf(buffer, sizeof(buffer),
                 "================================================\r\n\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                 "[CAN] Status OK - State: %d, TX Errors: %d, RX Errors: %d\r\n",
                 state, tec, rec);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }
}

static void attemptCANRecovery(void)
{
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "🔄 Attempting CAN recovery...\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    HAL_CAN_Stop(&CAN_BUS_LINE);
    HAL_Delay(200);

    HAL_CAN_DeInit(&CAN_BUS_LINE);
    HAL_Delay(100);

    MX_CAN1_Init();

    HAL_Delay(200);

    uint32_t esr = CAN_BUS_LINE.Instance->ESR;

    if (esr & (CAN_ESR_BOFF | CAN_ESR_EPVF))
    {
        snprintf(buffer, sizeof(buffer), "⚠️ Forcing error counter reset...\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

        HAL_Delay(500);
    }

    checkCANStatus();

    snprintf(buffer, sizeof(buffer), "✅ CAN recovery completed\r\n\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}

void readCanStart(void)
{
    CAN_RxHeaderTypeDef RxHeader;
    uint8_t RxData[8];
    uint32_t receivedValue = 0;

    uint32_t fifoLevel = HAL_CAN_GetRxFifoFillLevel(&CAN_BUS_LINE, CAN_RX_FIFO0);
    if (fifoLevel > 0)
    {
        HAL_StatusTypeDef status = HAL_CAN_GetRxMessage(&CAN_BUS_LINE, CAN_RX_FIFO0, &RxHeader, RxData);
        if (status == HAL_OK)
        {
            uint32_t m_ID;
            bool is_extended = (RxHeader.IDE == CAN_ID_EXT);

            if (is_extended)
            {
                m_ID = RxHeader.ExtId;
            }
            else
            {
                m_ID = RxHeader.StdId;
            }

            if (m_ID == CAN_ID_ASSIGNMENT_ID ||
                m_ID == CAN_MASTER_HEARTBEAT_ID ||
                m_ID == CAN_BROADCAST_ID ||
                m_ID == CAN_CENTRAL_COMMAND_ID)
            {
                handleDynamicIDProtocol(m_ID, RxData, RxHeader.DLC, is_extended);
                return;
            }

            if (RxHeader.DLC >= 4)
            {
                receivedValue = ((uint32_t)RxData[0] << 24) |
                               ((uint32_t)RxData[1] << 16) |
                               ((uint32_t)RxData[2] << 8) |
                               RxData[3];
            }

            parseIncomingValue(m_ID, receivedValue, RxData, RxHeader.DLC, is_extended);
        }
    }
}

static void handleMasterHeartbeat(uint8_t* data, uint8_t dlc)
{
    if (dlc < 3) return;

    uint8_t command = data[0];
    if (command != CMD_MASTER_HEARTBEAT) return;

    uint16_t incoming_master_id = ((uint16_t)data[1] << 8) | data[2];

    if (IS_ID_ASSIGNED() && incoming_master_id != g_master_id)
	{
		static uint8_t reject_count = 0;
		if (++reject_count >= 10) // Sadece 10 mesajda bir log bas, terminali doldurmasın
		{
			reject_count = 0;
			char buffer[128];
			snprintf(buffer, sizeof(buffer),
					 "[WARN] Ignored Master HB from 0x%04X (Locked to 0x%04lX)\r\n",
					 incoming_master_id, (unsigned long)g_master_id);
			HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
		}
		return; // İşlemi iptal et ve fonksiyondan çık
	}

    if (IS_ID_ASSIGNED() && incoming_master_id == g_master_id)
    {
        last_master_heartbeat_time = HAL_GetTick();

        static uint8_t hb_count = 0;
        if (++hb_count >= 10)
        {
            hb_count = 0;
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     "[MASTER HB] Received from 0x%04X\r\n",
                     incoming_master_id);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
        }
    }
}

static void checkMasterTimeout(void)
{
    if (!IS_ID_ASSIGNED()) return;

    uint32_t current_time = HAL_GetTick();

    if (last_master_heartbeat_time > 0 &&
        (current_time - last_master_heartbeat_time) > MASTER_HEARTBEAT_TIMEOUT_MS)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "\r\n"
                 "================================================\r\n"
                 " [MASTER TIMEOUT]                              \r\n"
                 "================================================\r\n"
                 " Locked Master : 0x%04lX                       \r\n"
                 " Timeout       : %lu ms                        \r\n"
                 " Action        : Waiting indefinitely...       \r\n"
                 "================================================\r\n\r\n",
                 (unsigned long)g_master_id,
                 (current_time - last_master_heartbeat_time));
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

        // Zamanlayıcıyı sıfırla ki sürekli aynı logu basmasın (her 15 sn'de bir uyarı verecek)
        last_master_heartbeat_time = current_time;

        // DİKKAT: eraseIDAndRestartAssignment() komutunu kaldırdık!
    }
}

static void handleDynamicIDProtocol(uint32_t m_ID, uint8_t* data, uint8_t dlc, bool is_extended)
{
    if (dlc < 1) return;

    uint8_t command = data[0];

    if (m_ID == CAN_MASTER_HEARTBEAT_ID)
    {
        handleMasterHeartbeat(data, dlc);
        return;
    }

    if (m_ID == CAN_ID_ASSIGNMENT_ID && command == CMD_ASSIGN_ID)
    {
        processIDAssignment(data, dlc);
        return;
    }

    if (dlc >= 5 && command == CMD_REASSIGN_REQUEST)
	{
		uint32_t node_id = ((uint32_t)data[1] << 24) |
						   ((uint32_t)data[2] << 16) |
						   ((uint32_t)data[3] << 8) |
						   data[4];

		if (node_id == g_node_unique_id)
		{
			// ---> YENİ EKLENEN GÜVENLİK DUVARI <---
			if (IS_ID_ASSIGNED())
			{
				char buffer[128];
				snprintf(buffer, sizeof(buffer),
						 "\r\n[SECURITY BLOCK] Remote wipe rejected! Locked to Master: 0x%04lX\r\n",
						 (unsigned long)g_master_id);
				HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
				return; // Silme işlemini iptal et ve fonksiyondan çık!
			}
			// ----------------------------------------

			char buffer[128];
			snprintf(buffer, sizeof(buffer),
					 "[REASSIGN] Request received! UID: 0x%08lX\r\n", node_id);
			HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

			eraseIDAndRestartAssignment();
		}
	}
}

static void processIDAssignment(uint8_t* data, uint8_t dlc)
{
    char buffer[256];

    if (dlc >= 8 && !g_assignment_part1_received)
    {
        snprintf(buffer, sizeof(buffer),
                 "[ASSIGN] Part 1/2 received (DLC=%d)\r\n", dlc);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

        uint32_t unique_id = ((uint32_t)data[1] << 24) |
                            ((uint32_t)data[2] << 16) |
                            ((uint32_t)data[3] << 8) |
                            data[4];

        if (unique_id != g_node_unique_id)
        {
            snprintf(buffer, sizeof(buffer),
                     "[ASSIGN] Not for me (Target:0x%08lX)\r\n", unique_id);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
            return;
        }

        memcpy(g_assignment_buffer, data, 8);
        g_assignment_part1_received = true;

        snprintf(buffer, sizeof(buffer),
                 "[ASSIGN] Part 1 buffered, waiting Part 2...\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
        return;
    }
    else if (dlc >= 5 && g_assignment_part1_received)
    {
        snprintf(buffer, sizeof(buffer),
                 "[ASSIGN] Part 2/2 received (DLC=%d)\r\n", dlc);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

        uint32_t unique_id = ((uint32_t)g_assignment_buffer[1] << 24) |
                            ((uint32_t)g_assignment_buffer[2] << 16) |
                            ((uint32_t)g_assignment_buffer[3] << 8) |
                            g_assignment_buffer[4];

        uint32_t assigned_can_id = ((uint32_t)g_assignment_buffer[5] << 24) |
                                   ((uint32_t)g_assignment_buffer[6] << 16) |
                                   ((uint32_t)g_assignment_buffer[7] << 8);

        assigned_can_id |= data[1];
        uint16_t new_master_id = ((uint16_t)data[2] << 8) | data[3];
        uint8_t new_room_id = data[4];

        if (unique_id != g_node_unique_id)
        {
            snprintf(buffer, sizeof(buffer),
                     "[ASSIGN] ERROR: UID mismatch in Part 2!\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
            g_assignment_part1_received = false;
            return;
        }

        if (IS_ID_ASSIGNED() && new_master_id != g_master_id)
		{
			snprintf(buffer, sizeof(buffer),
					"\r\n[ASSIGN REJECTED] Node is locked to Master:0x%04lX. Ignored Master:0x%04X\r\n",
					(unsigned long)g_master_id, new_master_id);
			HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
			g_assignment_part1_received = false;
			return;
		}

        g_my_assigned_id = assigned_can_id;
        g_my_room_id = new_room_id;
        g_my_node_offset = GET_NODE_ID(assigned_can_id);
        g_master_id = new_master_id;

        snprintf(buffer, sizeof(buffer),
                "\r\n========================================\r\n"
                "  LED NODE - ID ASSIGNMENT COMPLETE      \r\n"
                "========================================\r\n"
                "  Node UID    : 0x%08lX\r\n"
                "  CAN ID      : 0x%08lX\r\n"
                "  Room ID     : %d\r\n"
                "  Node Offset : %d\r\n"
                "  Master ID   : 0x%04lX\r\n"  // ← %04lX
                "========================================\r\n",
                g_node_unique_id,
                g_my_assigned_id,
                g_my_room_id,
                g_my_node_offset,
                (unsigned long)g_master_id);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

        HAL_StatusTypeDef flash_status = saveAssignedIDToFlash(
            g_my_assigned_id,
            g_master_id,
            g_my_room_id
        );

        if (flash_status != HAL_OK)
        {
            snprintf(buffer, sizeof(buffer),
                     "\r\n⚠️ WARNING: Flash save failed!\r\n\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        }

        g_node_state = NODE_STATE_ASSIGNED;
        g_assignment_part1_received = false;

        uint8_t confirm_data[8] = {0};
        confirm_data[0] = CMD_ID_CONFIRMATION;
        confirm_data[1] = (uint8_t)(g_node_unique_id >> 24);
        confirm_data[2] = (uint8_t)(g_node_unique_id >> 16);
        confirm_data[3] = (uint8_t)(g_node_unique_id >> 8);
        confirm_data[4] = (uint8_t)(g_node_unique_id);
        confirm_data[5] = (uint8_t)(g_my_assigned_id >> 24);
        confirm_data[6] = (uint8_t)(g_my_assigned_id >> 16);
        confirm_data[7] = (uint8_t)(g_my_assigned_id >> 8);

        sendCanMessage(CAN_ID_ASSIGNMENT_ID, confirm_data, 8, true);

        HAL_Delay(10);

        uint8_t confirm_data2[8] = {0};
        confirm_data2[0] = CMD_ID_CONFIRMATION;
        confirm_data2[1] = (uint8_t)(g_my_assigned_id);

        sendCanMessage(CAN_ID_ASSIGNMENT_ID, confirm_data2, 2, true);

        g_node_state = NODE_STATE_ACTIVE;
        last_heartbeat_time = HAL_GetTick();
        last_master_heartbeat_time = HAL_GetTick();

        sendHeartbeat();

        snprintf(buffer, sizeof(buffer),
                 " Node State: ACTIVE\r\n"
                 " First heartbeat sent\r\n"
                 "========================================\r\n\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                 "[ASSIGN] ERROR: Invalid message! DLC:%d, Part1:%d\r\n",
                 dlc, g_assignment_part1_received);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

        if (g_assignment_part1_received)
        {
            snprintf(buffer, sizeof(buffer),
                     "[ASSIGN] Resetting Part 1 flag\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
            g_assignment_part1_received = false;
        }
    }
}

static void requestNodeID(void)
{
    uint8_t request_data[8] = {0};
    request_data[0] = CMD_REQUEST_ID;
    request_data[1] = (uint8_t)(g_node_unique_id >> 24);
    request_data[2] = (uint8_t)(g_node_unique_id >> 16);
    request_data[3] = (uint8_t)(g_node_unique_id >> 8);
    request_data[4] = (uint8_t)(g_node_unique_id);
    request_data[5] = MY_NODE_TYPE;

    sendCanMessage(CAN_ID_REQUEST_ID, request_data, 6, true);

    g_node_state = NODE_STATE_REQUESTING;
    id_request_time = HAL_GetTick();

    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "Requesting ID... (UID:0x%08lX, Type:LED)\r\n",
             g_node_unique_id);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}

static void sendHeartbeat(void)
{
    if (!IS_ID_ASSIGNED()) return;

    uint8_t heartbeat_data[8] = {0};

    heartbeat_data[0] = CMD_HEARTBEAT;
    heartbeat_data[1] = (uint8_t)(g_node_unique_id >> 24);
    heartbeat_data[2] = (uint8_t)(g_node_unique_id >> 16);
    heartbeat_data[3] = (uint8_t)(g_node_unique_id >> 8);
    heartbeat_data[4] = (uint8_t)(g_node_unique_id);
    heartbeat_data[5] = (uint8_t)((g_master_id >> 8) & 0xFF);
    heartbeat_data[6] = (uint8_t)(g_master_id & 0xFF);

    sendCanMessage(g_my_assigned_id, heartbeat_data, 7, true);

    static uint8_t hb_log_counter = 0;
    if (++hb_log_counter >= 5)
    {
        hb_log_counter = 0;
        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                 "[HB] TX - ID:0x%08lX | Room:%d | Master:0x%04lX\r\n",
                 g_my_assigned_id, g_my_room_id, (unsigned long)g_master_id);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
    }
}

void canDynamicIDTask(void)
{
    uint32_t current_time = HAL_GetTick();

    if (current_time - last_can_status_check > 60000)
    {
        checkCANStatus();
        last_can_status_check = current_time;
    }

    if (g_assignment_part1_received)
    {
        static uint32_t part1_time = 0;

        if (part1_time == 0)
        {
            part1_time = current_time;
        }

        if ((current_time - part1_time) > 1000)
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     "[WARN] Assignment Part 2 timeout! Resetting.\r\n");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

            g_assignment_part1_received = false;
            part1_time = 0;
        }
    }

    checkMasterTimeout();

    switch (g_node_state)
    {
        case NODE_STATE_UNASSIGNED:
            requestNodeID();
            break;

        case NODE_STATE_REQUESTING:
            if (current_time - id_request_time > ID_REQUEST_TIMEOUT_MS)
            {
                id_request_retry_count++;
                if (id_request_retry_count < 5)
                {
                    char buffer[128];
                    snprintf(buffer, sizeof(buffer),
                             "[RETRY] ID Request timeout (%d/5)\r\n",
                             id_request_retry_count);
                    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

                    g_assignment_part1_received = false;
                    requestNodeID();
                }
                else
                {
                    attemptCANRecovery();
                    id_request_retry_count = 0;
                    g_assignment_part1_received = false;
                    HAL_Delay(1000);
                    g_node_state = NODE_STATE_UNASSIGNED;
                }
            }
            break;

        case NODE_STATE_ACTIVE:
            if (current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL_MS)
            {
                sendHeartbeat();
                last_heartbeat_time = current_time;
            }
            break;

        default:
            g_node_state = NODE_STATE_UNASSIGNED;
            break;
    }
}

// ========== sendCanMessage() - DÜZELTİLDİ: use_extended parametresi eklendi ==========
void sendCanMessage(uint32_t m_canID, uint8_t* data, uint8_t length, bool use_extended)
{
    CAN_TxHeaderTypeDef txHeader;
    uint32_t mailBox;
    char buffer[256];

    HAL_CAN_StateTypeDef canState = HAL_CAN_GetState(&CAN_BUS_LINE);
    uint32_t esr = CAN_BUS_LINE.Instance->ESR;

    if (esr & CAN_ESR_BOFF)
    {
        snprintf(buffer, sizeof(buffer), "CAN Bus-Off! TX Aborted.\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        return;
    }

    if (canState == HAL_CAN_STATE_RESET || canState == HAL_CAN_STATE_ERROR)
    {
        snprintf(buffer, sizeof(buffer), "CAN in bad state: %d\r\n", canState);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        attemptCANRecovery();
        return;
    }

    uint32_t wait_start = HAL_GetTick();
    uint32_t timeout = 500;

    while (HAL_CAN_GetTxMailboxesFreeLevel(&CAN_BUS_LINE) == 0)
    {
        if (HAL_GetTick() - wait_start > timeout)
        {
            snprintf(buffer, sizeof(buffer),
                     "TX Mailbox timeout - ID: 0x%08lX\r\n",
                     m_canID);
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

            HAL_CAN_AbortTxRequest(&CAN_BUS_LINE, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            return;
        }
        HAL_Delay(1);
    }

    // ← DÜZELTİLDİ: Extended/Standard ID seçimi
    if (use_extended || m_canID > 0x7FF)
    {
        txHeader.ExtId = m_canID;
        txHeader.IDE = CAN_ID_EXT;
        txHeader.StdId = 0;
    }
    else
    {
        txHeader.StdId = m_canID;
        txHeader.IDE = CAN_ID_STD;
        txHeader.ExtId = 0;
    }

    txHeader.DLC = length;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.TransmitGlobalTime = DISABLE;

    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(&CAN_BUS_LINE, &txHeader, data, &mailBox);

    if(status != HAL_OK)
    {
        uint32_t error = HAL_CAN_GetError(&CAN_BUS_LINE);
        snprintf(buffer, sizeof(buffer),
                 "TX FAILED - ID:0x%08lX %s Status:%d Error:0x%lX\r\n",
                m_canID,
                use_extended ? "Ext" : "Std",
                status, error);
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        checkCANStatus();
    }
}

void sendCanHeader(uint32_t m_canID, uint32_t m_value)
{
    uint8_t canData[8] = {0};
    canData[0] = (uint8_t)(m_value >> 24);
    canData[1] = (uint8_t)(m_value >> 16);
    canData[2] = (uint8_t)(m_value >> 8);
    canData[3] = (uint8_t)(m_value);

    bool use_extended = (m_canID > 0x7FF);
    sendCanMessage(m_canID, canData, 8, use_extended);
}

static void parseIncomingValue(uint32_t m_ID, uint32_t m_value, uint8_t* data, uint8_t dlc, bool is_extended)
{
    if (!IS_ID_ASSIGNED()) return;

    char buffer[128];

    if (is_extended && m_ID != CAN_BROADCAST_ID)
    {
        uint8_t target_room = GET_ROOM_ID(m_ID);
        uint8_t device_type = GET_DEVICE_TYPE(m_ID);
        uint16_t node_id = GET_NODE_ID(m_ID);

        if (target_room != MY_ROOM_ID && target_room != ROOM_BROADCAST)
        {
            return;
        }

        if (device_type != DEVICE_TYPE_LED && device_type != DEVICE_ALL)
        {
            return;
        }

        if (node_id != MY_NODE_OFFSET && node_id != NODE_ALL)
        {
            return;
        }
    }
    else if (!is_extended)
    {
        if (m_ID != g_my_assigned_id && m_ID != CAN_BROADCAST_ID)
        {
            return;
        }
    }

    if (dlc >= 2)
    {
        uint8_t command = data[0];

        if (command == LED_CMD_TOGGLE || command == LED_CMD_SET || command == LED_CMD_SET_BRIGHTNESS)
        {
            snprintf(buffer, sizeof(buffer),
                     "[CMD] LED (0x%02X) from Room:%d %s\r\n",
                     command, MY_ROOM_ID,
                     is_extended ? "Ext" : "Std");
            HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);

            ledControl_processCommand(data, dlc);
            return;
        }
    }
}

static uint32_t readMasterIDFromFlash(void)
{
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    uint32_t magic = flash_base[2];

    if (magic == FLASH_MAGIC_NUMBER)
    {
        return flash_base[1];
    }

    return 0;
}

static uint32_t readAssignedIDFromFlash(void)
{
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    uint32_t magic = flash_base[2];

    if (magic == FLASH_MAGIC_NUMBER)
    {
        return flash_base[0];
    }

    return 0;
}

static uint8_t readRoomIDFromFlash(void)
{
    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    uint32_t magic = flash_base[2];

    if (magic == FLASH_MAGIC_NUMBER)
    {
        return (uint8_t)flash_base[3];
    }

    return 0;
}

static HAL_StatusTypeDef saveAssignedIDToFlash(uint32_t assigned_id, uint32_t master_id, uint8_t room_id)
{
    char buffer[256];
    uint32_t pageError = 0;

    snprintf(buffer, sizeof(buffer),
             "\r\n Starting Flash Save...\r\n"
             "   CAN ID  : 0x%08lX\r\n"
             "   Master  : 0x%04lX\r\n"  // ← %04lX
             "   Room    : %d\r\n",
             assigned_id, (unsigned long)master_id, room_id);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = ID_STORAGE_ADDRESS;
    EraseInitStruct.NbPages     = 1;

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status != HAL_OK) return HAL_ERROR;

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &pageError);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    HAL_Delay(10);

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS, assigned_id);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 4, master_id);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 8, FLASH_MAGIC_NUMBER);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ID_STORAGE_ADDRESS + 12, (uint32_t)room_id);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    HAL_FLASH_Lock();
    HAL_Delay(100);

    uint32_t *flash_base = (uint32_t*)ID_STORAGE_ADDRESS;
    uint32_t verify_can_id = flash_base[0];
    uint32_t verify_master_id = flash_base[1];
    uint32_t verify_magic = flash_base[2];
    uint32_t verify_room_id = flash_base[3];

    if (verify_can_id != assigned_id ||
        verify_master_id != master_id ||
        verify_magic != FLASH_MAGIC_NUMBER ||
        verify_room_id != room_id)
    {
        snprintf(buffer, sizeof(buffer), "\r\n❌ FLASH VERIFICATION FAILED!\r\n\r\n");
        HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
        return HAL_ERROR;
    }

    snprintf(buffer, sizeof(buffer), "\r\n✅ FLASH SAVE SUCCESSFUL!\r\n\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    return HAL_OK;
}

static void eraseIDAndRestartAssignment(void)
{
    uint32_t pageError = 0;
    char buffer[256];

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = ID_STORAGE_ADDRESS;
    EraseInitStruct.NbPages      = 1;

    snprintf(buffer, sizeof(buffer),
             "\r\n🔄 MASTER CONNECTION LOST - RESET IN PROGRESS\r\n"
             "   Erasing Flash...\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&EraseInitStruct, &pageError);
    HAL_FLASH_Lock();

    g_my_assigned_id = 0;
    g_my_room_id = 0;
    g_my_node_offset = 0;
    g_master_id = 0;
    g_node_state = NODE_STATE_UNASSIGNED;
    id_request_retry_count = 0;
    last_master_heartbeat_time = 0;
    g_assignment_part1_received = false;

    snprintf(buffer, sizeof(buffer),
             "   State: UNASSIGNED\r\n"
             "   Searching for new master...\r\n\r\n");
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}
