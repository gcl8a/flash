//
//  flash.h
//  flash
//
//  Created by Gregory C Lewin on 12/26/17.
//  Copyright © 2017 Gregory C Lewin. All rights reserved.
//

#ifndef flash_h
#define flash_h

#include <Arduino.h>
#include <SPI.h>

#include <TArray.h>

#define BufferArray TArray<uint8_t> //basic structure for reading and writing

struct IDdata
{
    uint8_t manufacturerID = 0;
    uint8_t deviceID1 = 0;
    uint8_t deviceID2 = 0;
    
    uint8_t extLength = 0;
    //should really read the extended data, but we'll leave blank for now...
};

class Flash
{
protected:
    SPIClass* spi = NULL;
    uint8_t chipSelect = -1;
    
    //number of pages and number of total bytes
    //uint16_t totalPages = 0;
    uint32_t byteCount = 0;
    uint16_t bytesPerPage = 0;
    uint16_t bytesPerBlock = 0;
    
    IDdata idData;

public:
    Flash(void) {}
    Flash(SPIClass* _spi, uint8_t cs) : spi(_spi), chipSelect(cs) {}
    
    void Init(void)
    {
        pinMode(chipSelect, OUTPUT);        
        Deselect();
        
        //this might not play well with others...
        spi->setDataMode(SPI_MODE0);
        spi->setBitOrder(MSBFIRST);
        spi->setClockDivider(SPI_CLOCK_DIV4); //SAMD21 is too fast? or maybe I need better wiring
        
        spi->begin();
    }

    void Select(void) { digitalWrite(chipSelect, LOW); }
    void Deselect(void) { digitalWrite(chipSelect, HIGH); }
    
    void SendCommand(uint8_t cmd)
    {
        spi->transfer(cmd);
    }
    
    virtual uint32_t Write(uint32_t, const BufferArray&); //= 0;
    virtual uint32_t ReadBytes(uint32_t address, uint8_t* data, uint32_t count);
    
    uint32_t Erase(uint32_t addr, uint32_t size);

    friend class FlashStoreManager;
};

class FlashAT25DF641A : public Flash
{
protected:
    void SendAddress(uint32_t addr)
    {
        spi->transfer(addr >> 16);
        spi->transfer(addr >>  8);
        spi->transfer(addr      );
    }
    
public:
    FlashAT25DF641A(SPIClass* _spi, uint8_t cs) : Flash(_spi, cs)
    {
        //might not work in the constructor?
        pinMode(chipSelect, OUTPUT);
        Deselect();
    }
    
    IDdata Init(void);
    
    uint8_t IsBusy(void);    
    IDdata ReadIDdata(void);
    uint16_t ReadStatus(void);
    
    /*
     * it's up to the user to declare data to be the correct size
     */
    uint32_t ReadBytes(uint32_t address, uint8_t* data, uint32_t count);

    //uint8_t ReadByte(uint32_t address);
    
    uint8_t WriteEnable(void);
    uint8_t WriteDisable(void);
    uint16_t WritePage(uint32_t address, uint8_t* data, uint16_t count);
    
    uint8_t EraseBlock(uint32_t, uint8_t);
    uint8_t EraseBlock4K(uint32_t address);
    uint8_t EraseBlock32K(uint32_t address);
    uint8_t EraseBlock64K(uint32_t address);

    uint8_t ReadSectorProtectionStatus(uint32_t address);
    void GlobalUnprotect(void);
    void GlobalProtect(void);
};

class FlashAT45DB321E : public Flash
{
protected:
    //uint16_t currBufferIndex = 0; //byte index within a buffer [0..511]
    uint8_t currBuffer = 1; //SRAM buffer = 1 or 2
    
    void SendAddress(uint32_t address)
    {
        spi->transfer(address >> 16);
        spi->transfer(address >>  8);
        spi->transfer(address      );
    }
                                    
public:
    FlashAT45DB321E(SPIClass* _spi, uint8_t cs) : Flash(_spi, cs)
    {
        //might not work in the constructor?
        pinMode(chipSelect, OUTPUT);
        Deselect();
    }
    
    IDdata Init(void);
    
    uint8_t IsReady(void);
    uint8_t IsBusy(void);
    IDdata ReadIDdata(void);
    uint16_t ReadStatus(void);
    
    uint32_t Write(uint32_t addr, const BufferArray&);
    uint32_t ReadBytes(uint32_t address, uint8_t* data, uint32_t count);
    
    uint32_t BufferWrite(uint8_t* data, uint16_t count, uint8_t bufferNumber, uint16_t byteAddress);
    uint8_t WriteBufferToPage(uint8_t bufferNumber, uint32_t pageIndex, bool erase = false);

    uint32_t EraseBlock(uint32_t address) {return EraseBlock4K(address);}
    
    uint8_t EraseBlock(uint32_t, uint8_t);
    uint8_t EraseBlock4K(uint32_t address);
    uint8_t EraseSector(uint32_t address);

    /////////unused?
//    uint32_t WriteThruBuffer(uint8_t* data, uint16_t count, uint8_t bufferNumber,
//                                              uint16_t pageIndex, uint16_t byteAddress);
};

#endif /* flash_h */
