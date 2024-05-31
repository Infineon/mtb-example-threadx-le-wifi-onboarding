/******************************************************************************
* File Name: wifi_task.h
*
* Description: This is the header file for wifi_task.c. It contains macros,
* enums and structures used by the functions in wifi_task.c. It also contains
* function prototypes and externs of global variables that can be used by
* other files
*
* Related Document: See README.md
*
*******************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__

#include "cy_wcm.h"
#include "wiced_app_pre_init_cfg.h"
#include "cyabs_rtos.h"

/******************************************************************************
 *                                Constants
 ******************************************************************************/

/* Maximum number of connection retries to the Wi-Fi network. */
#define MAX_CONNECTION_RETRIES            (3)

/* Task parameters for WiFi tasks */
#define WIFI_TASK_STACK_SIZE                    (8192)
#define NOTIFY_TASK_STACK_SIZE                  (4096)

/* Macros defining the packet type */
#define DATA_PACKET_TYPE_SSID (1u)
#define DATA_PACKET_TYPE_PASSWORD (2u)

/* Macros defining packet type for scan data */
#define SCAN_PACKET_TYPE_SSID (1u)
#define SCAN_PACKET_TYPE_SECURITY (2u)
#define SCAN_PACKET_TYPE_RSSI (3u)

/* Macros defining the commands for WiFi control point characteristic */
#define WIFI_CONTROL_DISCONNECT (0u)
#define WIFI_CONTROL_CONNECT (1u)
#define WIFI_CONTROL_SCAN (2u)

/* Task notification value to indicate whether to use
 * WiFi credentials from EMEEPROM or from GATT DB
 */
enum wifi_task_notifications
{
    NOTIF_SCAN               = 0x0001,
    NOTIF_CONNECT            = 0x0002,
    NOTIF_DISCONNECT         = 0x0004,
    NOTIF_ERASE_DATA         = 0x0008,
    NOTIF_GATT_NOTIFICATION  = 0x0010
};

/******************************************************************************
 *                                Structures
 ******************************************************************************/
/* Structure to store WiFi details that goes into KV Store */
//__packed struct
typedef STRUCT_PACKED
{
    uint8_t wifi_ssid[CY_WCM_MAX_SSID_LEN];
    uint8_t ssid_len;
    uint8_t wifi_password[CY_WCM_MAX_PASSPHRASE_LEN];
    uint8_t password_len;
}wifi_details_t;

typedef STRUCT_PACKED
{
    cy_wcm_ssid_t                SSID;             /**< SSID (i.e., name of the AP). In case of a hidden AP, SSID.value will be empty and SSID.length will be 0. */
    cy_wcm_security_t            security;         /**< Security type.                                 */
} scan_result_t;

/******************************************************************************
 *                              Extern Variables
 ******************************************************************************/
extern wifi_details_t wifi_details;
extern cy_wcm_connect_params_t wifi_conn_param;
extern cy_queue_t xScanResultQueue;

/* Task Handles for WiFi Task */
extern cy_thread_t wifi_task_pointer;

/* Maintains the connection id of the current connection */
extern uint16_t conn_id;

/* This variable is set to true when button callback is received and
 * data is present in NVRAM. It is set to false after the WiFi Task
 * processes Disconnection notification. It is used to check button
 * interrupt while the device is trying to connect to WiFi
 */
extern volatile bool button_pressed;
/******************************************************************************
 *                              Function Prototypes
 ******************************************************************************/
void wifi_task(cy_thread_arg_t pvParameters);
void scan_notify_task(cy_thread_arg_t pvParameters);
void scan_callback(cy_wcm_scan_result_t *result_ptr, void *user_data,
                   cy_wcm_scan_status_t status);
/* __WIFI_TASK_H__ */


#endif
/* [] END OF FILE */
