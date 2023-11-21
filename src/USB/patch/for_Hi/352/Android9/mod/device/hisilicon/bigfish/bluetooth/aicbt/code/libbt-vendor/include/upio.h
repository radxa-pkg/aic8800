/******************************************************************************
 *
 *  Copyright (C) 2019-2021 Aicsemi Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Filename:      upio.h
 *
 *  Description:   Contains definitions used for I/O controls
 *
 ******************************************************************************/

#ifndef UPIO_H
#define UPIO_H

/******************************************************************************
**  Constants & Macros
******************************************************************************/

#define UPIO_BT_POWER_OFF 0
#define UPIO_BT_POWER_ON  1

/* UPIO signals */
enum {
    UPIO_BT_WAKE = 0,
    UPIO_HOST_WAKE,
    UPIO_LPM_MODE,
    UPIO_MAX_COUNT
};

/* UPIO assertion/deassertion */
enum {
    UPIO_UNKNOWN = 0,
    UPIO_DEASSERT,
    UPIO_ASSERT
};

/******************************************************************************
**  Extern variables and functions
******************************************************************************/

/******************************************************************************
**  Functions
******************************************************************************/

/*******************************************************************************
**
** Function        upio_init
**
** Description     Initialization
**
** Returns         None
**
*******************************************************************************/
void upio_init(void);

/*******************************************************************************
**
** Function        upio_cleanup
**
** Description     Clean up
**
** Returns         None
**
*******************************************************************************/
void upio_cleanup(void);

/*******************************************************************************
**
** Function        upio_set_bluetooth_power
**
** Description     Interact with low layer driver to set Bluetooth power
**                 on/off.
**
** Returns         0  : SUCCESS or Not-Applicable
**                 <0 : ERROR
**
*******************************************************************************/
int upio_set_bluetooth_power(int on);

/*******************************************************************************
**
** Function        upio_set
**
** Description     Set i/o based on polarity
**
** Returns         None
**
*******************************************************************************/
void upio_set(uint8_t pio, uint8_t action, uint8_t polarity);


/*******************************************************************************
**
** Function        bt_wake_up_host_mode_set
**
** Description     To enable/disable bt_wake_up_host mode.
**
** Returns         0  : SUCCESS or Not-Applicable
**                 <0 : ERROR
**
*******************************************************************************/
int bt_wake_up_host_mode_set(uint8_t mode);
#endif /* UPIO_H */
