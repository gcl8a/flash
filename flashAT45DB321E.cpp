//
//  flash.cpp
//  flash
//
//  Created by Gregory C Lewin on 12/26/17.
//  Copyright © 2017 Gregory C Lewin. All rights reserved.
//

#include "flash.h"

#define CMD_WRITE_THRU_BUFFER   0x02
#define CMD_READ_DATA           0x0B

//#define CMD_WRITE_DISABLE   0x04
//#define CMD_WRITE_ENABLE    0x06
#define CMD_READ_STATUS     0xD7

#define CMD_ERASE_PAGE          0x81
#define CMD_ERASE_BLOCK_4K      0x50
#define CMD_ERASE_SECTOR        0x7C
//#define CMD_ERASE_CHIP          C7h, 94h, 80h, and 9Ah

#define CMD_READ_ID_DATA    0x9F
//#define CMD_READ_PROTECTION_STATUS    0x3C
//
//#define REG_GLOBAL_UNPROTECT  0x00
//#define REG_GLOBAL_PROTECT    0x3C
//
#define STATUS_RDY      0x80

IDdata FlashAT45DB321E::Init(void)
{
    pinMode(chipSelect, OUTPUT);
    digitalWrite(chipSelect, HIGH);
    
    Deselect();
    
    //this might not play well with others...
    spi->setDataMode(SPI_MODE0);
    spi->setBitOrder(MSBFIRST);
    spi->setClockDivider(SPI_CLOCK_DIV4); //why slow it down?
    
    spi->begin();
    
    idData = ReadIDdata();
    
    //check it's an Adesto
    if(idData.manufacturerID != 0x1F) SerialUSB.print("Wrong manufacturer!");
    
    //check AT45D series
    if((idData.deviceID1 & 0xE0) != 0x20) SerialUSB.print("Wrong chip family!");

    byteCount = (uint32_t)1 << (15 + (idData.deviceID1 & 0x1F));

    //hard-coded for now...
    bytesPerPage = 512;
    bytesPerBlock = 4096;
    totalPages = byteCount / bytesPerPage;
        
    return idData;
}

uint8_t FlashAT45DB321E::IsReady(void)
{
    uint16_t status = ReadStatus();
    return status & STATUS_RDY;
}

uint8_t FlashAT45DB321E::IsBusy(void)
{
    return !IsReady();
}

IDdata FlashAT45DB321E::ReadIDdata(void)
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

uint16_t FlashAT45DB321E::ReadStatus(void)
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

uint32_t FlashAT45DB321E::BufferWrite(uint8_t* data, uint16_t count, uint8_t bufferNumber, uint16_t byteAddress)
{
    //84h for Buffer 1 or 87h for Buffer 2
    uint8_t op_code = 0x00;
    if(bufferNumber == 1) op_code = 0x84;
    else if(bufferNumber == 2) op_code = 0x87;
    else return 0;
    
    Select();
    SendCommand(op_code);
    SendAddress(MakeAddress(0x00, byteAddress)); //writing to buffer, page address is irrelevant
    
    uint16_t i = 0;
    for( ; i < count; i++)
    {
        spi->transfer(data[i]);
    }
    
    Deselect();
    
    return i;
}

uint32_t FlashAT45DB321E::WriteThruBuffer(uint8_t* data, uint16_t count, uint8_t bufferNumber,
                                          uint16_t pageIndex, uint16_t byteAddress)
{
    //84h for Buffer 1 or 87h for Buffer 2
    uint8_t op_code = 0x00;
    if(bufferNumber == 1) op_code = 0x84;
    else if(bufferNumber == 2) op_code = 0x87;
    else return 0;
    
    Select();
    SendCommand(op_code);
    SendAddress(MakeAddress(pageIndex, byteAddress));
    
    uint16_t i = 0;
    for( ; i < count; i++)
    {
        spi->transfer(data[i]);
    }
    
    Deselect();
    
    return i;
}

uint8_t FlashAT45DB321E::WriteBufferToPage(uint8_t bufferNumber, uint32_t pageIndex, bool erase)
{
    //with erase:       83h for Buffer 1 or 86h for Buffer 2
    //without erase:    88h for Buffer 1 or 89h for Buffer 2
    uint8_t op_code = 0x00;
    
    if(bufferNumber == 1 && erase) op_code = 0x83;
    else if(bufferNumber == 2 && erase) op_code = 0x86;
    else if(bufferNumber == 1 && !erase) op_code = 0x88;
    else if(bufferNumber == 2 && !erase) op_code = 0x89;
    else return 0;
    
    Select();
    SendCommand(op_code);
    SendAddress(MakeAddress(pageIndex));
    Deselect();
    
    return 1;
}
    
/*
 * To perform a Continuous Array Read using the binary page size (512 bytes),
 * the opcode 0Bh must be clocked into the device followed by three address bytes (A21 - A0)
 * and one dummy byte.
 *
 * it's up to the user to declare data to be the correct size
 */
uint32_t FlashAT45DB321E::ReadData(uint32_t address, uint8_t* data, uint32_t count)
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

//Write() allows the user to just write a stream of data without concerns for the underlying structure
uint32_t FlashAT45DB321E::Write(uint8_t* data, uint16_t size)
{
    //84h for Buffer 1 or 87h for Buffer 2
    uint8_t op_code = 0x84; //defaults to buffer 1
    if(currBuffer == 2) op_code = 0x87;
    
    Select();
    SendCommand(op_code);
    SendAddress(MakeAddress(0x00, byteAddress)); //writing to buffer, page address is irrelevant

    uint16_t i = 0;
    while(i < size)
    {
        spi->transfer(data[i++]);
        
        if(++currBufferIndex == bytesPerPage) //if buffer is full, write the buffer and switch to other one
        {
            Deselect();
            
            WriteBufferToPage(currBuffer, currPageIndex, false); //assumes already erased!!!
            
            if(currBuffer == 1)
            {
                currBuffer = 2;
                op_code = 0x87;
            }
            else
            {
                currBuffer = 1;
                op_code = 0x84;
            }

            //pick up with the other buffer (can be done while the one is writing)
            currBufferIndex = 0;
            
            Select();
            SendCommand(op_code);
            SendAddress(MakeAddress(0x00, byteAddress)); //writing to buffer, page address is irrelevant
        }
    }
    
    Deselect(); //this won't write the buffer to flash: could lose the last page of data
}


uint8_t FlashAT45DB321E::EraseBlock(uint32_t address, uint8_t sizeCmd)
{
    if(address >= byteCount) return 0; //basic check for address range
    
    Select();
    SendCommand(sizeCmd);
    SendAddress(address);
    Deselect();
    
    while(IsBusy()) {}
    
    return 1;
}

uint8_t FlashAT45DB321E::EraseBlock4K(uint32_t address)
{
    return EraseBlock(address, CMD_ERASE_BLOCK_4K);
    
}

uint8_t FlashAT45DB321E::EraseSector(uint32_t address)
{
    return EraseBlock(address, CMD_ERASE_SECTOR);
    
}
