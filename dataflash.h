//
//  dataflash.h
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#ifndef dataflash_h
#define dataflash_h

#include <flash.h>

#include <TArray.h>
#include <TList.h>

#define BufferArray TArray<uint8_t>

#define MAX_STORES 16

//not sure I really need this -- why not just a buffer?
struct Datapage
{
    TArray<uint8_t> buffer;
    Datapage(uint16_t size) : buffer(size) {}
};

/*
 * A Datastore is essentially a 'file' of data, but since this isn't a file system, per se,
 * let's call it a 'store'.
 */
struct Datastore
{
protected:
    uint8_t storeNumber = 0xff;
    uint32_t startAddress = -1;
    uint32_t nextAddress = 0; //address of NEXT page of store (first free page)
    uint32_t currAddress = 0; //for maintaining a pointer during reads
    uint16_t pages = 0;
    
public:
    Datastore(void) : storeNumber(-1) {}
    Datastore(uint8_t number, uint32_t startAddr = 0)
    {
        storeNumber = number;
        startAddress = startAddr;
        nextAddress = startAddr;
        currAddress = startAddr;
    }
 
    String Display(void)
    {
        String str = String(storeNumber) + '\t' + String(pages) + '\t' + String(startAddress) + '\t' + String(nextAddress) + '\n';
        return str;
    }
    
    bool operator == (const Datastore& store) { return storeNumber == store.storeNumber; }
    
    uint32_t Rewind(void) { return currAddress = startAddress; }
    friend class DataFlash;
    friend class Flashstore;
};

class DataFlash : public FlashAT25DF641A
{
protected:
//    FlashAT25DF641A flash;
    TArray<Datastore> stores;
    uint32_t lastAddress = 0; //this will be the next free address for making the next store
public:
    DataFlash(SPIClass* _spi, uint8_t cs) : FlashAT25DF641A(_spi, cs), stores(MAX_STORES) {}
    uint16_t FindStores(void);
    void DisplayStores(void);
    uint16_t EraseStore(uint8_t storeNumber);
    uint32_t CreateStore(uint8_t storeNumber);
    uint16_t AddRecord(uint8_t storeNumber, uint8_t* data, uint16_t size);
};

class Flashstore : public FlashAT25DF641A
{
protected:
    TList<Datastore> storeList;
    //Datastore* currStore = NULL; //not needed, since the current store

    BufferArray ReadPage(uint32_t address);
    //uint32_t WritePage(const BufferArray& buffer);
public:
    Flashstore(SPIClass* _spi, uint8_t cs) : FlashAT25DF641A(_spi, cs) {}
    
    TList<Datastore> ReadStoresFromFlash(void);
    TList<Datastore> ListStores(void);
    
    Datastore* CreateStore(uint8_t);
    uint16_t EraseStore(uint8_t storeNumber);
    
    BufferArray ReadPageFromStore(uint8_t storeNumber, bool rewind = false); //returns empty buffer when done?
    Datastore* WritePageToCurrentStore(BufferArray buffer);
};

#endif /* dataflash_h */
