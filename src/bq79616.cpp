/*
 *  @file bq79616.c
 *
 *  @author Vince Toledo - Texas Instruments Inc.
 *  @date July 2019
 *  @version 1.0 beta version
 *  @note Built with CCS for Hercules Version: 8.1.0.00011
 */

/*****************************************************************************
 **
 **  Copyright (c) 2011-2017 Texas Instruments
 **
 ******************************************************************************/
#include "bq79616.hpp"
#define DEBUG 1


//GLOBAL VARIABLES (use these to avoid stack overflows by creating too many function variables)
//avoid creating variables/arrays in functions, or you will run out of stack space quickly
char response_frame2[(MAXcharS+6)*TOTALBOARDS]; //response frame to be used by every read
char fault_frame[39*TOTALBOARDS]; //hold fault frame if faults are printed
int currentBoard = 0;
int currentCell = 0;
char bReturn = 0;
int bRes = 0;
int count = 10000;
char bBuf[8];
char pFrame[64];
static volatile unsigned int delayval = 0; //for delay and delayMicroseconds functions
extern int UART_RX_RDY;
extern int RTI_TIMEOUT;
int topFoundBoard = 0;
int baseCommunicating = 0;
int otpPass = 0;
char* currCRC;
int crc_i = 0;
uint16_t wCRC2 = 0xFFFF;
int crc16_i = 0;

//******
//PINGS
//******

void Wake79616(void) {
    // sciREG->GCR1 &= ~(1U << 7U); // put SCI into reset
    // sciREG->PIO0 &= ~(1U << 2U); // disable transmit function - now a GPIO
    // sciREG->PIO3 &= ~(1U << 2U); // set output to low
    // Serial1.write((char)0x0);
    Serial1.end();
    delay(1);
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delayMicroseconds(2250); // WAKE ping = 2.5ms to 3ms
    digitalWrite(1, HIGH);
    delay(1);
    Serial1.begin(BAUDRATE);
    delay(1);

    // sciInit();
    // sciSetBaudrate(sciREG, BAUDRATE);
}

void SD79616(void) {
    // sciREG->GCR1 &= ~(1U << 7U); // put SCI into reset
    // sciREG->PIO0 &= ~(1U << 2U); // disable transmit function - now a GPIO
    // sciREG->PIO3 &= ~(1U << 2U); // set output to low

    // delayMicroseconds(9000); 
    Serial1.end();
    delay(1);
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delay(10); /// SD ping = 9ms to 13ms
    digitalWrite(1, HIGH);
    delay(1);
    Serial1.begin(BAUDRATE);
    delay(1);
    // sciInit();
    // sciSetBaudrate(sciREG, BAUDRATE);
}

void StA79616(void) {
    // sciREG->GCR1 &= ~(1U << 7U); // put SCI into reset
    // sciREG->PIO0 &= ~(1U << 2U); // disable transmit function - now a GPIO
    // sciREG->PIO3 &= ~(1U << 2U); // set output to low

    // delayMicroseconds(250); 
    Serial1.end();
    delay(1);
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delayMicroseconds(250); // delayMicroseconds(250); 
    digitalWrite(1, HIGH);
    delay(1);
    Serial1.begin(BAUDRATE);
    delay(1);
    // sciInit();
    // sciSetBaudrate(sciREG, BAUDRATE);
}

void HWRST79616(void) {
    // sciREG->GCR1 &= ~(1U << 7U); // put SCI into reset
    // sciREG->PIO0 &= ~(1U << 2U); // disable transmit function - now a GPIO
    // sciREG->PIO3 &= ~(1U << 2U); // set output to low
    Serial1.end();
    delay(1);
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delayMicroseconds(36000); // HWRESET = 36ms
    digitalWrite(1, HIGH);
    delay(1);
    Serial1.begin(BAUDRATE);
    delay(1);

    // delayMicroseconds(36000); // StA ping = 36ms
    // sciInit();
    // sciSetBaudrate(sciREG, BAUDRATE);
}

//**********
//END PINGS
//**********

//**********************
//AUTO ADDRESS SEQUENCE
//**********************
void AutoAddress2()
{
    topFoundBoard = 0;
    baseCommunicating = 0;
    otpPass = 0;

    //GET BASE DEVICE COMMUNICATIONS WORKING
    while(baseCommunicating == 0)
    {
        //SEND NORMAL WAKE PING
        Wake79616();
        delayMicroseconds( (10000+520)*TOTALBOARDS ); //2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

        //DISABLE COMM LINES
        WriteReg(0, DEBUG_CTRL_UNLOCK, 0xA50500, 3, FRMWRT_ALL_W); //unlock comm ctrl registers, disable all comms, enable USER_DAISY_EN
        delay(5); //CHANGE - wait for COMM to disable

        //CHECK IF BASE DEVICE IS READING BACK
        memset(response_frame2,0,sizeof(response_frame2));
        ReadReg(0, DEBUG_CTRL_UNLOCK, response_frame2, 1, 0, FRMWRT_SGL_R);

        //IF WE GET THE CORRECT DATA BACK, THEN THE BASE DEVICE IS COMMUNICATING
        if(response_frame2[4] == 0xA5)
        {
            //SET THE LOOP VARIABLE FOR EXITING THIS LOOP
            baseCommunicating = 1;

            //RETURN THE BASE DEVICE TO ITS DEFAULT COMM REGISTERS STATUS
            WriteReg(0, DEBUG_CTRL_UNLOCK, 0x00040F, 3, FRMWRT_SGL_W); //set DEBUG_CTRL_UNLOCK and DEBUG_COMM_CTRL1/2 back to defaults

            printConsole("Base Communicating, moving on\n\r");
        }
        //OTHERWISE WE MUST KEEP LOOPING
        else
        {
            baseCommunicating = 0;
        }
    }

    //WHILE WE HAVEN'T FULLLY ADDRESSED THE ENTIRE STACK
    while(topFoundBoard < TOTALBOARDS-1)
    {
        //SEND WAKE TONE FROM THE HIGHEST DEVICE THAT RESPONDED
        WriteReg(topFoundBoard, CONTROL1, 0x20, 1, FRMWRT_SGL_W);
        //WAIT THE REQUIRED TIME FOR WAKE TRANSITION PER REMAINING BOARD
        delayMicroseconds( (2200+520)*TOTALBOARDS ); //2.2ms from shutdown/POR to active mode + 520us till device can send wake tone, PER DEVICE

        //AUTO ADDRESS ATTEMPT FOR EVERYONE

        //ENABLE AUTO ADDRESSING MODE
        WriteReg(0, CONTROL1, 0x01, 1, FRMWRT_ALL_W);

        //SET ADDRESSES FOR EVERY BOARD
        for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
        {
            WriteReg(0, DIR0_ADDR, currentBoard, 1, FRMWRT_ALL_W);
        }

        //CHECK ADDRESS OF EACH BOARD THAT RESPONDS
        for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
        {
            memset(response_frame2,0,sizeof(response_frame2));
            ReadReg(currentBoard, DIR0_ADDR, response_frame2, 1, 0, FRMWRT_SGL_R);
            //IF A DEVICE DOESN'T RESPOND WITH ITS ADDRESS CORRECTLY
            if(response_frame2[4] != currentBoard)
            {
                printConsole("BOARD %d NOT COMMUNICATING PROPERLY\n\r",currentBoard);
                //THEN THE LAST BOARD TO RESPOND WAS THE LAST FUNCTIONING BOARD
                topFoundBoard = currentBoard-1;

                //EXIT THIS LOOP AND RE-ATTEMPT THE WAKE+AUTO-ADDRESS FROM THE TOP WORKING BOARD ONWARD
                break;
            }
            else
            {
                topFoundBoard = currentBoard;
                //printConsole("BOARD %d OKAY\n\r",currentBoard);
            }
        }
    }


    //SET BASE/STACK/TOP OF STACK BITS
    WriteReg(0, COMM_CTRL, 0x02, 1, FRMWRT_ALL_W); //set everything as a stack device first

    if(TOTALBOARDS==1) //if there's only 1 board, it's the base AND top of stack, so change it to those
    {
        WriteReg(0, COMM_CTRL, 0x01, 1, FRMWRT_SGL_W);
    }
    else //otherwise set the base and top of stack individually
    {
        WriteReg(0, COMM_CTRL, 0x00, 1, FRMWRT_SGL_W);
        WriteReg(TOTALBOARDS-1, COMM_CTRL, 0x03, 1, FRMWRT_SGL_W);
    }


    //KEEP RELOADING OTP UNTIL OTP IS READING PROPERLY
    while(otpPass == 0)
    {
        //BROADCAST WRITE OTP RELOAD USING MARGIN_GO
        WriteReg(0, DIAG_OTP_CTRL, 0x01, 1, FRMWRT_ALL_W);
        delay(3); //load time + CRC compute

        //AUTO-ADDRESS AGAIN
        //ENABLE AUTO ADDRESSING MODE
        WriteReg(0, CONTROL1, 0X01, 1, FRMWRT_ALL_W);

        //SET ADDRESSES FOR EVERY BOARD
        for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
        {
            WriteReg(0, DIR0_ADDR, currentBoard, 1, FRMWRT_ALL_W);
        }

        WriteReg(0, COMM_CTRL, 0x02, 1, FRMWRT_ALL_W); //set everything as a stack device first

        if(TOTALBOARDS==1) //if there's only 1 board, it's the base AND top of stack, so change it to those
        {
            WriteReg(0, COMM_CTRL, 0x01, 1, FRMWRT_SGL_W);
        }
        else //otherwise set the base and top of stack individually
        {
            WriteReg(0, COMM_CTRL, 0x00, 1, FRMWRT_SGL_W);
            WriteReg(TOTALBOARDS-1, COMM_CTRL, 0x03, 1, FRMWRT_SGL_W);
        }


        //RESET OTP FAULTS
        WriteReg(0, FAULT_RST1, 0x0260, 2, FRMWRT_ALL_W);


        //CHECK IF FAULT SUMMARY SET FOR ANY BOARD
        memset(fault_frame, 0, sizeof(fault_frame));
        ReadReg(0, FAULT_SUMMARY, fault_frame, 1, 0, FRMWRT_ALL_R);
        memset(response_frame2, 0, sizeof(response_frame2));
        ReadReg(0, FAULT_OTP, response_frame2, 1, 0, FRMWRT_ALL_R);
        for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
        {
            //Serial.println("board %d fault_sum = %02x\t",currentBoard,fault_frame[currentBoard*7+4]);
            //Serial.println("board %d fault_otp = %02x\n",currentBoard,response_frame2[currentBoard*7+4]);
            if((fault_frame[currentBoard*7+4] & 0x20 == 0x20) && (response_frame2[currentBoard*7+4] & 0x02 != 0x02))
            {
                printConsole("ERROR: DIAGMARGIN FAILED - FAULT_SUMMARY[FAULT_OTP] NONZERO, BOARD %d FAULT_SUMMARY = %02x, FAULT_OTP = %02x\n",TOTALBOARDS-currentBoard-1,fault_frame[currentBoard*7+4],response_frame2[currentBoard*7+4]);
                otpPass = 0;
            }
            else
            {
                otpPass = 1;
            }
        }
    }
    //OPTIONAL: read back all device addresses
    for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
    {
        ReadReg(currentBoard, DIR0_ADDR, response_frame2, 1, 0, FRMWRT_SGL_R);
    }
}
void AutoAddress()
{
    //DUMMY WRITE TO SNCHRONIZE ALL DAISY CHAIN DEVICES DLL (IF A DEVICE RESET OCCURED PRIOR TO THIS)
    WriteReg(0, OTP_ECC_TEST, 0X00, 1, FRMWRT_ALL_W);

    //ENABLE AUTO ADDRESSING MODE
    WriteReg(0, CONTROL1, 0X01, 1, FRMWRT_ALL_W);

    //SET ADDRESSES FOR EVERY BOARD
    for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
    {
        WriteReg(0, DIR0_ADDR, currentBoard, 1, FRMWRT_ALL_W);
    }

    WriteReg(0, COMM_CTRL, 0x02, 1, FRMWRT_ALL_W); //set everything as a stack device first

    if(TOTALBOARDS==1) //if there's only 1 board, it's the base AND top of stack, so change it to those
    {
        WriteReg(0, COMM_CTRL, 0x01, 1, FRMWRT_SGL_W);
    }
    else //otherwise set the base and top of stack individually
    {
        WriteReg(0, COMM_CTRL, 0x00, 1, FRMWRT_SGL_W);
        WriteReg(TOTALBOARDS-1, COMM_CTRL, 0x03, 1, FRMWRT_SGL_W);
    }

    //SYNCRHONIZE THE DLL WITH A THROW-AWAY READ
    ReadReg(0, OTP_ECC_TEST, response_frame2, 1, 0, FRMWRT_ALL_R);

    //OPTIONAL: read back all device addresses
    for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
    {
        ReadReg(currentBoard, DIR0_ADDR, response_frame2, 1, 0, FRMWRT_SGL_R);
        //Serial.println("board %d\n",response_frame2[4]);
    }

    //RESET ANY COMM FAULT CONDITIONS FROM STARTUP
    WriteReg(0, FAULT_RST2, 0x03, 1, FRMWRT_ALL_W);

    return;
}
//**************************
//END AUTO ADDRESS SEQUENCE
//**************************


//************************
//WRITE AND READ FUNCTIONS
//************************

//FORMAT WRITE DATA, SEND TO
//BE COMBINED WITH REST OF FRAME
int WriteReg(char bID, uint16_t wAddr, uint64_t dwData, char bLen, char bWriteType) {
	// device address, register start address, data chars, data length, write type (single, broadcast, stack)
	bRes = 0;
	memset(bBuf,0,sizeof(bBuf));
	switch (bLen) {
	case 1:
		bBuf[0] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 1, bWriteType);
		break;
	case 2:
		bBuf[0] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[1] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 2, bWriteType);
		break;
	case 3:
		bBuf[0] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[1] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[2] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 3, bWriteType);
		break;
	case 4:
		bBuf[0] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[1] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[2] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[3] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 4, bWriteType);
		break;
	case 5:
		bBuf[0] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[1] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[2] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[3] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[4] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 5, bWriteType);
		break;
	case 6:
		bBuf[0] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[1] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[2] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[3] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[4] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[5] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 6, bWriteType);
		break;
	case 7:
		bBuf[0] = (dwData & 0x00FF000000000000) >> 48;
		bBuf[1] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[2] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[3] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[4] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[5] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[6] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 7, bWriteType);
		break;
	case 8:
		bBuf[0] = (dwData & 0xFF00000000000000) >> 56;
		bBuf[1] = (dwData & 0x00FF000000000000) >> 48;
		bBuf[2] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[3] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[4] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[5] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[6] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[7] = dwData & 0x00000000000000FF;
		bRes = WriteFrame(bID, wAddr, bBuf, 8, bWriteType);
		break;
	default:
		break;
	}
	return bRes;
}

//FORMAT WRITE DATA NORMALLY,
//BUT USE FRAME WITH BAD CRC
int WriteRegBad(char bID, uint16_t wAddr, uint64_t dwData, char bLen,
		char bWriteType) {
	bRes = 0;
	memset(bBuf,0,sizeof(bBuf));
	switch (bLen) {
	case 1:
		bBuf[0] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 1, bWriteType);
		break;
	case 2:
		bBuf[0] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[1] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 2, bWriteType);
		break;
	case 3:
		bBuf[0] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[1] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[2] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 3, bWriteType);
		break;
	case 4:
		bBuf[0] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[1] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[2] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[3] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 4, bWriteType);
		break;
	case 5:
		bBuf[0] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[1] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[2] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[3] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[4] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 5, bWriteType);
		break;
	case 6:
		bBuf[0] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[1] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[2] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[3] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[4] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[5] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 6, bWriteType);
		break;
	case 7:
		bBuf[0] = (dwData & 0x00FF000000000000) >> 48;
		bBuf[1] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[2] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[3] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[4] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[5] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[6] = dwData & 0x00000000000000FF;
		;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 7, bWriteType);
		break;
	case 8:
		bBuf[0] = (dwData & 0xFF00000000000000) >> 56;
		bBuf[1] = (dwData & 0x00FF000000000000) >> 48;
		bBuf[2] = (dwData & 0x0000FF0000000000) >> 40;
		bBuf[3] = (dwData & 0x000000FF00000000) >> 32;
		bBuf[4] = (dwData & 0x00000000FF000000) >> 24;
		bBuf[5] = (dwData & 0x0000000000FF0000) >> 16;
		bBuf[6] = (dwData & 0x000000000000FF00) >> 8;
		bBuf[7] = dwData & 0x00000000000000FF;
		bRes = WriteBadFrame(bID, wAddr, bBuf, 8, bWriteType);
		break;
	default:
		break;
	}
	return bRes;
}

//GENERATE COMMAND FRAME
int WriteFrame(char bID, uint16_t wAddr, char * pData, char bLen, char bWriteType) {
	int bPktLen = 0;
	char * pBuf = pFrame;
	uint16_t wCRC;
	memset(pFrame, 0x7F, sizeof(pFrame));
	*pBuf++ = 0x80 | (bWriteType) | ((bWriteType & 0x10) ? bLen - 0x01 : 0x00); //Only include blen if it is a write; Writes are 0x90, 0xB0, 0xD0
	if (bWriteType == FRMWRT_SGL_R || bWriteType == FRMWRT_SGL_W)
	{
		*pBuf++ = (bID & 0x00FF);
	}
	*pBuf++ = (wAddr & 0xFF00) >> 8;
	*pBuf++ = wAddr & 0x00FF;

	while (bLen--)
		*pBuf++ = *pData++;

	bPktLen = pBuf - pFrame;

	wCRC = CRC16(pFrame, bPktLen);
	*pBuf++ = wCRC & 0x00FF;
	*pBuf++ = (wCRC & 0xFF00) >> 8;
	bPktLen += 2;
	//THIS SEEMS to occasionally drop chars from the frame. Sometimes is not sending the last frame of the CRC.
	//(Seems to be caused by stack overflow, so take precautions to reduce stack usage in function calls)
	// sciSend(sciREG, bPktLen, pFrame);
    Serial1.write(pFrame, bPktLen);
    delay(1);

	return bPktLen;
}

//WRITES BAD CRC, TO TEST CRC
int WriteBadFrame(char bID, uint16_t wAddr, char * pData, char bLen,
		char bWriteType) {
	int bPktLen = 0;
	char * pBuf = pFrame;
	uint16_t wCRC;

    memset(pFrame, 0x7F, sizeof(pFrame));
    *pBuf++ = 0x80 | (bWriteType) | ((bWriteType & 0x10) ? bLen - 0x01 : 0x00); //Only include blen if it is a write; Writes are 0x90, 0xB0, 0xD0
    if (bWriteType == FRMWRT_SGL_R || bWriteType == FRMWRT_SGL_W)
    {
        *pBuf++ = (bID & 0x00FF);
    }
    *pBuf++ = (wAddr & 0xFF00) >> 8;
    *pBuf++ = wAddr & 0x00FF;

    while (bLen--)
        *pBuf++ = *pData++;

    bPktLen = pBuf - pFrame;

	wCRC = 0xABCD;
	*pBuf++ = wCRC & 0x00FF;
	*pBuf++ = (wCRC & 0xFF00) >> 8;
	bPktLen += 2;

	// sciSend(sciREG, bPktLen, pFrame);
  Serial1.write(pFrame, bPktLen);

	return bPktLen;
}

//GENERATE READ COMMAND FRAME AND THEN WAIT FOR RESPONSE DATA (INTERRUPT MODE FOR SCIRX)
int ReadReg(char bID, uint16_t wAddr, char * pData, char bLen, uint32_t dwTimeOut,
        char bWriteType) {
    // device address, register start address, char frame pointer to store data, data length, read type (single, broadcast, stack)
    bRes = 0;
    count = 500000;
    if (bWriteType == FRMWRT_SGL_R) {
        ReadFrameReq(bID, wAddr, bLen, bWriteType);
        memset(pData, 0, sizeof(pData));
        // sciEnableNotification(sciREG, SCI_RX_INT);
        // sciReceive(sciREG, bLen + 6, pData);
        Serial1.readBytes(pData, bLen + 6);
        // while(UART_RX_RDY == 0U && count>0) count--; /* Wait */
        // UART_RX_RDY = 0;
        bRes = bLen + 6;
    } else if (bWriteType == FRMWRT_STK_R) {
        bRes = ReadFrameReq(bID, wAddr, bLen, bWriteType);
        memset(pData, 0, sizeof(pData));
        // sciEnableNotification(sciREG, SCI_RX_INT);
        // sciReceive(sciREG, (bLen + 6) * (TOTALBOARDS - 1), pData);
        Serial1.readBytes(pData, (bLen + 6) * (TOTALBOARDS - 1));
        // while(UART_RX_RDY == 0U && count>0) count--; /* Wait */
        // UART_RX_RDY = 0;
        bRes = (bLen + 6) * (TOTALBOARDS - 1);
    } else if (bWriteType == FRMWRT_ALL_R) {
        bRes = ReadFrameReq(bID, wAddr, bLen, bWriteType);
        memset(pData, 0, sizeof(pData));
        // sciEnableNotification(sciREG, SCI_RX_INT);
        // sciReceive(sciREG, (bLen + 6) * TOTALBOARDS, pData);
        Serial1.readBytes(pData, (bLen + 6) * TOTALBOARDS);
        // while(UART_RX_RDY == 0U && count>0) count--; /* Wait */
        // UART_RX_RDY = 0;
        bRes = (bLen + 6) * TOTALBOARDS;
    } else {
        bRes = 0;
    }

//    CHECK IF CRC IS CORRECT
   for(crc_i=0; crc_i<bRes; crc_i+=(bLen+6))
   {
       if(CRC16(&pData[crc_i], bLen+6)!=0)
       {
           printConsole("\n\rBAD CRC=%04X,i=%d,bLen=%d\n\r",(pData[crc_i+bLen+4]<<8|pData[crc_i+bLen+5]),crc_i,bLen);
           PrintFrame(pData, bLen);
       }
   }
   crc_i = 0;
   currCRC = pData;
   for(crc_i=0; crc_i<bRes; crc_i+=(bLen+6))
   {
    //    printConsole("%x",&currCRC);
       if(CRC16(currCRC, bLen+6)!=0)
       {
           printConsole("\n\rBAD CRC=%04X,char=%d\n\r",(currCRC[bLen+4]<<8|currCRC[bLen+5]),crc_i);
           PrintFrame(pData, bLen);
       }
       *currCRC+=(bLen+6);
   }

    return bRes;
}

int ReadFrameReq(char bID, uint16_t wAddr, char bcharToReturn, char bWriteType) {
	bReturn = bcharToReturn - 1;

	if (bReturn > 127)
		return 0;

	return WriteFrame(bID, wAddr, &bReturn, 1, bWriteType);
}

int ReadFrameReqBad(char bID, uint16_t wAddr, char bcharToReturn) {
	bReturn = bcharToReturn - 1;

	if (bReturn > 127)
		return 0;

	return WriteBadFrame(bID, wAddr, &bReturn, 1, FRMWRT_SGL_R);
}

// CRC16 TABLE
// ITU_T polynomial: x^16 + x^15 + x^2 + 1
const uint16_t crc16_table[256] = { 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301,
		0x03C0, 0x0280, 0xC241, 0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1,
		0xC481, 0x0440, 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81,
		0x0E40, 0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
		0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 0x1E00,
		0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 0x1400, 0xD4C1,
		0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641, 0xD201, 0x12C0, 0x1380,
		0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040, 0xF001, 0x30C0, 0x3180, 0xF141,
		0x3300, 0xF3C1, 0xF281, 0x3240, 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501,
		0x35C0, 0x3480, 0xF441, 0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0,
		0x3E80, 0xFE41, 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881,
		0x3840, 0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
		0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40, 0xE401,
		0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1,
		0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041, 0xA001, 0x60C0, 0x6180,
		0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 0x6600, 0xA6C1, 0xA781, 0x6740,
		0xA501, 0x65C0, 0x6480, 0xA441, 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01,
		0x6FC0, 0x6E80, 0xAE41, 0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1,
		0xA881, 0x6840, 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80,
		0xBA41, 0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
		0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640, 0x7200,
		0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041, 0x5000, 0x90C1,
		0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241, 0x9601, 0x56C0, 0x5780,
		0x9741, 0x5500, 0x95C1, 0x9481, 0x5440, 0x9C01, 0x5CC0, 0x5D80, 0x9D41,
		0x5F00, 0x9FC1, 0x9E81, 0x5E40, 0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901,
		0x59C0, 0x5880, 0x9841, 0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1,
		0x8A81, 0x4A40, 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80,
		0x8C41, 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
		0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

uint16_t CRC16(char *pBuf, int nLen) {
    wCRC2 = 0xFFFF;
    //printConsole("CRCOUT = \t");
    for (crc16_i = 0; crc16_i < nLen; crc16_i++) {
        //printConsole("%02x ",*pBuf);
        wCRC2 ^= (*pBuf++) & 0x00FF;
        wCRC2 = crc16_table[wCRC2 & 0x00FF] ^ (wCRC2 >> 8);
    }
    //printConsole("\n\r");

    return wCRC2;
}
//****************************
//END WRITE AND READ FUNCTIONS
//****************************

//************************
//MISCELLANEOUS FUNCTIONS
//************************
float Complement(uint16_t rawData, float multiplier)
{
    return -1*(~rawData+1)*multiplier;
}

bool GetFaultStat() {

    // if (!gioGetBit(gioPORTA, 7))
    //     return 0;
    // return 1;
    return 0;
}

void ResetAllFaults(char bID, char bWriteType)
{
    if(bWriteType==FRMWRT_ALL_W)
    {
        ReadReg(0, CUST_CRC_RSLT_HI, fault_frame, 2, 0, FRMWRT_ALL_R);
        for(currentBoard=0; currentBoard<TOTALBOARDS; currentBoard++)
        {
            WriteReg(TOTALBOARDS-currentBoard-1, CUST_CRC_HI, fault_frame[currentBoard*8+4] << 8 | fault_frame[currentBoard*8+5], 2, FRMWRT_SGL_W);
        }
        WriteReg(0, FAULT_RST1, 0xFFFF, 2, FRMWRT_ALL_W);
    }
    else if(bWriteType==FRMWRT_SGL_W)
    {
        WriteReg(bID, FAULT_RST1, 0xFFFF, 2, FRMWRT_SGL_W);
    }
    else if(bWriteType==FRMWRT_STK_W)
    {
        WriteReg(0, FAULT_RST1, 0xFFFF, 2, FRMWRT_STK_W);
    }
    else
    {
        Serial.println("ERROR: ResetAllFaults bWriteType incorrect\n");
    }
}

void MaskAllFaults(char bID, char bWriteType)
{
    if(bWriteType==FRMWRT_ALL_W)
    {
        WriteReg(0, FAULT_MSK1, 0xFFFF, 2, FRMWRT_ALL_W);
    }
    else if(bWriteType==FRMWRT_SGL_W)
    {
        WriteReg(bID, FAULT_MSK1, 0xFFFF, 2, FRMWRT_SGL_W);
    }
    else if(bWriteType==FRMWRT_STK_W)
    {
        WriteReg(0, FAULT_MSK1, 0xFFFF, 2, FRMWRT_STK_W);
    }
    else
    {
        Serial.println("ERROR: MaskAllFaults bWriteType incorrect\n");
    }
}

void PrintAllFaults(char bID, char bWriteType)
{
    //PRINT 39 REGISTERS STARTING FROM FAULT_SUMMARY (INCLUDES RESERVED REGISTERS)
    printConsole("\n\r");
    currentBoard = 0;
    currentCell = 0;
    int currentLoc = 0;
    memset(fault_frame,0,sizeof(fault_frame));
    if(bWriteType==FRMWRT_ALL_R)
    {
       ReadReg(0, FAULT_SUMMARY, fault_frame, 41, 0, FRMWRT_ALL_R);
       printConsole("BOARD X FAULTS:\t",TOTALBOARDS-currentBoard);
       printConsole("                ");
       for(currentCell = 0; currentCell<(41+6); currentCell++)
       {
           printConsole("%03x ", currentCell+0x52D);
       }
       printConsole("\n\r");
       for(currentBoard = 0; currentBoard<TOTALBOARDS; currentBoard++)
       {
           printConsole("BOARD %d FAULTS:\t",TOTALBOARDS-currentBoard);
           for(currentCell = 0; currentCell<(41+6); currentCell++)
           {
               currentLoc = (currentBoard*(41+6)) + currentCell;
               printConsole("%02x  ",fault_frame[currentLoc]);
           }
           printConsole("\n\r");
       }
    }
    else if(bWriteType==FRMWRT_SGL_R)
    {
        ReadReg(bID, FAULT_SUMMARY, fault_frame, 41, 0, FRMWRT_SGL_R);
        printConsole("BOARD %d FAULTS:\t",bID);
        for(currentCell = 0; currentCell<41; currentCell++)
        {
            printConsole("%02x ",fault_frame[4+currentCell]);
        }
        printConsole("\n\r");
    }
    else if(bWriteType==FRMWRT_STK_R)
    {
        ReadReg(0, FAULT_SUMMARY, fault_frame, 41, 0, FRMWRT_STK_R);
        for(currentBoard = 0; currentBoard<(TOTALBOARDS-1); currentBoard++)
        {
            printConsole("BOARD %d FAULTS:\t",TOTALBOARDS-currentBoard);
            for(currentCell = 0; currentCell<41; currentCell++)
            {
                printConsole("%02x ",fault_frame[(currentBoard*(41+6))+4+currentCell]);
            }
            printConsole("\n\r");
        }
    }
    else
    {
        printConsole("ERROR: PrintAllFaults bWriteType incorrect\n\r");
    }
    printConsole("\n\r");
}

uint16_t volt2char(float volt)
{
    return (uint16_t)~((int16_t)((-volt/0.00019073)-1.0));
}

void PrintFrame(char arr[], int chars)
{
    if (DEBUG){
        int j = 0;
        int k = 0;
        printConsole("\n\r");
        for(j = 0; j<TOTALBOARDS; j++)
        {
            int offset = j*(chars+6);
            for(k = 0; k<(chars+6); k++)
            {
                printConsole("%02X ",arr[offset+k]);
            }
            printConsole("\n\r");
        }
    }
}


unsigned printConsole(const char *_format, ...)
{
    if (DEBUG){
        char str[128];
        int length = -1, k = 0;

            
        va_list argList;
        va_start( argList, _format );

        length = vsnprintf(str, sizeof(str), _format, argList);

        va_end( argList );

        //   if (length > 0)
        //   {
        //      for(k=0; k<length; k++)
        //      {
        //          HetUART1PutChar(str[k]);
        //      }
        //   }
        Serial.print(str);
        //   Serial.print()
        //  sciSend(scilinREG, length, str);

        return (unsigned)length;
    }
}

//***************************
//END MISCELLANEOUS FUNCTIONS
//***************************

//EOF

