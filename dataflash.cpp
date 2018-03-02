//
//  dataflash.cpp
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#include <dataflash.h>

Flash* Datastore::flash = NULL;// = new FlashAT45DB321E(&SPI, FLASH_CS);

uint32_t Datastore::WriteBytes(const BufferArray& buffer)
{
    uint32_t currSize = endAddress - startAddress;
    if(currSize + buffer.GetSize() > reservedSize)
    {
        SerialUSB.println("Overrun!");
        return 0;
    }
    
    uint32_t byteCount = 0;
    
    if(flash)
    {
        byteCount = flash->Write(endAddress + 1, buffer);
        endAddress += byteCount; //should do some error checking/handling
    }
    
    return byteCount;
}

uint32_t Datastore::ReadBytes(BufferArray& buffer)
{
    if(currAddress > endAddress) return 0;
    
    uint32_t byteCount = 0;
    
    if(flash)
    {
        byteCount = flash->ReadBytes(currAddress, &buffer[0], buffer.GetSize());
        currAddress += byteCount;
    }
    
    return byteCount;
}

uint32_t Datastore::Close(void)
{
    //store the actual start and end of the store
    BufferArray storeInfo(8);
    memcpy(&storeInfo[0], &startAddress, 4);
    memcpy(&storeInfo[4], &endAddress, 4);
    if(flash) flash->WriteBytes(storeNumber * 8, storeInfo);
    else return 0;
    
    //now change the reserved size to an integral number of blocks
    uint32_t currSize = endAddress + 1 - startAddress;
    
    //make currSize integral number of blocks
    uint16_t blocks = currSize / flash->GetBytesPerBlock();
    if(currSize % flash->GetBytesPerBlock()) blocks++;
    currSize = blocks * flash->GetBytesPerBlock();
    
    Resize(currSize);
    //store->currAddress = store->endAddress + 1; //move to the end to avoid overwrites...
    
    return reservedSize;
}

uint16_t FlashStoreManager::ReadStoresFromFlash(void)
{
    if(!Datastore::flash) return 0;

    storeList.Flush();
    
    //read the FAT
    uint16_t maxStores = Datastore::flash->bytesPerBlock / 8;
    
    for(uint16_t index = 0; index < maxStores; index++)
    {
        //get start address
        BufferArray fileInfo(8);
        Datastore::flash->ReadBytes(index * 8, &fileInfo[0], fileInfo.GetSize());
        
        uint32_t start = 0;
        memcpy(&start, &fileInfo[0], 4);
        
        uint32_t end = 0;
        memcpy(&end, &fileInfo[4], 4);
        
        if(start != 0xffffffff)
        {
            storeList.Add(Datastore(index, start, end + 1 - start));
        }
    }
    
    return storeList.GetItemsInContainer();
}
//
//uint32_t FlashStoreManager::Write(uint16_t storeNum, const BufferArray& buffer)
//{
//    Datastore* currStore = storeList.Find(Datastore(storeNum));
//    if(!currStore)
//    {
//        SerialUSB.println("No open store");
//        return 0;
//    }
//
//    uint32_t currSize = currStore->endAddress + 1 - currStore->startAddress;
//    if(currSize + buffer.GetSize() > currStore->reservedSize)
//    {
//        SerialUSB.print(currStore->reservedSize);
//        SerialUSB.print('\t');
//        SerialUSB.println("Overrun!");
//        return 0;
//    }
//
////    SerialUSB.println(currStore->currAddress);
//
//    uint32_t byteCount = Datastore::flash->Write(currStore->endAddress + 1, buffer);
//    currStore->endAddress += byteCount; //should do some error checking/handling
//
//    return byteCount;
//}

uint32_t FlashStoreManager::DeleteStore(uint16_t storeNumber)
{
    uint32_t deletedByteCount = 0;
    Datastore* store = storeList.Find(Datastore(storeNumber));
    if(!store)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }

    deletedByteCount = Datastore::flash->Erase(store->startAddress, store->reservedSize);
    BufferArray storeInfo(8);
    for(uint8_t i = 0; i < 8; i++) storeInfo[i] = 0xff;
    Datastore::flash->WriteBytes(storeNumber * 8, storeInfo);

    ReadStoresFromFlash();
    
    return deletedByteCount;
}

uint32_t FlashStoreManager::RewindStore(uint16_t storeNum)
{
    Datastore* store = storeList.Find(Datastore(storeNum));
    if(!store)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }
    
    return store->Rewind();
}

uint32_t FlashStoreManager::CloseStore(uint16_t storeNum) //truncates reserved Size to current block
{
    Datastore* store = storeList.Find(Datastore(storeNum));
    if(!store)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }
    
    return store->Close();
//
//    //store the actual start and end of the store
//    BufferArray storeInfo(8);
//    memcpy(&storeInfo[0], &store->startAddress, 4);
//    memcpy(&storeInfo[4], &store->endAddress, 4);
//    Datastore::flash->WriteBytes(storeNum * 8, storeInfo);
//    
//    //now change the reserved size to an integral number of blocks
//    uint32_t currSize = store->endAddress + 1 - store->startAddress;
//    
//    //make currSize integral number of blocks
//    uint16_t blocks = currSize / Datastore::flash->bytesPerBlock;
//    if(currSize % Datastore::flash->bytesPerBlock) blocks++;
//    currSize = blocks * Datastore::flash->bytesPerBlock;
//    
//    store->Resize(currSize);
//    //store->currAddress = store->endAddress + 1; //move to the end to avoid overwrites...
//    
//    return store->reservedSize;
}

Datastore* FlashStoreManager::OpenStore(uint16_t storeNumber, uint32_t sizeReq)
{
    //check if file number is valid
    //first block acts as rudimentary FAT; 8 bytes per store => max file is blocksize / 8
    uint16_t maxStoreNum = Datastore::flash->bytesPerBlock / 8;
    if(storeNumber > maxStoreNum) return NULL;
    
    //check if file number is available
    //first refresh list
    ReadStoresFromFlash();
    
    //then check if the store exists
    Datastore* retStore = storeList.Find(Datastore(storeNumber));
    if(retStore) return retStore; //need to add ability to resize...

    //didn't find it, so create new one
    CreateStore(storeNumber, sizeReq);
    
    return storeList.Find(Datastore(storeNumber));
}

uint32_t FlashStoreManager::CreateStore(uint16_t fileNum, uint32_t sizeReq)
{
    //check if file number is valid
    //first block acts as rudimentary FAT; 8 bytes per store => max file is blocksize / 8
    uint16_t maxFileNum = Datastore::flash->bytesPerBlock / 8;
    if(fileNum > maxFileNum) return 0;
    
    //check if file number is available
    //first refresh list
    ReadStoresFromFlash();
    //then check if the number is taken
    if(storeList.Find(Datastore(fileNum))) return 0;
    
    //now find an chunk of unallocated memory
    //for now, just check if there is memory after the last file (which will help distribute writes, as well)
    uint32_t firstFreeMem = Datastore::flash->bytesPerBlock; //first block reserved for "FAT"
    
    Datastore* lastStore = storeList.GetTail();
    if(lastStore) firstFreeMem = lastStore->startAddress + lastStore->reservedSize;
    
    //check for space
    //make sizeReq integral number of blocks
    uint16_t blocks = sizeReq / Datastore::flash->bytesPerBlock;
    if(sizeReq % Datastore::flash->bytesPerBlock) blocks++;
    sizeReq = blocks * Datastore::flash->bytesPerBlock;
    
    if((Datastore::flash->byteCount - firstFreeMem) < sizeReq) return 0; //could be made a lot smarter...
    
    //SerialUSB.println(firstFreeMem);
    
    //if we've made it this far, we can make a store
    //create the FAT entry
    Datastore newStore(fileNum, firstFreeMem, sizeReq);
    newStore.Rewind();
    
    storeList.Add(newStore);

    BufferArray storeInfo(8);
    memcpy(&storeInfo[0], &newStore.startAddress, 4);
    memcpy(&storeInfo[4], &newStore.endAddress, 4);
    Datastore::flash->WriteBytes(fileNum * 8, storeInfo);
    
    //SerialUSB.println(newStore.startAddress);
    
    //erase the relevant memory
    Datastore::flash->Erase(newStore.startAddress, sizeReq);
    
    return newStore.reservedSize;
}

uint32_t FlashStoreManager::Read(uint16_t storeNum, BufferArray& buffer)
{
    Datastore* store = storeList.Find(Datastore(storeNum));
    if(!store) return 0;
    
    if(store->currAddress > store->endAddress) return 0; //should really check if buffer is longer than remaining bytes...
    
    uint32_t bytes = Datastore::flash->ReadBytes(store->currAddress, &buffer[0], buffer.GetSize());
    store->currAddress += bytes;
    
    //SerialUSB.println(store->currAddress);
    
    return bytes;    
}
