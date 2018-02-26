//
//  dataflash.h
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#ifndef dataflash_h
#define dataflash_h

#include <flash.h>
#include <TList.h>

/*
 * A Datastore is essentially a 'file' of data, but since this isn't a file system, per se,
 * let's call it a 'store'.
 */
struct Datastore
{
protected:
    uint16_t storeNumber = 0xffff;
    uint32_t startAddress = -1;
    uint32_t endAddress = -1; //last byte, inclusive
    uint32_t size = 0; //in bytes, since page sizes vary by flash chip...REDUNDANT!!!
    uint32_t currAddress = -1;
    
public:
    Datastore(void) : storeNumber(-1) {}
    Datastore(uint16_t number) : storeNumber(number) {}
    Datastore(uint16_t number, uint32_t startAddr, uint32_t endAddr)
    {
        storeNumber = number;
        startAddress = startAddr;
        endAddress = endAddr;
        size = endAddr - startAddr + 1;
        currAddress = endAddress;
    }
 
    uint32_t Rewind(void)
    {
        return currAddress = startAddress;
    }
    
    uint32_t Resize(uint32_t newSize)
    {
        size = newSize;
        return endAddress = startAddress + size - 1;
    }
    
    String Display(void)
    {
        String str = String(storeNumber) + '\t' + String(size/1024) + String("kB \t") + String(startAddress) + '\t' + String(currAddress) + '\t' + String(endAddress) + '\n';
        return str;
    }
    
    bool operator == (const Datastore& store) { return storeNumber == store.storeNumber; }
    bool operator > (const Datastore& store) { return startAddress > store.startAddress; }

    friend class FlashStoreManager;
};

class FlashStoreManager// : virtual Flash
{
protected:
    Flash* flash = NULL; //pointer to the flash memory -- this let's us swap physical memory more easily than deriving
    TSList<Datastore> storeList; //note that the "current" store is always the last one -- this is not a file system
    Datastore* currStore = NULL;
    
public:
    FlashStoreManager(Flash* fl) : flash(fl) {}
    void Init(void)
    {
        if(!flash) return;
        flash->Init();
    }

    uint32_t Select(uint16_t storeNumber);

    uint16_t ReadStoresFromFlash(void);
    TListIterator<Datastore> GetStoresIterator(bool refresh)
    {
        if(refresh) ReadStoresFromFlash();
        return TListIterator<Datastore>(storeList);
    }
    
    uint32_t CreateStore(uint16_t fileNum, uint32_t sizeReq);
    uint32_t DeleteStore(uint16_t);
    uint32_t RewindStore(uint16_t);
    uint32_t CloseStore(uint16_t);

    uint32_t Write(const BufferArray&);
    uint32_t ReadBytes(uint16_t, BufferArray&);
    uint32_t ReadBytes(BufferArray& buffer)
    {
        if(currStore) return ReadBytes(currStore->storeNumber, buffer);
        else return 0;
    }
};

#endif /* dataflash_h */
