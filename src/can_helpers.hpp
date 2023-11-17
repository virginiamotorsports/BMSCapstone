#ifndef CANHELPERS_HPP
#define CANHELPERS_HPP

#include "stdint.h"
#include "math.h"



enum CAN_Msgs
{
    START_ITEM = 0x100,
    STATUS = START_ITEM,
    FAULT,
    CELL_TEMP_0,
    CELL_TEMP_1,
    CELL_TEMP_2, 
    CELL_TEMP_3,
    CELL_TEMP_4,
    CELL_TEMP_5, 
    CELL_TEMP_6,
    CELL_TEMP_7,
    CELL_TEMP_8, 
    CELL_TEMP_9,
    CELL_TEMP_10,
    CELL_TEMP_11, 
    CELL_VOLTAGE_0,
    CELL_VOLTAGE_1,
    CELL_VOLTAGE_2, 
    CELL_VOLTAGE_3,
    CELL_VOLTAGE_4,
    CELL_VOLTAGE_5, 
    CELL_VOLTAGE_6,
    CELL_VOLTAGE_7,
    CELL_VOLTAGE_8, 
    CELL_VOLTAGE_9,
    CELL_VOLTAGE_10,
    CELL_VOLTAGE_11,
    CELL_VOLTAGE_12,
    CELL_VOLTAGE_13,
    CELL_VOLTAGE_14, 
    CELL_VOLTAGE_15,
    CELL_VOLTAGE_16,
    CELL_VOLTAGE_17, 
    CELL_VOLTAGE_18,
    CELL_VOLTAGE_19,
    CELL_VOLTAGE_20, 
    CELL_VOLTAGE_21,
    CELL_VOLTAGE_22,
    CELL_VOLTAGE_23,
    LAST_ITEM
};

const uint8_t message_data_width = LAST_ITEM - STATUS;
char message_data[message_data_width][8];




//Methods to convert the integers into bytes to be sent via canbus
char* int_to_bytes(uint32_t input){
    char bytes[4] = {((input >> 24) & 0xFF), ((input >> 16) & 0xFF), ((input >> 8) & 0xFF), (input & 0xFF)};

    return bytes;
}

char* int_to_bytes(uint16_t input){
    char bytes[2] = {((input >> 16) & 0xFF), (input & 0xFF)};

    return bytes;
}

char* float_to_4bytes(float input){
    uint32_t integer_from_float = round(input * 100);

    return int_to_bytes(integer_from_float);
}

char* float_to_2bytes(float input){
    uint16_t integer_from_float = round(input * 100);

    return int_to_bytes(integer_from_float);
}


#endif