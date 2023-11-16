
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

#include "WiFiS3.h"

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int led =  LED_BUILTIN;
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup()
{
  // put your setup code here, to run once:
  // systemInit();
  // gioInit();
  // sciInit();
  // rtiInit();

  WiFi.config(IPAddress(192,168,0,2));
  status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    // don't continue
    while (true);
  }

  // wait 10 seconds for connection:
  delay(100);

  // start the web server on port 80
  server.begin();


  // BQ_board.begin(115200);
  // sciSetBaudrate(sciREG, 1000000);
  // vimInit();
  // _enable_IRQ();
  // pinMode(0, INPUT_PULLUP);

  Wake79616();
  // delayMicroseconds((10000 + 520) * TOTALBOARDS); // 2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

  Serial.begin(9600);
  Serial1.begin(1000000, SERIAL_8N1);

  Serial.print("Hello this the the BMS Code\r\n");

  // printConsole("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\rBeginning Program\n\r");
  // INITIALIZE BQ79616-Q1
  
  // Wake79616();
  // delayMicroseconds((10000 + 520) * TOTALBOARDS); // 2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

  AutoAddress();
  // AutoAddress2();
  Serial.println("Autoaddress Completed");

  delayMicroseconds(4000 - (2200 + 520)); // 4ms total required after shutdown to wake transition for AFE settling time, this is for top device only
  // WriteReg(0, FAULT_MSK2, 0x40, 1, FRMWRT_ALL_W); //OPTIONAL: MASK CUST_CRC SO CONFIG CHANGES DON'T FLAG A FAULT
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
char response_frame_current[(MAXcharS+6)]; // hold all 3 current_hi/mid/lo values
float_t cell_voltages[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

void loop()
{
  
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
      delayMicroseconds(10);                // This is required for the Arduino Nano RP2040 Connect - otherwise it will loop so fast that SPI will never be served.
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out to the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("Cell1: " + String(cell_voltages[0]) + " Cell2: " + String(cell_voltages[1]) + " Cell3: " + String(cell_voltages[2]) +" Cell4: " + String(cell_voltages[3]) +" Cell5: " + String(cell_voltages[4]) +" Cell6: " + String(cell_voltages[5]));
            // client.print("<p style=\"font-size:7vw;\">Click <a href=\"/H\">here</a> turn the LED on<br></p>");
            // client.print("<p style=\"font-size:7vw;\">Click <a href=\"/L\">here</a> turn the LED off<br></p>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            // break;
          }
          else {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    // close the connection:
    // client.stop();
    Serial.println("client disconnected");
  }
  else{
  }
  // put your main code here, to run repeatedly:
  // RESET ITERATORS EACH LOOP
  i = 0;
  cb = 0;
  // delay(10);
  // WAIT FOR GPIO TO SENSE CS_DRDY GO LOW
  //  while(BQ_board.available() == 0)
  //  {
  //      //do other things
  //  }
  delay(1000);
  ///////////////////////////
  // CURRENT SENSE (EVERY 1ms)
  ReadReg(0, PARTID, response_frame_current, 1, 0, FRMWRT_SGL_R); // 175 us

  //        //FOR B0
  // raw_data = (response_frame_current[3] << 8) | response_frame_current[2];
  
  // // signed_val = ((raw_data & 0x800000) == 0x800000) ? (raw_data | 0xFF000000) : raw_data;
  // // sr_val = signed_val*0.0000000146;
  PrintFrame(response_frame_current, 8);
  // FOR A0
  raw_data = (response_frame_current[1] << 8) | (response_frame_current[0]);
  signed_val = (int16_t)raw_data;
  // sr_val = signed_val * 0.00000763;
  Serial.print("PARTID: ");
  Serial.println(raw_data);
  // current_accumulated += sr_val;
  // time_accumulated += 2; // add one current-sense reading worth of time to the time counter (1ms conversion time)
  // coulomb_accumulated = current_accumulated * (time_accumulated / 1000);
    ///////////////////////////


  
  // VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
  ReadReg(0, VCELL16_HI + (16 - ACTIVECHANNELS) * 2, response_frame, ACTIVECHANNELS * 2, 0, FRMWRT_SGL_R); // 494 us

  ////////////////
  // PRINT VOLTAGES
  // Serial.println("About to print");

  printConsole("\n\r"); // start with a newline to add some extra spacing between loop
  // Serial.println("printed");

  for (cb = 0; cb < (BRIDGEDEVICE == 1 ? TOTALBOARDS - 1 : TOTALBOARDS); cb++)
  {
    printConsole("BOARD %d:\t", TOTALBOARDS - cb - 1);
    for (i = 0; i < (ACTIVECHANNELS * 2); i += 2)
    {
      int boardcharStart = (ACTIVECHANNELS * 2 + 6) * cb;
      uint16_t rawData = (response_frame[boardcharStart + i + 4] << 8) | response_frame[boardcharStart + i + 5];
      cell_voltages[int(i / 2)] = Complement(rawData, 0.00019073);
      // float cellVoltage = Complement(rawData, 0.00019073);
      printConsole("%f\t", ((ACTIVECHANNELS * 2) - i) / 2, cell_voltages[int(i / 2)]);
    }
    printConsole("\n\r"); // newline per board
  }

  // ///////////////////////////////
  // // PRINT CURRENT, TIME, COULOMBS
  // printConsole("PARTID = %Le\n\r", sr_val);
  // printConsole("current_accumulated = %Le\n\r", current_accumulated);
  // printConsole("time_accumulated = %Le\n\r", time_accumulated);
  // printConsole("coulomb_accumulated = %Le\n\r", coulomb_accumulated);
  // printConsole("Current Sense hexadecimal = %x\n\r", raw_data);

  // countTimer += 1;
}