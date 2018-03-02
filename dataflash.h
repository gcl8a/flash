//
//  dataflash.h
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

/*
 * In this version, Datastore is simply a struct with little functionality. FlashManager keeps track of the
 * open store and reads, writes, etc.
 *
 * The basic workflow is open, write, close. Once closed, the store is done, unless reopened with more memory.
 * That is, this isn't a true file system -- just a way to quickly collect and store data, which can then be
 * written to an SD card later.
 *
 * Do not open and close a file repeatedly, or you'll burn out the (very rudimentary) FAT. The first sector of the
 * flash is used to keep track of the various stores, so every time you close a store, it writes to that sector. Not
 * the prettiest, but again, this is just for rapid data collection. (I suppose we don't even really need the "FAT" --
 * but it makes it easy to reset the program without losing store metadata. An alternative -- maybe later -- would be
 * to just make the first byte of each page a store number, but then it's more harder to append data...or maybe not,
 * but that'll have to wait.)
 */

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
    //static Flash* flash;
    
    uint16_t storeNumber = 0xffff;  //16-bit is overkill, but there it is...
    uint32_t startAddress = -1;     //start of the store, obviously
    uint32_t endAddress = -1;       //last byte that has been written to the store, inclusive
    uint32_t reservedSize = 0;      //this is the space that has been pre-erased; the store is smaller; see endAddress
    uint32_t currAddress = -1;      //used for playback; writing is always done by appending

    //these are protected, but FlashManager accesses as a friend
    Datastore(void) : storeNumber(-1) {}
    Datastore(uint16_t number) : storeNumber(number) {}
    Datastore(uint16_t number, uint32_t startAddr, uint32_t endAddr, uint32_t size)
    {
        storeNumber = number;
        startAddress = startAddr;
        endAddress = endAddr;
        currAddress = startAddr;

        reservedSize = size;
    }
    
public:

    String Display(void)
    {
        uint32_t size = endAddress + 1 - startAddress;
        size /= 1024;
        
        String str = String(storeNumber) + '\t' + String(size) + String("kB \t") + String(startAddress) + '\t' + String(currAddress) + '\t' + String(endAddress) + '\t' + String(reservedSize) + '\n';
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
    TSList<Datastore> storeList; //this is not a fully functional file system; just a list of stores
    Datastore* currStore = NULL;

public:
    FlashStoreManager(Flash* fl) : flash(fl) {}
    IDdata Init(void)
    {
        if(!flash) return IDdata();
        else return flash->Init();
    }

    TListIterator<Datastore> GetStoresIterator(bool refresh)
    {
        if(refresh) ReadStoresFromFlash();
        return TListIterator<Datastore>(storeList);
    }
    
    uint16_t ReadStoresFromFlash(void);

    uint32_t OpenStore(uint16_t storeNumber, uint32_t sizeReq = 0); //passing 0 is the equivalent of Select()

protected:
    uint32_t CreateStore(uint16_t fileNum, uint32_t sizeReq);
    uint32_t Resize(uint32_t newSize); //resizes current store; add argument?
    
public:
    uint32_t RewindStore(void); //rewinds current store; add argument?
    uint32_t CloseStore(void); //truncates current store; add argument?
    uint32_t DeleteStore(uint16_t); //remove argument?
    
    uint32_t Write(const BufferArray&); // writes to current store
    uint32_t Read(BufferArray&); //reads from current store
};

#endif /* dataflash_h */
