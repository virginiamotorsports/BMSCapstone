
/* USER CODE BEGIN (0) */

/*
 * CONNECTIONS BETWEEN BQ79616EVM AND LAUNCH XL2 (TMS570LS1224)::
 * bq79616EVM J3 pin 1 (GND)    -> LAUNCH XL2 J3 pin 2 (GND)
 * bq79616EVM J3 pin 2 (NFAULT) -> LAUNCH XL2 J2 pin 5 (PA7)
 * bq79616EVM J3 pin 3 (NC)     -> FLOAT
 * bq79616EVM J3 pin 4 (RX)     -> LAUNCH XL2 J2 pin 4 (UATX)
 * bq79616EVM J3 pin 5 (TX)     -> LAUNCH XL2 J2 pin 3 (UARX)
 * bq79616EVM J3 pin 6 (NC)     -> FLOAT
 *
 * RELEVANT MODIFIED FILES:
 * bq79606.h        must change TOTALBOARDS and MAXcharS here for code to function
 * bq79606.c        contains all relevant functions used in the sample code
 * notification.c   sets UART_RX_RDY and RTI_TIMEOUT when their respective interrupts happen
 * .dil/.hcg        used for generating the basic TMS570LS1224 code files, can be used to make changes to the microcontroller
 */

/* USER CODE END */

/* Include Files */

// #include "sys_common.h"

/* USER CODE BEGIN (1) */
// #include <Arduino.h>
// extern UART Serial1;

#include "bq79616.hpp"
#include <math.h>
#include <stdio.h>
#include <WiFiS3.h>
#include <Arduino_CAN.h>
#include "can_helpers.hpp"
#include "BMSFSM.hpp"

// #include "Arduino.h"
/* USER CODE END */

/** @fn void main(void)
 *   @brief Application main function
 *   @note This function is empty by default.
 *
 *   This function is called after startup.
 *   The user can use this function to implement the application.
 */

/* USER CODE BEGIN (2) */
int UART_RX_RDY = 0;
int RTI_TIMEOUT = 0;

#define SECRET_SSID "BMS_WIFI"
#define SECRET_PASS "batteryboyz"



///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiUDP udp_sender;
char UDP_Buffer[200];
uint16_t cell_voltages[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void setup()
{

  // WiFi.config(IPAddress(192,168,137,2), IPAddress(8,8,8,8), IPAddress(192,168,137,1), IPAddress(255,255,255,0));

  Serial.begin(9600);
  Serial1.begin(BAUDRATE, SERIAL_8N1);
  CAN.begin(CanBitRate::BR_1000k);

  Serial.print("Hello this the the BMS Code\r\n");
  // HWRST79616();
  // delay(15);

  Wake79616();
  delayMicroseconds((10000 + 520) * TOTALBOARDS); // 2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE
  // printConsole("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\rBeginning Program\n\r");
  // INITIALIZE BQ79616-Q1
  
  // Wake79616();
  // delayMicroseconds((10000 + 520) * TOTALBOARDS); // 2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

  AutoAddress();
  // AutoAddress2();
  Serial.println("Autoaddress Completed");

  delayMicroseconds(4000 - (2200 + 520)); // 4ms total required after shutdown to wake transition for AFE settling time, this is for top device only
  WriteReg(0, FAULT_MSK2, 0x40, 1, FRMWRT_ALL_W); //OPTIONAL: MASK CUST_CRC SO CONFIG CHANGES DON'T FLAG A FAULT
  WriteReg(0, FAULT_MSK1, 0xFFFE, 2, FRMWRT_ALL_W); // INITIAL B0 SILICON: MASK FAULT_PWR SO TSREF_UV doesn't flag a fault
  ResetAllFaults(0, FRMWRT_ALL_W);

  // VARIABLES

  // ARRAYS (MUST place out of loop so not re-allocated every iteration)

  // ENABLE TSREF
  WriteReg(0, CONTROL2, 0x01, 1, FRMWRT_ALL_W); // enable TSREF
  delay(10);                                  // wait for TSREF to fully enable on all boards

  // CONFIGURE GPIO1 as CS ADC conversion toggle function
  WriteReg(0, GPIO_CONF2, 0x40, 1, FRMWRT_ALL_W); // GPIO1 as CS ADC conversion toggle function

  // CONFIGURE THE MAIN ADC
  WriteReg(0, ACTIVE_CELL, ACTIVECHANNELS - 6, 1, FRMWRT_ALL_W); // set all cells to active
  WriteReg(0, ADC_CONF1, 0x04, 1, FRMWRT_ALL_W);                 // LPF_ON - LPF = 9ms

  // CLEAR FAULTS AND UPDATE CUST_CRC
  ResetAllFaults(0, FRMWRT_ALL_W); // CLEAR ALL FAULTS
  delay(100);                    // visual separation for logic analyzer

  // START THE MAIN ADC
  WriteReg(0, ADC_CTRL1, 0x2E, 1, FRMWRT_ALL_W); // continuous run and MAIN_GO and LPF_VCELL_EN and CS_DR = 1ms

  //clear the write buffer
  memset(UDP_Buffer, 0, sizeof(UDP_Buffer));
  memset(message_data, 0, sizeof(message_data[0][0]) * message_data_width * 8);

  // memset(cell_voltages, 0, sizeof(cell_voltages));

}

int countTimer = 0;
int i = 0;
int cb = 0;
long double current_accumulated = 0;
long double time_accumulated = 0;
long double coulomb_accumulated = 0;
uint32_t raw_data = 0;
int32_t signed_val = 0;
long double sr_val = 0;
char response_frame[(16 * 2 + 6) * TOTALBOARDS];    // hold all 16 vcell*_hi/lo values
char response_frame_current[(MAXcharS+6)]; //


void loop()
{
  
  if(status == WL_CONNECTED){
    int ret = udp_sender.beginPacket(IPAddress(192,168,137,1), 10000);
    if(ret == 0)
      Serial.println("Error beginning packet");
    else{
      udp_sender.write(UDP_Buffer, 100);
      ret = udp_sender.endPacket();
      if(ret == 0)
        Serial.println("Error sending packet");
    }
  }
  else{
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(1);
  }

  // RESET ITERATORS EACH LOOP
  i = 0;
  cb = 0;

  delay(500);


  // ReadReg(0, PARTID, response_frame_current, 1, 0, FRMWRT_SGL_R); // 175 us
  // // PrintFrame(response_frame_current, 8);
  // // FOR A0
  // raw_data = (response_frame_current[1] << 8) | (response_frame_current[0]);
  // signed_val = (int16_t)raw_data;
  // // sr_val = signed_val * 0.00000763;
  // Serial.print("PARTID: ");
  // Serial.println(signed_val);

  
  // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
  ReadReg(0, VCELL16_HI + (16 - ACTIVECHANNELS) * 2, response_frame, ACTIVECHANNELS * 2, 0, FRMWRT_SGL_R); // 494 us

  for (cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
  {
    printConsole("BOARD %d:\t", TOTALBOARDS - cb - 1);
    for (i = 0; i < (ACTIVECHANNELS * 2); i += 2)
    {
      int boardcharStart = (ACTIVECHANNELS * 2 + 6) * cb;
      uint16_t rawData = (response_frame[boardcharStart + i + 4] << 8) | response_frame[boardcharStart + i + 5];
      cell_voltages[int(i / 2)] = round(Complement(rawData, 0.00019073) * 100);
      // float cellVoltage = Complement(rawData, 0.00019073);
      printConsole("%hu ", cell_voltages[int(i / 2)]);
    }
    printConsole("\n\r"); // newline per board
  }



}