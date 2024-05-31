/*******************************************************************************
 * File Name:   wifi_task.c
 *
 * Description: This file contains the task definition for initializing the
 * Wi-Fi device, connecting to the AP, disconnecting from AP, scanning and
 * NVRAM related functionality.
 *
 * Related Document: See Readme.md
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
#include "cybsp.h"
#include "stdio.h"
#include "wifi_task.h"
#include "cycfg_gatt_db.h"
#include "wiced_bt_stack.h"
#include "app_utils.h"
#include "cycfg_gap.h"
#include "wiced_memory.h"

/******************************************************************************
 *                             Global Variables
 ******************************************************************************/
/* WCM related structures */
cy_wcm_connect_params_t wifi_conn_param;

/* Variable to be used to store the WiFi credentials which can be copied to
 * NVRAM
 */
wifi_details_t wifi_details;

/* Notification values received from other tasks */
uint32_t ulNotifiedValue = NOTIF_DISCONNECT;


/*******************************************************************************
* Function Name: scan_notify_task
********************************************************************************
* Summary:
* This function prints the scan results and sends notifications to the Bluetooth  
* LE central device
*
*******************************************************************************/
void scan_notify_task(cy_thread_arg_t arg)
{
    uint8_t *scan_data;
    uint8_t byte_no = 0;
    uint8_t ssid_len = 0;
    scan_result_t result_ptr;
    uint8_t security_len = 4;
    /* Scan packet type length of SSID and security */
    uint8_t scan_packet_type_len = 1;


    for (;;)
    {
        if (CY_RSLT_SUCCESS == cy_rtos_queue_get(&xScanResultQueue, &result_ptr, CY_RTOS_NEVER_TIMEOUT))
        {
            if(strlen((const char *)result_ptr.SSID) != 0)
            {
                ssid_len = strlen((const char *)result_ptr.SSID);
                wifi_conn_param.ap_credentials.security = result_ptr.security;
                printf("SSID: %-32s\t", result_ptr.SSID);
                printf("Security: %s\n", get_wifi_security_name(result_ptr.security));

                /* If scan bit is set then send the scan result to the BLE Client */
                if(NOTIF_SCAN & ulNotifiedValue)
                {
                    /* Get memory for the buffer to send the scan data */
                    /* Length of SCAN_PACKET_TYPE_SSID + size of ssid_len + ssid_len +
                     * Length of SCAN_PACKET_TYPE_SECURITY + size of security_len + security_len
                     */
                    scan_data = (uint8_t *)app_alloc_buffer(scan_packet_type_len +
                            sizeof(ssid_len) + ssid_len + scan_packet_type_len
                            + sizeof(security_len) + security_len);

                    if(NULL == scan_data)
                    {
                        printf("Buffer for notification not allocated\n");
                        return;
                    }
                    /*Prepare the packet in Type, Length, Value format*/
                    /* Fill in the SSID first */
                    scan_data[byte_no++] = SCAN_PACKET_TYPE_SSID;
                    scan_data[byte_no++] = ssid_len;
                    memcpy(&scan_data[byte_no], (const char *)result_ptr.SSID, ssid_len);
                    byte_no = byte_no + ssid_len;

                    /* Fill in the 4 bytes of security value */
                    /*Packet type*/
                    scan_data[byte_no++] = SCAN_PACKET_TYPE_SECURITY;
                    //Security data size
                    scan_data[byte_no++] = security_len;
                    scan_data[byte_no++] = (uint8_t)(result_ptr.security & 0x000000ff);
                    scan_data[byte_no++] = (uint8_t)((result_ptr.security &
                            0x0000ff00) >> 8);
                    scan_data[byte_no++] = (uint8_t)((result_ptr.security &
                            0x00ff0000) >> 16);
                    scan_data[byte_no++] = (uint8_t)((result_ptr.security &
                            0xff000000) >> 24);

                    if ((conn_id != 0) &&
                            (app_custom_service_wifi_networks_client_char_config[0] &
                                    GATT_CLIENT_CONFIG_NOTIFICATION))
                    {
                        wiced_bt_gatt_server_send_notification(conn_id,
                                HDLC_CUSTOM_SERVICE_WIFI_NETWORKS_VALUE, byte_no, scan_data,
                                (wiced_bt_gatt_app_context_t)app_free_buffer);
                    }
                    else
                    {

                        app_free_buffer(scan_data);
                    }
                }
            }
        }
    }
}
/*******************************************************************************
* Function Name: wifi_task
********************************************************************************
* Summary:
* This function initializes the WCM module and handles task notifications
*
*******************************************************************************/
void wifi_task(cy_thread_arg_t arg)
{
    cy_rslt_t result;

    /* Variable to use for filtering WiFi scan results */
    cy_wcm_scan_filter_t scan_filter;

    /* WCM configuration and IP address variables */
    cy_wcm_config_t wifi_config;
    cy_wcm_ip_address_t ip_address;

    wifi_config.interface = CY_WCM_INTERFACE_TYPE_STA;

    /* Initialize WCM */
    result = cy_wcm_init(&wifi_config);
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Failed to initialize Wi-Fi\n");
        CY_ASSERT(0);
    }
    else
    {
        printf("Initializing wcm is successful \n");
    }

    while (true)
    {
        cy_rtos_thread_wait_notification(CY_RTOS_NEVER_TIMEOUT);

        /* Check if the notification value has the scan bit set. In some cases both
         * the scan bit and connect bit are set. In both the cases we start with
         * the scan, pass the notification value to the scan callback and if
         * the connect bit is also set then the scan callback will pass the
         * connect message to this task and this task will try and connect to
         * the WiFi network. If the notification value passed to the scan
         * callback contains only the scan bit then it will try and send
         * GATT notifications to the BLE client
         */
        if (NOTIF_SCAN & ulNotifiedValue)
        {
            memset(&scan_filter, 0, sizeof(cy_wcm_scan_filter_t));

            /* If the Connect bit is set then we need to only scan for networks
             * with the given SSID. Set the correct filter values and start scan
             */
            if (NOTIF_CONNECT & ulNotifiedValue)
            {
                /* Configure the scan filter for SSID */
                scan_filter.mode = CY_WCM_SCAN_FILTER_TYPE_SSID;
                memcpy((char *)scan_filter.param.SSID, (char *)wifi_conn_param.ap_credentials.SSID,
                strlen((char *)wifi_conn_param.ap_credentials.SSID) + 1);
                printf("Starting scan with SSID: %s\n", scan_filter.param.SSID);
                result = cy_wcm_start_scan(scan_callback, &ulNotifiedValue,
                                           &scan_filter);

                if (CY_RSLT_SUCCESS != result)
                {
                    printf("Start scan failed\n");
                }
            }
            /* Else start scan without any filter */
            else
            {
                result = cy_wcm_start_scan(scan_callback, &ulNotifiedValue, NULL);
            }

            if (CY_RSLT_SUCCESS != result)
            {
                printf("Start scan failed\n");
            }
        }

        /* Check for connection notification */
        else if (NOTIF_CONNECT == ulNotifiedValue)
        {
            /* Variable to track the number of connection retries to the Wi-Fi
             * AP
             */
            uint8_t conn_retries = 0;

            /* Join the Wi-Fi AP */
            result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);
            while ((result != CY_RSLT_SUCCESS) && (conn_retries <
                                                        MAX_CONNECTION_RETRIES))
            {
                /* If button ISR is received then skip connecting to WiFi */
                if (button_pressed)
                {
                    break;
                }
                printf("\nTrying to connect SSID: %s, Password: %s\n",
                        wifi_conn_param.ap_credentials.SSID,
                        wifi_conn_param.ap_credentials.password);
                /* Connect to the given AP */
                result = cy_wcm_connect_ap(&wifi_conn_param, &ip_address);
                conn_retries++;
            }

            if (result == CY_RSLT_SUCCESS)
            {
                printf("Successfully joined the Wi-Fi network\n");
                /* Update GATT DB about connection */
                app_custom_service_wifi_control[0] = WIFI_CONTROL_CONNECT;

                /* Check if the connection is active and  notifications are
                 * enabled and then send the notification
                 */
                if ((conn_id != 0) &&
                   ((app_custom_service_wifi_control_client_char_config[0] &
                     GATT_CLIENT_CONFIG_NOTIFICATION)))
                {
                    uint8_t *p_attr = (uint8_t *)app_custom_service_wifi_control;
                    wiced_bt_gatt_server_send_notification(conn_id,
                                    HDLC_CUSTOM_SERVICE_WIFI_CONTROL_VALUE,
                                    sizeof(app_custom_service_wifi_control[0]),
                                    p_attr, NULL);
                }
                else /* Notification not sent */
                {
                    printf("Notification not sent\n");
                }
            }
            else /* WiFi connection failed */
            {
                /* Update GATT DB about unsuccessful connection */
                app_custom_service_wifi_control[0] = WIFI_CONTROL_DISCONNECT;

                /* Send notification for unsuccessful connection */
                /* Check if the connection is active and notifications are
                 * enabled and then send the notification
                 */
                if ((conn_id != 0) &&
                    ((app_custom_service_wifi_control_client_char_config[0] &
                      GATT_CLIENT_CONFIG_NOTIFICATION)))
                {
                    uint8_t *p_attr = (uint8_t *)app_custom_service_wifi_control;
                    wiced_bt_gatt_server_send_notification(conn_id,
                                     HDLC_CUSTOM_SERVICE_WIFI_CONTROL_VALUE,
                                     sizeof(app_custom_service_wifi_control[0]),
                                     p_attr, NULL);
                }
                else /* Notification not sent */
                {
                    printf("Notification not sent\n");
                }

                printf("Failed to join Wi-Fi network\n");
                if ((0 == conn_id) && (BTM_BLE_ADVERT_OFF ==
                                      wiced_bt_ble_get_current_advert_mode()))
                {
                    printf("Starting BLE ADV. Connect to BLE and provide"
                            "proper credentials\n");
                    /* Set the advertising params and make the device
                     * discoverable
                     */
                    result = wiced_bt_ble_set_raw_advertisement_data(
                                                     CY_BT_ADV_PACKET_DATA_SIZE,
                                                     cy_bt_adv_packet_data);
                    if (WICED_SUCCESS != result)
                    {
                        printf("Set ADV data failed\n");
                    }

                    wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_LOW,
                                                  0, NULL);
                }
            }
        }
        /* Task notification for disconnection */
        else if (NOTIF_DISCONNECT & ulNotifiedValue)
        {
            /* Check if the erase bit is also set in the notification. If yes
             * then erase the data from MVRAM, to be supported in next release.
             */
            if (NOTIF_ERASE_DATA & ulNotifiedValue)
            {
                printf("Deleting Wi-Fi data\n");
                /* Set the data to 0*/
                memset(&wifi_details, 0, sizeof(wifi_details));
            }

            printf("Disconnecting Wi-Fi\n");
            result = cy_wcm_disconnect_ap();
            if (result == CY_RSLT_SUCCESS)
            {
                /* Update GATT DB about disconnection */
                app_custom_service_wifi_control[0] = WIFI_CONTROL_DISCONNECT;

                printf("Successfully disconnected from AP\n");
                /* Start ADV if the device is not already connected or
                 * advertising
                */
                if ((0 == conn_id) && (BTM_BLE_ADVERT_OFF ==
                                      wiced_bt_ble_get_current_advert_mode()))
                {
                    printf("Starting BLE ADV. Connect to BLE and provide "
                            "proper credentials\n");
                    result = wiced_bt_start_advertisements(
                                                  BTM_BLE_ADVERT_UNDIRECTED_LOW,
                                                  0, NULL);

                    if (WICED_SUCCESS != result)
                    {
                        printf("Failed to start ADV");
                    }
                }
                else /* Connection is active */
                {
                    /* Send notification for successful disconnection */
                    /* Check if the connection is active and notifications are
                     * enabled
                     */
                    if ((conn_id != 0) &&
                        (app_custom_service_wifi_control_client_char_config[0] &
                         GATT_CLIENT_CONFIG_NOTIFICATION))
                    {
                        uint8_t *p_attr = (uint8_t *)app_custom_service_wifi_control;
                        result = wiced_bt_gatt_server_send_notification(conn_id,
                                     HDLC_CUSTOM_SERVICE_WIFI_CONTROL_VALUE,
                                     sizeof(app_custom_service_wifi_control[0]),
                                     p_attr, NULL);

                        if (CY_RSLT_SUCCESS != result)
                        {
                            printf("Start scan failed\n");
                        }
                    }
                    else /* Notification not sent */
                    {
                        printf("Notification not sent\n");
                    }
                }
            }
            else /* Disconnection failed */
            {
                printf("Failed to disconnect\n");
            }

            /* Set the variable as false as the disconnection was done */
            button_pressed = false;
        }
    }
}

/*******************************************************************************
 * Function Name: scan_callback
 *******************************************************************************
 * Summary: The callback function which accumulates the scan results.
 * If the CONNECT bit is set in the user_data then send back task notification
 * to the WiFi task to connect to the AP. If not then send the results back to
 * the BLE Client
 *
 * Parameters:
 *  cy_wcm_scan_result_t *result_ptr: Pointer to the scan result
 *  void *user_data: User data.
 *  cy_wcm_scan_status_t status: Status of scan completion.
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void scan_callback(cy_wcm_scan_result_t *result_ptr, void *user_data,
                   cy_wcm_scan_status_t status)
{
    cy_rslt_t result;
    scan_result_t scan_result;
    uint8_t ssid_len = 0;

    if (status == CY_WCM_SCAN_INCOMPLETE)
    {
        ssid_len = strlen((const char *)result_ptr->SSID);
        memcpy(scan_result.SSID, (const char *)result_ptr->SSID, ssid_len+1);
        scan_result.security=result_ptr->security;
        cy_rtos_queue_put(&xScanResultQueue, &scan_result, 0);
    }

    /* If the connect bit is set in the user data then connect notification to
     * the WiFi task
     */
    else if ((CY_WCM_SCAN_COMPLETE == status))
    {
        printf("Scan Complete \r\n");

        if (((NOTIF_SCAN | NOTIF_CONNECT) == *(uint32_t *)user_data))
        {
            ulNotifiedValue = NOTIF_CONNECT;
            result = cy_rtos_thread_set_notification(&wifi_task_pointer);
            if (result != CY_RSLT_SUCCESS)
            {
                printf("set thread notification failed \r\n");
            }
        }
    }
}

/* [] END OF FILE */
