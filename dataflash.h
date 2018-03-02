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
class Datastore
{
protected:
    static Flash* flash;
    
    uint16_t storeNumber = 0xffff;  //16-bit is overkill, but there it is...
    uint32_t startAddress = -1;     //start of the store, obviously
    uint32_t endAddress = -1;       //last byte that has been written to the store, inclusive
    uint32_t reservedSize = 0;      //this is the space that has been pre-erased; the store is smaller; see endAddress
    uint32_t currAddress = -1;      //used for playback; writing is always done by appending

public:
    Datastore(void) : storeNumber(-1) {}
    Datastore(uint16_t number) : storeNumber(number) {}
    Datastore(uint16_t number, uint32_t startAddr, uint32_t size)
    {
        storeNumber = number;
        startAddress = startAddr;
        endAddress = startAddr - 1;
        currAddress = startAddr;

        Resize(size);
    }
 
    uint32_t WriteBytes(const BufferArray& buffer);
    uint32_t ReadBytes(BufferArray& buffer);

    uint32_t Rewind(void)
    {
        return currAddress = startAddress;
    }
    
    uint32_t Resize(uint32_t newSize)
    {
        uint32_t bytesPerBlock = flash->GetBytesPerBlock();
        uint16_t blocks = newSize / bytesPerBlock;
        if(newSize % bytesPerBlock) blocks++;
        return reservedSize = blocks * bytesPerBlock;
    }
    
    uint32_t Close(void);
    
    String Display(void)
    {
        uint32_t size = endAddress + 1 - startAddress;
        size /= 1024;
        
        String str = String(storeNumber) + '\t' + String(size) + String("kB \t") + String(startAddress) + '\t' + String(currAddress) + '\t' + String(endAddress) + '\n';
        return str;
    }
    
    bool operator == (const Datastore& store) { return storeNumber == store.storeNumber; }
    bool operator > (const Datastore& store) { return startAddress > store.startAddress; }

    friend class FlashStoreManager;
};

class FlashStoreManager// : virtual Flash
{
protected:
    //Flash* flash = NULL; //pointer to the flash memory -- this let's us swap physical memory more easily than deriving
    TSList<Datastore> storeList; //note that the "current" store is always the last one -- this is not a file system
    //Datastore* currStore = NULL;
    
    uint32_t CreateStore(uint16_t fileNum, uint32_t sizeReq);
public:
    FlashStoreManager(Flash* fl) {Datastore::flash = fl;}
    void Init(void)
    {
        if(!Datastore::flash) return;
        Datastore::flash->Init();
    }

    //uint32_t Select(uint16_t storeNumber);
    Datastore* OpenStore(uint16_t storeNumber, uint32_t sizeReq = 0);

    uint16_t ReadStoresFromFlash(void);
    TListIterator<Datastore> GetStoresIterator(bool refresh)
    {
        if(refresh) ReadStoresFromFlash();
        return TListIterator<Datastore>(storeList);
    }
    
    uint32_t DeleteStore(uint16_t);
    uint32_t RewindStore(uint16_t); //move to Datastore
    uint32_t CloseStore(uint16_t); //truncates; move to Datastore?

    uint32_t Write(uint16_t, const BufferArray&); //obsolete
    uint32_t Read(uint16_t, BufferArray&); //obsolete
};

#endif /* dataflash_h */
