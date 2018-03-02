//
//  flashAT25DB641A.cpp
//  flash
//
//  Created by Gregory C Lewin on 12/26/17.
//  Copyright © 2017 Gregory C Lewin. All rights reserved.
//

#include "flash.h"

#define CMD_WRITE_STATUS1   0x01
#define CMD_WRITE           0x02
#define CMD_READ_DATA       0x0B

#define CMD_WRITE_DISABLE   0x04
#define CMD_READ_STATUS     0x05
#define CMD_WRITE_ENABLE    0x06

#define CMD_ERASE_BLOCK_4K     0x20
#define CMD_ERASE_BLOCK_32K    0x52
#define CMD_ERASE_BLOCK_64K    0xD8

#define CMD_READ_ID_DATA    0x9F
#define CMD_READ_PROTECTION_STATUS    0x3C

#define REG_GLOBAL_UNPROTECT  0x00
#define REG_GLOBAL_PROTECT    0x3C

#define STATUS_BSY      0x01
#define STATUS_WEL      0x02

IDdata FlashAT25DF641A::Init(void)
{
    pinMode(chipSelect, OUTPUT);
    digitalWrite(chipSelect, HIGH);
    
    Deselect();
    
    //this might not play well with others...
    spi->setDataMode(SPI_MODE0);
    spi->setBitOrder(MSBFIRST);
    spi->setClockDivider(SPI_CLOCK_DIV64); //why slow it down?
    
    spi->begin();
    
    idData = ReadIDdata();
    
    if(idData.manufacturerID != 0x1F) SerialUSB.print("Wrong manufacturer!");
    
    byteCount = (uint32_t)1 << (15 + (idData.deviceID1 & 0x1F));

    //hard-coded for now...
    bytesPerPage = 256;
    bytesPerBlock = 4096;
    //totalPages = byteCount / bytesPerPage;
        
    return idData;
}

uint8_t FlashAT25DF641A::IsBusy(void)
{
    uint16_t status = ReadStatus();
    return (status >> 8) & STATUS_BSY;
}

IDdata FlashAT25DF641A::ReadIDdata(void)
{
    IDdata id;
    
    Select();
    SendCommand(CMD_READ_ID_DATA);
    
    id.manufacturerID = spi->transfer(0);
    id.deviceID1 = spi->transfer(0);
    id.deviceID2 = spi->transfer(0);
    id.extLength = spi->transfer(0);
    //should really read the extended data, but we'll leave blank for now...
    
    Deselect();
    
    return id;
}

uint16_t FlashAT25DF641A::ReadStatus(void)
{
    uint16_t status;
    
    Select();
    
    SendCommand(CMD_READ_STATUS);
    
    status = spi->transfer(0);
    status <<= 8;
    status |= spi->transfer(0);
    
    Deselect();
    
    return status;
}

    
/*
 * it's up to the user to declare data to be the correct size
 */
uint32_t FlashAT25DF641A::ReadBytes(uint32_t address, uint8_t* data, uint32_t count)
{
    if(address >= byteCount) return 0; //basic check for address range
    
    Select();
    SendCommand(CMD_READ_DATA);
    SendAddress(address);
    if(CMD_READ_DATA == 0x0b) SendCommand(0x0); //dummy bits for fast read
    
    uint32_t i = 0;
    for( ; i < count; i++) //need to check requested size!
    {
        data[i] = spi->transfer(0x0);
    }
    
    Deselect();
    
    return i;
}

//uint8_t FlashAT25DF641A::ReadByte(uint32_t address)
//{
//    if(address >= byteCount) return 0; //basic check for address range
//
//    Select();
//    SendCommand(CMD_READ_DATA);
//    SendAddress(address);
//    if(CMD_READ_DATA == 0x0b) SendCommand(0x0); //dummy bits for fast read
//
//    uint8_t b = spi->transfer(0x0);
//
//    Deselect();
//
//    return b;
//}

uint8_t FlashAT25DF641A::WriteEnable(void)
{
    while(IsBusy()) {}
    
    Select();
    SendCommand(CMD_WRITE_ENABLE);
    Deselect();
    
    Select();
    uint16_t status = ReadStatus();
    Deselect();
    
    return (status >> 8) & STATUS_WEL;
}

uint8_t FlashAT25DF641A::WriteDisable(void)
{
    Select();
    SendCommand(CMD_WRITE_DISABLE);
    Deselect();
    
    Select();
    uint16_t status = ReadStatus();
    Deselect();
    
    return (status >> 8) & STATUS_WEL;
}

uint16_t FlashAT25DF641A::WritePage(uint32_t address, uint8_t* data, uint16_t count)
{
    if(address >= byteCount) return 0; //basic check for address range
    
    WriteEnable();
    
    Select(); //should check for WEL bit
    SendCommand(CMD_WRITE);
    
    SendAddress(address);

    uint16_t i = 0;
    for( ; i < count; i++)
    {
        spi->transfer(data[i]);
    }

    Deselect();

    //while(IsBusy()) {} //up to the user to make sure the write is done,
    //returning now let's it can happen in the background
    
    return i;
}

uint8_t FlashAT25DF641A::EraseBlock(uint32_t address, uint8_t sizeCmd)
{
    if(address >= byteCount) return 0; //basic check for address range
    
    Select();
    SendCommand(sizeCmd);
    SendAddress(address);
    Deselect();
    
    while(IsBusy()) {}
    
    return 1;
}

uint8_t FlashAT25DF641A::EraseBlock4K(uint32_t address)
{
    return EraseBlock(address, CMD_ERASE_BLOCK_4K);
    
}

uint8_t FlashAT25DF641A::EraseBlock32K(uint32_t address)
{
    return EraseBlock(address, CMD_ERASE_BLOCK_32K);
    
}

uint8_t FlashAT25DF641A::EraseBlock64K(uint32_t address)
{
    return EraseBlock(address, CMD_ERASE_BLOCK_64K);
    
}

uint8_t FlashAT25DF641A::ReadSectorProtectionStatus(uint32_t address)
{
    if(address >= byteCount) return 0; //basic check for address range
    
    Select();
    
    SendCommand(CMD_READ_PROTECTION_STATUS);
    SendAddress(address);
    
    uint8_t status = spi->transfer(0x0);
    
    while(IsBusy()) {}
    
    Deselect();
    
    return status;
}

void FlashAT25DF641A::GlobalUnprotect(void)
{
    Select();
    
    SendCommand(CMD_WRITE_STATUS1);
    SendCommand(REG_GLOBAL_UNPROTECT);
    
    Deselect();
}

void FlashAT25DF641A::GlobalProtect(void)
{
    Select();
    
    SendCommand(CMD_WRITE_STATUS1);
    SendCommand(REG_GLOBAL_PROTECT);
    
    Deselect();
}
