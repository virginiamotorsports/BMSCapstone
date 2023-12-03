
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


#define SECRET_SSID "BMS_WIFI"
#define SECRET_PASS "batteryboyz"

IPAddress send_to_address(192, 168, 244, 2);

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;

char UDP_Buffer[200];
struct BMS_status modules[6];
// uint16_t cell_voltages[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int countTimer = 0;
int i = 0;
int cb = 0;
uint32_t raw_data = 0;
int32_t signed_val = 0;
long double sr_val = 0;
char response_frame[(16 * 2 + 6) * TOTALBOARDS];    // hold all 16 vcell*_hi/lo values
char response_frame_current[(MAXcharS+6)]; //
WiFiUDP udp;

void setup()
{

  WiFi.config(IPAddress(192,168,137,2), IPAddress(8,8,8,8), IPAddress(192,168,137,1), IPAddress(255,255,255,0));
  status = WiFi.beginAP(ssid, pass);

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
  
  set_registers();

  //clear the write buffer
  memset(UDP_Buffer, 0, sizeof(UDP_Buffer));
  memset(response_frame_current, 0, sizeof(response_frame_current));
  memset(response_frame, 0, sizeof(response_frame));
  memset(message_data, 0, sizeof(message_data[0][0]) * message_data_width * 8);

  // memset(cell_voltages, 0, sizeof(cell_voltages));

}





void loop()
{

  // uint8_t const msg_data[] = {0xCA,0xFE,0,0,0,0,0,0};
  // memcpy((void *)(msg_data + 4), &msg_cnt, sizeof(msg_cnt));
  // CanMsg msg(CAN_ID, sizeof(msg_data), msg_data);

  udp.beginPacket(send_to_address, 10244);
  udp.write(UDP_Buffer, sizeof(UDP_Buffer));
  udp.endPacket();

  // RESET ITERATORS EACH LOOP
  i = 0;
  cb = 0;

  delay(500);


  ReadReg(0, FAULT_SUMMARY, response_frame_current, 1, 0, FRMWRT_ALL_R); // 175 us
  Serial.print("PROT: ");
  Serial.print((response_frame_current[4] & 0x80));

  Serial.print("\tADC: ");
  Serial.print((response_frame_current[4] & 0x40));

  Serial.print("\tOTP: ");
  Serial.print((response_frame_current[4] & 0x20));

  Serial.print("\tCOMM: ");
  Serial.print((response_frame_current[4] & 0x10));

  Serial.print("\tOTUT: ");
  Serial.print((response_frame_current[4] & 0x08));

  Serial.print("\tOVUV: ");
  Serial.print((response_frame_current[4] & 0x04));

  Serial.print("\tSYS: ");
  Serial.print((response_frame_current[4] & 0x02));

  Serial.print("\tPWR: ");
  Serial.println((response_frame_current[4] & 0x01));
  // // FOR A0
  // raw_data = response_frame_current[4];
  // signed_val = (int16_t)raw_data;
  // // // sr_val = signed_val * 0.00000763;
  // Serial.print("PARTID: ");
  // Serial.println(signed_val);

  
  // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
  ReadReg(0, VCELL16_HI + (16 - ACTIVECHANNELS) * 2, response_frame, ACTIVECHANNELS * 2, 0, FRMWRT_ALL_R); // 494 us

  for (cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
  {
    printConsole("B%d voltages:\t", TOTALBOARDS - cb - 1);
    for (i = 0; i < (ACTIVECHANNELS * 2); i += 2)
    {
      int boardcharStart = (ACTIVECHANNELS * 2 + 6) * cb;
      uint16_t rawData = (response_frame[boardcharStart + i + 4] << 8) | response_frame[boardcharStart + i + 5];
      modules[cb].cell_voltages[int(i / 2)] = round(Complement(rawData, 0.00019073) * 100);
      printConsole("%hu ", modules[cb].cell_voltages[int(i / 2)]);
    }
    printConsole("\n\r"); // newline per board
  }


  // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
  ReadReg(0, GPIO1_HI + (8 - CELL_TEMP_NUM) * 2, response_frame, CELL_TEMP_NUM * 2, 0, FRMWRT_ALL_R); // 494 us

  for (cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
  {
    printConsole("B%d temps:\t", TOTALBOARDS - cb - 1);
    for (i = 0; i < (CELL_TEMP_NUM * 2); i += 2)
    {
      int boardcharStart = (ACTIVECHANNELS * 2 + 6) * cb;
      uint16_t rawData = (response_frame[boardcharStart + i + 4] << 8) | response_frame[boardcharStart + i + 5];
      modules[cb].cell_temps[int(i / 2)] = round(rawData * 0.00019073 * 100);
      printConsole("%hu ", modules[cb].cell_temps[int(i / 2)]);
    }
    printConsole("\n\r"); // newline per board
  }

  
  for (cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
  {
    if(modules[cb].fault){
      // AMS.fault = true;
      break;
    }
  }


}


void set_registers(void){

  WriteReg(0, FAULT_MSK2, 0x40, 1, FRMWRT_ALL_W); //OPTIONAL: MASK CUST_CRC SO CONFIG CHANGES DON'T FLAG A FAULT
  WriteReg(0, FAULT_MSK1, 0xFFFE, 2, FRMWRT_ALL_W); // INITIAL B0 SILICON: MASK FAULT_PWR SO TSREF_UV doesn't flag a fault
  ResetAllFaults(0, FRMWRT_ALL_W);

  // VARIABLES

  // ARRAYS (MUST place out of loop so not re-allocated every iteration)

  // ENABLE TSREF
  WriteReg(0, CONTROL2, 0x01, 1, FRMWRT_ALL_W); // enable TSREF

  // CONFIGURE GPIOS as temp inputs
  WriteReg(0, GPIO_CONF1, 0x09, 1, FRMWRT_ALL_W); // GPIO1 and 2 as temp inputs
  WriteReg(0, GPIO_CONF2, 0x09, 1, FRMWRT_ALL_W); // GPIO3 and 4 as temp inputs
  WriteReg(0, GPIO_CONF3, 0x09, 1, FRMWRT_ALL_W); // GPIO5 and 6 as temp inputs
  WriteReg(0, GPIO_CONF4, 0x09, 1, FRMWRT_ALL_W); // GPIO7 and 8 as temp inputs

  WriteReg(0, OTUT_THRESH, 0xDA, 1, FRMWRT_ALL_W); // Sets OV thresh to 80% and UT thresh to 20% to meet rules


  WriteReg(0, OV_THRESH, 0x25, 1, FRMWRT_ALL_W); // Sets Over voltage protection to 4.25V
  WriteReg(0, UV_THRESH, 0x24, 1, FRMWRT_ALL_W); // Sets Under voltage protection to 3.0V


  WriteReg(0, OVUV_CTRL, 0x05, 1, FRMWRT_ALL_W); // Sets voltage controls
  WriteReg(0, OTUT_CTRL, 0x05, 1, FRMWRT_ALL_W); // Sets temperature controls


  // CONFIGURE THE MAIN ADC
  WriteReg(0, ACTIVE_CELL, ACTIVECHANNELS - 6, 1, FRMWRT_ALL_W); // set all cells to active
  WriteReg(0, ADC_CONF1, 0x04, 1, FRMWRT_ALL_W);                 // LPF_ON - LPF = 9ms

  // CLEAR FAULTS AND UPDATE CUST_CRC
  ResetAllFaults(0, FRMWRT_ALL_W); // CLEAR ALL FAULTS
  // delay(100);                    // visual separation for logic analyzer

  // START THE MAIN ADC
  WriteReg(0, ADC_CTRL1, 0x1E, 1, FRMWRT_ALL_W); // continuous run and MAIN_GO and LPF_VCELL_EN and CS_DR = 1ms
  WriteReg(0, ADC_CTRL3, 0x06, 1, FRMWRT_ALL_W); // continuous run and MAIN_GO and LPF_VCELL_EN and CS_DR = 1ms

}