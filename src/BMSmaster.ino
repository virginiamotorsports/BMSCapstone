#include "bq79616.hpp"
#include <math.h>
#include <stdio.h>
#include <WiFiS3.h>
#include <Arduino_CAN.h>
#include "can_helpers.hpp"
#include "BMSFSM.hpp"

#define FAULT_PIN D3
#define NFAULT_PIN D2

#define SECRET_SSID "BMS_WIFI"
#define SECRET_PASS "batteryboyz"

#define GET_TEMP(resistance) (1/(a1 + b1*(log(resistance/10000.0)) + c1*(pow(log(resistance/10000.0), 2)) + d1*(pow(log(resistance/10000.0), 3))) - 273.15)
#define GET_RESISTANCE(voltage) ((10000 * voltage) / (5 - voltage))

IPAddress send_to_address(192, 168, 244, 2);

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;

const float_t a1 = 0.003354016;
const float_t b1 = 0.000256524;
const float_t c1 = 2.61E-6;
const float_t d1 = 6.33E-8;

unsigned long lastMillis;

char UDP_Buffer[200];
struct BMS_status modules[6];
int countTimer = 0;
bool comm_fault = false;
bool bms_fault = false;
bool n_fault = false;
bool OVUV_fault = false;
bool OTUT_fault = false;

uint16_t raw_data = 0;
int32_t signed_val = 0;
long double sr_val = 0;
char voltage_response_frame[(16 * 2 + 6) * TOTALBOARDS];    // hold all 16 vcell*_hi/lo values
char temp_response_frame[(8 * 2 + 6) * TOTALBOARDS];    // hold all 16 vcell*_hi/lo values
char response_frame_current[(1+6)]; //
WiFiUDP udp;

void setup()
{

  status = WiFi.beginAP(ssid, pass);
  WiFi.config(IPAddress(192,168,244,1));

  Serial.begin(9600);
  Serial1.begin(BAUDRATE, SERIAL_8N1);
  Serial1.setTimeout(1000);
  CAN.begin(CanBitRate::BR_1000k);


  pinMode(FAULT_PIN, OUTPUT);
  pinMode(NFAULT_PIN, INPUT);
  digitalWrite(FAULT_PIN, LOW);

  Serial.print("Hello this the the BMS Code\r\n");
  // HWRST79616();

  Wake79616();
  delayMicroseconds((10000 + 520) * TOTALBOARDS); // 2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

  AutoAddress();
  // AutoAddress2();
  Serial.println("Autoaddress Completed");
  
  set_registers();

  //clear the write buffer
  memset(UDP_Buffer, 0, sizeof(UDP_Buffer));
  memset(response_frame_current, 0, sizeof(response_frame_current));
  memset(temp_response_frame, 0, sizeof(temp_response_frame));
  memset(voltage_response_frame, 0, sizeof(voltage_response_frame));
  memset(message_data, 0, sizeof(message_data[0][0]) * message_data_width * 8);
  // memset(modules, 0, sizeof(modules) * (16 * 2 + 8 * 2 + 1 + 1));

  // memset(cell_voltages, 0, sizeof(cell_voltages));
  udp.begin(IPAddress(192,168,244,1), 10000);
  udp.setTimeout(10);
}

void loop()
{
  delay(500);

  n_fault = !digitalRead(NFAULT_PIN);
  

  if(comm_fault){
      // HWRST79616();
      // delay(200);
      restart_chips();
  }
  else{

    WriteReg(0, OVUV_CTRL, 0x05, 1, FRMWRT_ALL_W); // run OV UV
    WriteReg(0, OTUT_CTRL, 0x05, 1, FRMWRT_ALL_W); // run OT UT

    ReadReg(0, FAULT_SUMMARY, response_frame_current, 1, 0, FRMWRT_ALL_R); // 175 us

    // Serial.print("PROT: ");
    // Serial.print((response_frame_current[4] & 0x80));

    // Serial.print("\tADC: ");
    // Serial.print((response_frame_current[4] & 0x40));

    // Serial.print("\tOTP: ");
    // Serial.print((response_frame_current[4] & 0x20));

    // Serial.print("\tCOMM: ");
    // Serial.print((response_frame_current[4] & 0x10));

    // Serial.print("\tOTUT: ");
    // Serial.print((response_frame_current[4] & 0x08));

    // Serial.print("\tOVUV: ");
    // Serial.print((response_frame_current[4] & 0x04));

    // Serial.print("\tSYS: ");
    // Serial.print((response_frame_current[4] & 0x02));

    // Serial.print("\tPWR: ");
    // Serial.println((response_frame_current[4] & 0x01));

    OVUV_fault = ((response_frame_current[4] & 0x04) ? true : false);
    OTUT_fault = ((response_frame_current[4] & 0x08) ? true : false);
    
    // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
    ReadReg(0, VCELL16_HI + (16 - ACTIVECHANNELS) * 2, voltage_response_frame, ACTIVECHANNELS * 2, 0, FRMWRT_ALL_R); // 494 us

    for (uint16_t cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
    {
      printConsole("B%d voltages:\t", TOTALBOARDS - cb - 1);
      for (int i = 0; i < ACTIVECHANNELS; i++)
      {
        int boardcharStart = (ACTIVECHANNELS * 2 + 6) * cb;
        raw_data = (((voltage_response_frame[boardcharStart + (i * 2) + 4] & 0xFF) << 8) | (voltage_response_frame[boardcharStart + (i * 2) + 5] & 0xFF));
        modules[cb].cell_voltages[i] = (uint16_t)(Complement(raw_data, 0.19073));
        printConsole("%.3f ", modules[cb].cell_voltages[i] / 1000.0);
      }
      printConsole("\n\r"); // newline per board
    }

    // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
    ReadReg(0, GPIO1_HI, temp_response_frame, CELL_TEMP_NUM * 2, 0, FRMWRT_ALL_R); // 494 us
    uint16_t temp_voltage = 0;
    for (uint16_t cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
    {
      printConsole("B%d temps:\t", TOTALBOARDS - cb - 1);
      for (int i = 0; i < CELL_TEMP_NUM; i++)
      {
        int boardcharStart = (CELL_TEMP_NUM * 2 + 6) * cb;
        raw_data = ((temp_response_frame[boardcharStart + (i * 2) + 4] & 0xFF) << 8) | (temp_response_frame[boardcharStart + (i * 2) + 5] & 0xFF);
        temp_voltage = (uint16_t)(raw_data * 0.15259);
        if(temp_voltage >= 4800)
        {
          modules[cb].cell_temps[i] = 1000;
        }
        else{
          modules[cb].cell_temps[i] = (uint16_t)(GET_TEMP(GET_RESISTANCE((temp_voltage / 1000.0))) * 10);
        }
        printConsole("%.1f ", modules[cb].cell_temps[i] / 10.0);
      }
      printConsole("\n\r"); // newline per board
    }

    // send_udp_packet();

    send_can_data();

    if (millis() - lastMillis >= 30*1000UL && !OVUV_fault && !OTUT_fault) 
    {
      lastMillis = millis();  //get ready for the next iteration
      Serial.println("Running CB");
      WriteReg(0, BAL_CTRL2, 0x33, 1, FRMWRT_ALL_W); // starts balancing all cells
    }

    if(OVUV_fault){
      WriteReg(0, FAULT_RST1, 0x18, 1, FRMWRT_ALL_W); // starts balancing all cells
      
    }

    if(OTUT_fault){
      WriteReg(0, FAULT_RST1, 0x60, 1, FRMWRT_ALL_W); // starts balancing all cells
      
    }
    
  }

  if(bms_fault || comm_fault || n_fault || OVUV_fault || OTUT_fault){
    digitalWrite(FAULT_PIN, LOW);
    Serial.println("Fault Detected");
  }
  else{
    digitalWrite(FAULT_PIN, HIGH);
  }

}

void send_udp_packet(void){
  for (uint16_t cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++){
    memcpy(UDP_Buffer, &cb, sizeof(cb));
    memcpy((UDP_Buffer + 2), modules[cb].cell_voltages, sizeof(modules[cb].cell_voltages));
    memcpy((UDP_Buffer + sizeof(modules[cb].cell_voltages) + 2) , modules[cb].cell_temps, sizeof(modules[cb].cell_temps));
    int ret = udp.beginPacket(IPAddress(192,168,244,2), 10244); 
    if(ret == 0){
      Serial.print("failed");
    }
    else{
      udp.write(UDP_Buffer, 50);
      udp.endPacket();
    }
  }
}

void send_can_data(void){

  // Copy cell temps over
  memcpy(message_data[CELL_TEMP_0], modules[0].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_1], (modules[0].cell_temps + 8), 8);
  memcpy(message_data[CELL_TEMP_2], modules[1].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_3], (modules[1].cell_temps + 8), 8);
  memcpy(message_data[CELL_TEMP_4], modules[2].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_5], (modules[2].cell_temps + 8), 8);
  memcpy(message_data[CELL_TEMP_6], modules[3].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_7], (modules[3].cell_temps + 8), 8);
  memcpy(message_data[CELL_TEMP_8], modules[4].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_9], (modules[4].cell_temps + 8), 8);
  memcpy(message_data[CELL_TEMP_10], modules[5].cell_temps, 8);
  memcpy(message_data[CELL_TEMP_11], (modules[5].cell_temps + 8), 8);

  // Copy cell voltages over
  memcpy(message_data[CELL_VOLTAGE_0], modules[0].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_1], (modules[0].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_2], (modules[0].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_3], (modules[0].cell_voltages + 24), 8);
  
  memcpy(message_data[CELL_VOLTAGE_4], modules[1].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_5], (modules[1].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_6], (modules[1].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_7], (modules[1].cell_voltages + 24), 8);
  
  memcpy(message_data[CELL_VOLTAGE_8], modules[2].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_9], (modules[2].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_10], (modules[2].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_11], (modules[2].cell_voltages + 24), 8);

  memcpy(message_data[CELL_VOLTAGE_12], modules[3].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_13], (modules[3].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_14], (modules[3].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_15], (modules[3].cell_voltages + 24), 8);

  memcpy(message_data[CELL_VOLTAGE_16], modules[4].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_17], (modules[4].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_18], (modules[4].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_19], (modules[4].cell_voltages + 24), 8);

  memcpy(message_data[CELL_VOLTAGE_20], modules[5].cell_voltages, 8);
  memcpy(message_data[CELL_VOLTAGE_21], (modules[5].cell_voltages + 8), 8);
  memcpy(message_data[CELL_VOLTAGE_22], (modules[5].cell_voltages + 16), 8);
  memcpy(message_data[CELL_VOLTAGE_23], (modules[5].cell_voltages + 24), 8);

  CanMsg msg;
  for(uint16_t addr = START_ITEM; addr < LAST_ITEM; addr++){
    
    msg = CanMsg(addr, sizeof(message_data[addr]), message_data[addr]);
    CAN.write(msg);
  }
}
