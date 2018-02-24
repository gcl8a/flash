//
//  dataflash.cpp
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#include <dataflash.h>

uint16_t FlashStoreManager::ReadStoresFromFlash(void)
{
    storeList.Flush();
    
    //read the FAT
    uint16_t maxStores = flash->bytesPerBlock / 8;
    for(uint16_t index = 0; index < maxStores; index++)
    {
        //get start address
        BufferArray fileInfo(8);
        flash->ReadBytes(index * 8, &fileInfo[0], fileInfo.GetSize());
        
        uint32_t start = -1;
        memcpy(&fileInfo[0], &start, 4);
        
        uint32_t end = -1;
        memcpy(&fileInfo[4], &end, 4);
        
        if(start != 0xffffffff)
        {
            storeList.Add(Datastore(index, start, end));
        }
    }
    
    return storeList.GetItemsInContainer();
}

uint32_t FlashStoreManager::Select(uint16_t storeNumber)
{
    currStore = storeList.Find(Datastore(storeNumber));
    if(currStore) return currStore->endAddress - currStore->firstAddress; //available size
    else return 0;
}

uint32_t FlashStoreManager::Write(const BufferArray& buffer)
{
    //Datastore* store = storeList.Find(Datastore(storeNumber));
    if(!currStore) return 0;
    
    uint32_t byteCount = flash->Write(currStore->currAddress, buffer); //somewhere needs to check for overrun...
    currStore->currAddress += byteCount;
    
    return byteCount;
}

uint32_t FlashStoreManager::DeleteStore(uint16_t storeNumber)
{
    uint32_t deletedByteCount = 0;
    Datastore* store = storeList.Find(Datastore(storeNumber));
    if(!store)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }

    deletedByteCount = flash->Erase(store->startAddress, store->size);
    BufferArray storeInfo(8);
    for(uint8_t i = 0; i < 8; i++) storeInfo[i] = 0xff;
    flash->Write(storeNumber * 8, storeInfo);

    ReadStoresFromFlash();
    
    return deletedByteCount;
}

uint32_t FlashStoreManager::CreateStore(uint16_t fileNum, uint32_t sizeReq)
{
    //check if file number is valid
    //first block acts as rudimentary FAT; 8 bytes per store => max file is blocksize / 8
    uint16_t maxFileNum = flash->bytesPerBlock / 8;
    if(fileNum > maxFileNum) return 0;
    
    //check if file number is available
    //first refresh list
    ReadStoresFromFlash();
    //then check if the number is taken
    if(storeList.Find(Datastore(fileNum))) return 0;
    
    //now find an chunk of unallocated memory
    //for now, just check if there is memory after the last file (which will help distribute writes, as well)
    uint32_t firstFreeMem = flash->bytesPerBlock; //first block reserved for "FAT"
    if(storeList.GetTail()) firstFreeMem = storeList.GetTail()->endAddress + 1;
    
    //check for space
    //make sizeReq integral number of blocks
    uint16_t blocks = sizeReq / flash->bytesPerBlock;
    if(sizeReq % flash->bytesPerBlock) blocks++;
    sizeReq = blocks * flash->bytesPerBlock;
    
    if((flash->byteCount - firstFreeMem) < sizeReq) return 0; //could be made a lot smarter...
    
    //if we've made it this far, we can make a store
    //create the FAT entry
    Datastore newStore(fileNum, firstFreeMem, firstFreeMem + sizeReq);
    storeList.Add(newStore);

    BufferArray storeInfo(8);
    memcpy(&storeInfo[0], &newStore.startAddress, 4);
    memcpy(&storeInfo[4], &newStore.endAddress, 4);
    flash->Write(fileNum * 8, storeInfo);
    
    //erase the relevant memory
    flash->Erase(newStore.startAddress, sizeReq);
    
    return Select(fileNum);
}
