/*
    Project: Virtual Memory Management
    Description: Handles the simulation of virtual memory operations, including allocation,
                 deallocation, and management of memory swaps.

    File: vm.c
    Author: Bruno Ribeiro
    Contact: Bruno Ribeiro <brlla@cesar.school>
    Organization: CESAR School

    Creation Date: 05/16/2023
    Last Updated: 06/28/2023
    License: MIT License

    Notes: This module is part of a larger system intended to demonstrate advanced
           concepts in operating system design.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PAGE_TABLE_SIZE 256 // Number of entries in the page table (0-128) -> 128 pages in the virtual logicalAddress space
#define PAGE_SIZE 256       // Page size (256 bytes or 256 instructions) -> Size of each page

#define FRAME_SIZE 256                                                 // Frame size (256 bytes) -> Size of each frame in physical memory
#define NUM_FRAMES_PHYSICAL_MEMORY 128                                 // Number of frames in physical memory
#define PHYSICAL_MEMORY_SIZE (NUM_FRAMES_PHYSICAL_MEMORY * FRAME_SIZE) // Physical memory size = 128 frames of 256 bytes each (128 * 256 = 32,768 bytes)

#define TLB_SIZE 16 // Number of entries in the TLB

// Data Structures
typedef struct TLBEntry
{
    int pageNumber;
    int frameNumber;
} TLBEntry;

typedef struct PageTableEntry
{
    int pageNumber;
    int frameNumber;
    long long accessTime;
} PageTableEntry;

// Global Variables
TLBEntry TLB[TLB_SIZE];                                             // 16 (0-15); 2 = [0] -> Page number, [1] -> Frame number
PageTableEntry pageTable[PAGE_TABLE_SIZE];                          // 128 frames (0-127) -> The size of the page table is the number of available
signed char physicalMemory[NUM_FRAMES_PHYSICAL_MEMORY][FRAME_SIZE]; // 128 frames x 256-byte frame size
signed char values[PAGE_SIZE];                                      // Array to store all the values of each pageNumber (256 bytes)
int translatedAddresses = 0;
int pageFaults = 0;
int TLBHits = 0;
int TLBCounter = 0;   // Tracks the number of entries currently used in the TLB
int nextTLBEntry = 0; // Index of the next available entry in the TLB for new page mappings
int nextFrame = 0;    // Points to the next free frame number in the physical memory  

const char *replacementStrategy;
FILE *BACKING_STORE;

// Function Declarations
void initTLB();
void initPageTable();

void getAddress(int logicalAddress);

bool searchTLB(int pageNumber, int *frameNumber);
bool searchPageTable(int pageNumber, int *frameNumber);

bool TLBFull();
int findEmptyTLBEntry();
void TLBAdd(int pageNumber, int frameNumber);

bool pageTableFull();
void pageTableAdd(int pageNumber, int frameNumber);
void updatePageTableFIFO(int pageNumber, int *frameNumber);
void updatePageTableLRU(int pageNumber, int *frameNumber);

int readBackingStore(int address);
void writeOutputFile(const char *outputFileName, int logicalAddress, int frameNumber, int pageOffset, int value, int TLBCounter);
void writeStatistics(const char *outputFileName);

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s input_file [fifo|lru]\n", argv[0]);
        return 1;
    }

    FILE *outputFile = fopen("correct.txt", "w");
    if (outputFile == NULL)
    {
        fprintf(stderr, "Error opening \"correct.txt\"\n");
        return 1;
    }
    fclose(outputFile);

    initTLB();
    initPageTable();

    BACKING_STORE = fopen("BACKING_STORE.bin", "rb");
    if (BACKING_STORE == NULL)
    {
        fprintf(stderr, "Error opening BACKING_STORE.bin\n");
        return 1;
    }

    replacementStrategy = argv[2];
    if (strcmp(replacementStrategy, "fifo") != 0 && strcmp(replacementStrategy, "lru") != 0)
    {
        fprintf(stderr, "Replacement strategy must be either 'fifo' or 'lru'\n");
        return 1;
    }

    FILE *inputFile = fopen(argv[1], "r");
    if (inputFile == NULL)
    {
        fprintf(stderr, "Error opening input file: %s\n", argv[1]);
        return 1;
    }

    int logicalAddress;
    while (fscanf(inputFile, "%d", &logicalAddress) == 1)
    {
        getAddress(logicalAddress);
    }

    fclose(inputFile);
    fclose(BACKING_STORE);

    writeStatistics("correct.txt");

    return 0; 
}

void initTLB()
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        TLB[i].pageNumber = -1;
        TLB[i].frameNumber = -1;
    }
}

void initPageTable()
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        pageTable[i].pageNumber = -1;
        pageTable[i].frameNumber = -1;
    }
}

void getAddress(int logicalAddress)
{
    int pageNumber = (logicalAddress >> 8) & 0xFF;
    int pageOffset = logicalAddress & 0xFF;

    translatedAddresses++;

    int frameNumber;
    bool TLBFound = searchTLB(pageNumber, &frameNumber);
    bool pageTableFound = searchPageTable(pageNumber, &frameNumber);

    if (TLBFound)
    {
        TLBHits++;
        // printf("TLB FOUND - Page Number: %d on TLB Entry: %d\n", pageNumber, TLBCounter);
    }
    else if (pageTableFound)
    {
        // Print page table entry found and add to TLB (even if full)
        // printf("PAGE TABLE FOUND - Page Number: %d on Frame %d\n", pageNumber, frameNumber);
        TLBAdd(pageNumber, frameNumber);
    }
    else
    {
        pageFaults++;
        // printf("PAGE FAULT - Page Number %d was not found\n", pageNumber);

        // Update page table based on replacement strategy (FIFO or LRU)
        if (strcmp(replacementStrategy, "fifo") == 0)
        {
            updatePageTableFIFO(pageNumber, &frameNumber);
            // printf("PAGE TABLE updated successfully with Page Number: %d on Frame: %d\n", pageNumber, frameNumber);
        }
        else if (strcmp(replacementStrategy, "lru") == 0)
        {
            updatePageTableLRU(pageNumber, &frameNumber);
            // printf("PAGE TABLE updated successfully with Page Number: %d on Frame: %d\n", pageNumber, frameNumber);
        }
        TLBAdd(pageNumber, frameNumber);
    }

    int backingStoreAddress = pageNumber * PAGE_SIZE + pageOffset;

    signed char value = readBackingStore(backingStoreAddress);

    writeOutputFile("correct.txt", logicalAddress, frameNumber, pageOffset, value, TLBCounter);

    // Output the results to the terminal (if desired)
    // printf("Frame Number: %d\n", frameNumber);
    // printf("Page Offset: %d\n", pageOffset);
    // printf("Logical address: %d TLB: %d Physical address: %d Value: %d\n", logicalAddress, TLBCounter, physicalAddress, value);
    // printf("\n");
}

bool searchTLB(int pageNumber, int *frameNumber)
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        if (TLB[i].pageNumber == pageNumber)
        {
            *frameNumber = TLB[i].frameNumber;

            TLBCounter = i;

            return true; 
        }
    }
    return false; 
}

bool searchPageTable(int pageNumber, int *frameNumber)
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pageTable[i].pageNumber == pageNumber)
        {
            *frameNumber = pageTable[i].frameNumber;
            pageTable[i].accessTime = translatedAddresses; 
            return true;
        }
    }
    return false;
}

bool TLBFull()
{
    for (int i = 0; i < TLB_SIZE; i++)
    {
        if (TLB[i].pageNumber == -1)
            return false;
    }
    return true;
}

int findEmptyTLBEntry()
{
    for (int i = 0; i < TLB_SIZE; ++i)
    {
        if (TLB[i].pageNumber == -1)
        {
            return i;
        }
    }
    return -1;
}

void TLBAdd(int pageNumber, int frameNumber)
{
    int TLBIndex = -1;

    if (TLBFull())
    {
        TLB[nextTLBEntry].pageNumber = pageNumber;
        TLB[nextTLBEntry].frameNumber = frameNumber;
        TLBIndex = nextTLBEntry;

        // Print a message indicating that TLB was full and updated with the new entry
        // printf("TLB was FULL and updated with Page Number: %d on TLB Entry: %d\n", pageNumber, nextTLBEntry);

        nextTLBEntry = (nextTLBEntry + 1) % TLB_SIZE;

        TLBCounter = TLBIndex;
    }
    else
    {
        for (int i = 0; i < TLB_SIZE; ++i)
        {
            if (TLB[i].pageNumber == -1)
            {
                TLB[i].pageNumber = pageNumber;
                TLB[i].frameNumber = frameNumber;
                TLBIndex = i;

                // Print a message indicating that TLB was not full and updated with the new entry
                // printf("TLB was NOT FULL and updated with Page Number: %d on TLB Entry: %d\n", pageNumber, i);

                TLBCounter = TLBIndex;
                return;
            }
        }
    }
}

bool pageTableFull()
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pageTable[i].pageNumber == -1)
        {
            return false;
        }
    }
    return true;
}

void pageTableAdd(int pageNumber, int frameNumber)
{
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        if (pageTable[i].pageNumber == -1)
        {
            pageTable[i].pageNumber = pageNumber;
            pageTable[i].frameNumber = frameNumber;
            break;
        }
    }
}

void updatePageTableFIFO(int pageNumber, int *frameNumber)
{
    if (nextFrame < NUM_FRAMES_PHYSICAL_MEMORY)
    {
        *frameNumber = nextFrame++;
    }
    else
    {
        *frameNumber = (nextFrame++) % NUM_FRAMES_PHYSICAL_MEMORY;

        for (int i = 0; i < PAGE_TABLE_SIZE; i++)
        {
            if (pageTable[i].frameNumber == *frameNumber)
            {
                pageTable[i].pageNumber = -1;
                pageTable[i].frameNumber = -1;
                pageTable[i].accessTime = -1;
                break;
            }
        }
    }

    for (int i = 0; i < PAGE_SIZE; i++)
    {
        physicalMemory[*frameNumber][i] = values[i];
    }

    pageTable[pageNumber].pageNumber = pageNumber;
    pageTable[pageNumber].frameNumber = *frameNumber;
    pageTable[pageNumber].accessTime = translatedAddresses;
}

void updatePageTableLRU(int pageNumber, int *frameNumber)
{
    if (nextFrame < NUM_FRAMES_PHYSICAL_MEMORY)
    {
        *frameNumber = nextFrame++;
    }
    else
    {
        long long oldestTime = -1;
        int lruIndex = -1;
        for (int i = 0; i < PAGE_TABLE_SIZE; i++)
        {
            if (pageTable[i].frameNumber != -1 && (oldestTime == -1 || pageTable[i].accessTime < oldestTime))
            {
                oldestTime = pageTable[i].accessTime;
                lruIndex = i;
            }
        }

        if (lruIndex == -1)
        {
            fprintf(stderr, "Error: Unable to find a frame for replacement.\n");
            exit(EXIT_FAILURE);
        }

        *frameNumber = pageTable[lruIndex].frameNumber;

        pageTable[lruIndex].pageNumber = -1;
        pageTable[lruIndex].frameNumber = -1;
        pageTable[lruIndex].accessTime = -1;
    }

    for (int i = 0; i < PAGE_SIZE; i++)
    {
        physicalMemory[*frameNumber][i] = values[i];
    }

    if (pageTable[pageNumber].frameNumber != -1)
    {
        fprintf(stderr, "Error: Inconsistent page replacement.\n");
        exit(EXIT_FAILURE);
    }

    pageTable[pageNumber].pageNumber = pageNumber;
    pageTable[pageNumber].frameNumber = *frameNumber;
    pageTable[pageNumber].accessTime = translatedAddresses;
}

int readBackingStore(int address)
{
    fseek(BACKING_STORE, address, SEEK_SET);

    signed char value;

    fread(&value, sizeof(signed char), 1, BACKING_STORE);

    return (int)value;
}

void writeOutputFile(const char *outputFileName, int logicalAddress, int frameNumber, int pageOffset, int value, int TLBCounter)
{
    FILE *outputFile = fopen(outputFileName, "a");
    if (outputFile == NULL)
    {
        fprintf(stderr, "Error opening \"%s\"\n", outputFileName);
        return;
    }

    fprintf(outputFile, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n", logicalAddress, TLBCounter, (frameNumber * FRAME_SIZE) + pageOffset, value);

    fclose(outputFile);
}

void writeStatistics(const char *outputFileName)
{
    FILE *outputFile = fopen(outputFileName, "a");
    if (outputFile == NULL)
    {
        fprintf(stderr, "Error opening \"%s\"\n", outputFileName);
        return;
    }

    double pageFaultRate = pageFaults / (float)translatedAddresses;
    double TLBRate = TLBHits / (float)translatedAddresses;

    fprintf(outputFile, "Number of Translated Addresses = %d\n", translatedAddresses);
    fprintf(outputFile, "Page Faults = %d\n", pageFaults);
    fprintf(outputFile, "Page Fault Rate = %.3f\n", pageFaultRate);
    fprintf(outputFile, "TLB Hits = %d\n", TLBHits);
    fprintf(outputFile, "TLB Hit Rate = %.3f\n", TLBRate);

    fclose(outputFile);
}
