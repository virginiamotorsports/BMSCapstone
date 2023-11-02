
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
#include <cmsis_compiler.h>
#include <bsp_arm_exceptions.h>
#include <R7FA4M1AB.h>
#include "bq79616.h"
// #include <SoftwareSerial.h>

// #include "system.h"
// #include "gio.h"
// #include "sci.h"
// #include "rti.h"
// #include "sys_vim.h"
#include <math.h>
// #include <stdio.h>
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
// SoftwareSerial BQ_board(0, 1);
int main() {
  // put your setup code here, to run once:
  // systemInit();
  // gioInit();
  // sciInit();
  // rtiInit();
 
  // BQ_board.begin(115200); 
  // sciSetBaudrate(sciREG, 1000000);
  // vimInit();
  // _enable_IRQ();

  // printConsole("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\rBeginning Program\n\r");
  //INITIALIZE BQ79616-Q1
  Wake79616();
  delayus( (10000+520)*TOTALBOARDS ); //2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE
  Wake79616();
  delayus( (10000+520)*TOTALBOARDS ); //2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE
  AutoAddress2();
  delayus(4000-(2200+520)); //4ms total required after shutdown to wake transition for AFE settling time, this is for top device only
  //WriteReg(0, FAULT_MSK2, 0x40, 1, FRMWRT_ALL_W); //OPTIONAL: MASK CUST_CRC SO CONFIG CHANGES DON'T FLAG A FAULT
  WriteReg(0, FAULT_MSK1, 0xFFFE, 2, FRMWRT_ALL_W); //INITIAL B0 SILICON: MASK FAULT_PWR SO TSREF_UV doesn't flag a fault
  ResetAllFaults(0, FRMWRT_ALL_W);

  //VARIABLES
  

  //ARRAYS (MUST place out of loop so not re-allocated every iteration)
  

  //ENABLE TSREF
  WriteReg(0, CONTROL2, 0x01, 1, FRMWRT_ALL_W); //enable TSREF
  delayms(10); //wait for TSREF to fully enable on all boards

  //CONFIGURE GPIO1 as CS ADC conversion toggle function
  WriteReg(0, GPIO_CONF2, 0x40, 1, FRMWRT_ALL_W); //GPIO1 as CS ADC conversion toggle function

  //CONFIGURE THE MAIN ADC
  WriteReg(0, ACTIVE_CELL, ACTIVECHANNELS-6, 1, FRMWRT_ALL_W); //set all cells to active
  WriteReg(0, ADC_CONF1, 0x04, 1, FRMWRT_ALL_W);  //LPF_ON - LPF = 9ms

  //CLEAR FAULTS AND UPDATE CUST_CRC
  ResetAllFaults(0, FRMWRT_ALL_W); //CLEAR ALL FAULTS
  delayms(100); //visual separation for logic analyzer

  //START THE MAIN ADC
  WriteReg(0, ADC_CTRL1, 0x2E, 1, FRMWRT_ALL_W); //continuous run and MAIN_GO and LPF_VCELL_EN and CS_DR = 1ms

  int countTimer = 0;
  int i = 0;
  int cb = 0;
  long double current_accumulated = 0;
  long double time_accumulated = 0;
  long double coulomb_accumulated = 0;
  uint32_t raw_data = 0;
  int32_t signed_val = 0;
  long double sr_val = 0;
  char response_frame[(16*2+6)*TOTALBOARDS]; //hold all 16 vcell*_hi/lo values
  char response_frame_current[(3+6)*TOTALBOARDS]; //hold all 3 current_hi/mid/lo values

  while(1){
    // put your main code here, to run repeatedly:
  //RESET ITERATORS EACH LOOP
    i = 0;
    cb = 0;

    //WAIT FOR GPIO TO SENSE CS_DRDY GO LOW
    // while(BQ_board.available() == 0)
    // {
    //     //do other things
    // }

    ///////////////////////////
    //CURRENT SENSE (EVERY 1ms)
    ReadReg(0, CURRENT_HI, response_frame_current, 3, 0, FRMWRT_SGL_R); //175 us
  //        //FOR B0
  //        raw_data = (response_frame_current[4] << 16) | (response_frame_current[5] << 8) | response_frame_current[6];
  //        signed_val = ((raw_data & 0x800000) == 0x800000) ? (raw_data | 0xFF000000) : raw_data;
  //        sr_val = signed_val*0.0000000146;
    //FOR A0
    raw_data = (response_frame_current[4] << 8) | (response_frame_current[5]);
    signed_val = (int16_t)raw_data;
    sr_val = signed_val*0.00000763;
    current_accumulated+=sr_val;
    time_accumulated+=2; //add one current-sense reading worth of time to the time counter (1ms conversion time)
    coulomb_accumulated = current_accumulated*(time_accumulated/1000);

    if(countTimer%5==0)
    {
        ///////////////////////////
        //VOLTAGE SENSE (EVERY 9ms, so every 5 loops of 2ms each)
        ReadReg(0, VCELL16_HI+(16-ACTIVECHANNELS)*2, response_frame, ACTIVECHANNELS*2, 0, FRMWRT_ALL_R); //494 us

        ////////////////
        //PRINT VOLTAGES
        printConsole("\n\r"); //start with a newline to add some extra spacing between loop
        for(cb=0; cb<( BRIDGEDEVICE==1 ? TOTALBOARDS-1 : TOTALBOARDS); cb++)
        {
            printConsole("BOARD %d:\t",TOTALBOARDS-cb-1);
            for(i=0; i<(ACTIVECHANNELS*2); i+=2)
            {
                int boardcharStart = (ACTIVECHANNELS*2+6)*cb;
                uint16_t rawData = (response_frame[boardcharStart+i+4] << 8) | response_frame[boardcharStart+i+5];
                float cellVoltage = Complement(rawData,0.00019073);
                printConsole("%f\t", ((ACTIVECHANNELS*2)-i)/2, cellVoltage);
            }
            printConsole("\n\r"); //newline per board
        }
    }

    ///////////////////////////////
    //PRINT CURRENT, TIME, COULOMBS
    printConsole("Current Sense = %Le\n\r", sr_val);
    printConsole("current_accumulated = %Le\n\r", current_accumulated);
    printConsole("time_accumulated = %Le\n\r", time_accumulated);
    printConsole("coulomb_accumulated = %Le\n\r", coulomb_accumulated);
    printConsole("Current Sense hexadecimal = %x\n\r", raw_data);

    countTimer+=1;
    }

  return 0;
}

void NMI_Handler (void) { while(1) { }}