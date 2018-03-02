//
//  dataflash.cpp
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#include <dataflash.h>

uint16_t FlashStoreManager::ReadStoresFromFlash(void)
{
    if(!flash) return 0;

    storeList.Flush();
    
    //read the FAT
    uint16_t maxStores = flash->bytesPerBlock / 8;
    
    for(uint16_t index = 0; index < maxStores; index++)
    {
        //get start address
        BufferArray fileInfo(8);
        flash->ReadBytes(index * 8, &fileInfo[0], fileInfo.GetSize());
        
        uint32_t start = 0;
        memcpy(&start, &fileInfo[0], 4);
        
        uint32_t end = 0;
        memcpy(&end, &fileInfo[4], 4);
        
        if(start != 0xffffffff)
        {
            storeList.Add(Datastore(index, start, end, end + 1 - start));
        }
    }
    
    return storeList.GetItemsInContainer();
}

uint32_t FlashStoreManager::OpenStore(uint16_t storeNumber, uint32_t sizeReq)
{
    //check if file number is valid
    //first block acts as rudimentary FAT; 8 bytes per store => max file is blocksize / 8
    uint16_t maxStoreNum = flash->bytesPerBlock / 8;
    if(storeNumber > maxStoreNum) return 0;
    
    //check if file number is available
    //first refresh list
    ReadStoresFromFlash();
    
    //then check if the store exists
    currStore = storeList.Find(Datastore(storeNumber));
    if(currStore) return currStore->endAddress + 1 - currStore->startAddress; //need to add ability to resize...
    
    //didn't find it, so create new one if sizeReq > 0 (otherwise, we're just selecting)
    if(sizeReq) return CreateStore(storeNumber, sizeReq);
    else return 0;
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
    
    Datastore* lastStore = storeList.GetTail();
    if(lastStore) firstFreeMem = lastStore->startAddress + lastStore->reservedSize;
    
    //make sure we start on a block boundary
    if(firstFreeMem % flash->bytesPerBlock)
    {
        firstFreeMem = flash->bytesPerBlock * (firstFreeMem / flash->bytesPerBlock + 1);
    }
    
    //check for space
    //make sizeReq integral number of blocks
    uint16_t blocks = sizeReq / flash->bytesPerBlock;
    if(sizeReq % flash->bytesPerBlock) blocks++;
    sizeReq = blocks * flash->bytesPerBlock;
    
    if((flash->byteCount - firstFreeMem) < sizeReq) return 0; //could be made a lot smarter...
    
    //SerialUSB.println(firstFreeMem);
    
    //if we've made it this far, we can make a store
    //create the FAT entry
    Datastore newStore(fileNum, firstFreeMem, firstFreeMem - 1, sizeReq);
    //newStore.Rewind();
    
    currStore = storeList.Add(newStore);
    
    BufferArray storeInfo(8);
    memcpy(&storeInfo[0], &newStore.startAddress, 4);
    memcpy(&storeInfo[4], &newStore.endAddress, 4);
    flash->WriteBytes(fileNum * 8, storeInfo);
    
    //SerialUSB.println(newStore.startAddress);
    
    //erase the relevant memory
    flash->Erase(newStore.startAddress, sizeReq);
    
    return newStore.reservedSize;
}

uint32_t FlashStoreManager::Resize(uint32_t newSize)
{
    if(!currStore) return 0;
    if(!flash) return 0;
    
    uint32_t bytesPerBlock = flash->bytesPerBlock;
    uint16_t blocks = newSize / bytesPerBlock;
    if(newSize % bytesPerBlock) blocks++;
    return currStore->reservedSize = blocks * bytesPerBlock;
}

uint32_t FlashStoreManager::RewindStore(void) //could take an argument?
{
    //Datastore* currStore = storeList.Find(Datastore(storeNum));
    if(!currStore)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }
    
    return currStore->currAddress = currStore->startAddress;
}

uint32_t FlashStoreManager::CloseStore(void) //truncates reserved Size to current block; could take argument?
{
    //Datastore* currStore = storeList.Find(Datastore(storeNum));
    if(!currStore)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }
    
        //store the actual start and end of the store
        BufferArray storeInfo(8);
        memcpy(&storeInfo[0], &currStore->startAddress, 4);
        memcpy(&storeInfo[4], &currStore->endAddress, 4);
        if(flash) flash->WriteBytes(currStore->storeNumber * 8, storeInfo);
        else return 0;
        
        //now change the reserved size to an integral number of blocks
        uint32_t currSize = currStore->endAddress + 1 - currStore->startAddress;
        
        //make currSize integral number of blocks
        uint16_t blocks = currSize / flash->bytesPerBlock;
        if(currSize % flash->bytesPerBlock) blocks++;
        currSize = blocks * flash->bytesPerBlock;
        
        currStore->reservedSize = currSize;
        //store->currAddress = store->endAddress + 1; //move to the end to avoid overwrites...
        
        return currStore->reservedSize;
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
    
    deletedByteCount = flash->Erase(store->startAddress, store->reservedSize);
    BufferArray storeInfo(8);
    for(uint8_t i = 0; i < 8; i++) storeInfo[i] = 0xff;
    flash->WriteBytes(storeNumber * 8, storeInfo);
    
    ReadStoresFromFlash();
    
    return deletedByteCount;
}

uint32_t FlashStoreManager::Write(const BufferArray& buffer)
{
    if(!currStore)
    {
        SerialUSB.println("No open store");
        return 0;
    }
    
    if(!flash)
    {
        SerialUSB.println("No open flash");
        return 0;
    }
    
    uint32_t currSize = currStore->endAddress + 1 - currStore->startAddress;
    if(currSize + buffer.GetSize() > currStore->reservedSize) //really should write what it can
    {
        SerialUSB.println("Overrun!");
        return 0;
    }
    
    uint32_t byteCount = flash->Write(currStore->endAddress + 1, buffer);
    currStore->endAddress += byteCount; //should do some error checking/handling
    
    //note that if we're using a flash with a buffer, there needs to be some cleaning up: there
    //are bytes in the buffer that haven't been written to the NVM
    
    return byteCount;
}

uint32_t FlashStoreManager::Read(BufferArray& buffer)
{
    if(!currStore) return 0;
    
    if(currStore->currAddress > currStore->endAddress) return 0; //should really check if buffer is longer than remaining bytes...
    
    uint32_t bytes = flash->ReadBytes(currStore->currAddress, &buffer[0], buffer.GetSize());
    currStore->currAddress += bytes;
    
    SerialUSB.println(currStore->currAddress);
    
    return bytes;
}


