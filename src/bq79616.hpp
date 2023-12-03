/*
 *  @file bq79606.h
 *
 *  @author Vince Toledo - Texas Instruments Inc.
 *  @date July 2019
 *  @version 1.0 beta version
 *  @note Built with CCS for Hercules Version: 8.1.0.00011
 *  @note Built for TMS570LS1224 (LAUNCH XL2)
 */

/*****************************************************************************
**
**  Copyright (c) 2011-2017 Texas Instruments
**
******************************************************************************/


#ifndef BQ79616_HPP_
#define BQ79616_HPP_


#include "Arduino.h"
#include "stdint.h"
#include "stdio.h"
#include "stdbool.h"
#include "string.h"

//****************************************************
// ***Register defines, choose one of the following***
// ***based on your device silicon revision:       ***
//****************************************************
//#include "A0_reg.h"
#include "B0_reg.h"

// User defines
#define TOTALBOARDS 1       //boards in stack
#define CELL_TEMP_NUM 8
#define ACTIVECHANNELS 16   //channels to activate (incomplete, does not work right now)
#define BRIDGEDEVICE 0   //
#define MAXcharS (16*2)     //maximum number of chars to be read from the devices (for array creation)
#define BAUDRATE 1000000    //device + uC baudrate

#define FRMWRT_SGL_R	0x00    //single device READ
#define FRMWRT_SGL_W	0x10    //single device WRITE
#define FRMWRT_STK_R	0x20    //stack READ
#define FRMWRT_STK_W	0x30    //stack WRITE
#define FRMWRT_ALL_R	0x40    //broadcast READ
#define FRMWRT_ALL_W	0x50    //broadcast WRITE
#define FRMWRT_REV_ALL_W 0xE0   //broadcast WRITE reverse direction
                  
// namespace bq{
// Function Prototypes
void Wake79616();
void Wake79606();
void HWRST79616();
void CommClear(void);
void CommSleepToActive(void);
void CommReset(void);
void AutoAddress();
void AutoAddress2();
void set_registers(void);
void restart_chips(void);
bool GetFaultStat();
float Complement(uint16_t rawData, float multiplier);

uint16_t CRC16(char *pBuf, int nLen);

int  WriteReg(char bID, uint16_t wAddr, uint64_t dwData, char bLen, char bWriteType);
int  WriteRegBad(char bID, uint16_t wAddr, uint64_t dwData, char bLen, char bWriteType);
int  ReadReg(char bID, uint16_t wAddr, char * pData, char bLen, uint32_t dwTimeOut, char bWriteType);

int  WriteFrame(char bID, uint16_t wAddr, char * pData, char bLen, char bWriteType);
int  WriteBadFrame(char bID, uint16_t wAddr, char * pData, char bLen, char bWriteType);
int  ReadFrameReq(char bID, uint16_t wAddr, char bcharToReturn,char bWriteType);
int  ReadFrameReqBad(char bID, uint16_t wAddr, char bcharToReturn);
int  WaitRespFrame(char *pFrame, uint32_t bLen, uint32_t dwTimeOut);

// void delayms(uint16_t ms);
// void delayus(uint16_t us);

void ResetAllFaults(char bID, char bWriteType);
void MaskAllFaults(char bID, char bWriteType);

void PrintFrame(char arr[], int chars);
unsigned printConsole(const char *_format, ...);

uint16_t volt2char(float volt);


struct BMS_status{
    uint16_t cell_voltages[16];
    uint16_t cell_temps[8];
    uint8_t status;
    bool fault;
};

// }
#endif /* BQ79606_H_ */
//EOF

