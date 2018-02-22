//
//  dataflash.cpp
//  
//
//  Created by Gregory C Lewin on 12/31/17.
//

#include <dataflash.h>

//uint16_t DataFlash::FindStores(void)
//{
//    uint16_t totalBlocks = 0;
//    for(uint32_t address = 0; address < byteCount; address += bytesPerBlock)
//    {
//        uint8_t fileNumber = ReadByte(address);
//        if(fileNumber < MAX_STORES)
//        {
//            if(!stores[fileNumber].blocks) //first record
//            {
//                stores[fileNumber].startAddress = address;
//            }
//
//            stores[fileNumber].blocks++;
//            totalBlocks++;
//
//            lastAddress = address;
//        }
//    }
//
//    return totalBlocks;
//}
//
//void DataFlash::DisplayStores(void)
//{
//    for(uint8_t i = 0; i < MAX_STORES; i++)
//    {
//        SerialUSB.print(i);
//        SerialUSB.print('\t');
//        SerialUSB.print(stores[i].startAddress);
//        SerialUSB.print('\t');
//        SerialUSB.print(stores[i].blocks);
//        SerialUSB.print('\n');
//    }
//}
//
//uint16_t DataFlash::EraseStore(uint8_t storeNumber)
//{
//    uint16_t deletedBlockCount = 0;
//    uint32_t address = stores[storeNumber].startAddress;
//
//    uint8_t blockStore = ReadByte(address);
//
//    while(blockStore == storeNumber)
//    {
//        EraseBlock4K(address);
//        address += bytesPerBlock;
//        blockStore = ReadByte(address);
//
//        deletedBlockCount++;
//    }
//
//    if(deletedBlockCount != stores[storeNumber].blocks) //do some sanity checking here...
//    {
//        SerialUSB.print(F("Incorrect delete count: "));
//        SerialUSB.println((int16_t)(deletedBlockCount - stores[storeNumber].blocks));
//    }
//
//    stores[storeNumber].startAddress = -1;
//    stores[storeNumber].blocks = 0;
//
//    return deletedBlockCount;
//}
//
//uint32_t DataFlash::CreateStore(uint8_t storeNumber)
//{
//    uint32_t address = lastAddress;
//
//    if(stores[storeNumber].blocks != 0)
//    {
//        SerialUSB.println(F("Non-zero blocks in DataFlash::CreateStore()"));
//        return 0;
//    }
//
//    if(ReadByte(address) != 0xFF)
//    {
//        SerialUSB.println(F("Address not free in DataFlash::CreateStore()"));
//        return 0;
//    }
//
//    stores[storeNumber].startAddress = address;
//    stores[storeNumber].lastAddress = address;
//
//    return address;
//}
//
//uint16_t DataFlash::AddRecord(uint8_t storeNumber, uint8_t* data, uint16_t size) //assumes size less than a page
//{
//    if(stores[storeNumber].startAddress >= byteCount)
//    {
//        SerialUSB.println(F("Invalid store address in DataFlash::AddRecord()"));
//        return 0;
//    }
//
//    if(size > bytesPerPage)
//    {
//        SerialUSB.println(F("Too much data in DataFlash::AddRecord()"));
//        return 0;
//    }
//
//    uint16_t bytesWritten = WritePage(stores[storeNumber].lastAddress, data, size);
//    stores[storeNumber].lastAddress += bytesPerPage;
//
//    return bytesWritten;
//}

TList<Datastore> Flashstore::ListStores(void)
{
    return storeList;
}

TList<Datastore> Flashstore::ReadStoresFromFlash(void)
{
    storeList.Flush();
    
    //brute force reads all the stores
    for(uint32_t address = 0; address < byteCount; address += bytesPerPage)
    {
        uint8_t storeNumber = ReadByte(address);
        
        if(storeNumber == 0xff) continue;
        
        Datastore* currStore = storeList.Find(Datastore(storeNumber));
        
        if(!currStore)
        {
            //start a new block
            currStore = storeList.Add(Datastore(storeNumber));
            currStore->startAddress = address;
        }

        currStore->nextAddress = address + bytesPerPage;
        currStore->pages++;
    }
    
    return storeList;
}

Datastore* Flashstore::CreateStore(uint8_t number)
{
    Datastore* currStore = storeList.Find(Datastore(number));
    if(currStore) return NULL; //return NULL if it already exists
    
    Datastore* last = storeList.GetTail();
    
    uint32_t startAddress = last ? last->nextAddress : 0;
    while(startAddress % bytesPerBlock) {startAddress += bytesPerPage;}
    
    return storeList.Add(Datastore(number, startAddress));
}

Datastore* Flashstore::WritePageToCurrentStore(BufferArray buffer)
{
    if(buffer.GetSize() > bytesPerPage) return NULL; //limit to one page for now
    
    Datastore* currStore = storeList.GetTail();
    if(!currStore) return NULL;
    if(currStore->nextAddress >= byteCount) currStore->nextAddress = 0; //wrap around

    buffer[0] = currStore->storeNumber; //first byte always holds storeNumber!!!

//    //printing for debug...
//    SerialUSB.print(currStore -> nextAddress);
//    for(uint16_t i = 0; i < buffer.GetSize(); i++)
//    {
//        SerialUSB.print(',');
//        SerialUSB.print(buffer[i], HEX);
//    }
//    SerialUSB.print('\n');

    FlashAT25DF641A::WritePage(currStore->nextAddress, &buffer[0], buffer.GetSize());

    currStore->nextAddress += bytesPerPage;
    
    return currStore;
}

BufferArray Flashstore::ReadPageFromStore(uint8_t storeNumber, bool rewind) //returns empty buffer when done?
{
    Datastore* store = storeList.Find(storeNumber);
    if(!store) return BufferArray(); //empty
    
    if(rewind) store->Rewind();
    
    BufferArray buffer(bytesPerPage);
    ReadData(store->currAddress, &buffer[0], buffer.GetSize());
    
//    SerialUSB.print(store->currAddress);
//    for(uint16_t i = 0; i < buffer.GetSize(); i++)
//    {
//        SerialUSB.print(',');
//        SerialUSB.print(buffer[i], HEX);
//    }
//    SerialUSB.print('\n');
    
    if(buffer[0] == storeNumber)
    {
        store->currAddress += bytesPerPage;
        return buffer; //slow to return by value...
    }
    
    else return BufferArray(); //empty buffer if numbers don't match up
}

uint16_t Flashstore::EraseStore(uint8_t storeNumber)
{
    uint16_t deletedPageCount = 0;
    Datastore* store = storeList.Find(Datastore(storeNumber));
    if(!store)
    {
        SerialUSB.println("Can't find store.");
        return 0;
    }
    
    uint32_t address = store->startAddress;
    uint8_t storeFromData = ReadByte(address);
    
    while(storeFromData == storeNumber)
    {
        WriteEnable();
        EraseBlock4K(address);
        address += bytesPerBlock;
        storeFromData = ReadByte(address);
        
        deletedPageCount += bytesPerBlock / bytesPerPage;
    }
    
    if(deletedPageCount < store->pages) //do some sanity checking here...but since we're deleting by block, they won't be equal
    {
        SerialUSB.print(F("Incorrect delete count: "));
        SerialUSB.println((int16_t)(deletedPageCount - store->pages));
    }
    
    ReadStoresFromFlash();
    //storeList.Delete(store);
    
    return deletedPageCount;
}

