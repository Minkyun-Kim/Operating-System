//
// Virual Memory Simulator Homework
// Two-level page table system
// Inverted page table with a hashing system 
// Student Name: 김민균
// Student Number: B411027
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int level;				// page table level (1 or 2)
	char valid;     // why valid is char?? in the book, 1 is valid, 0 is invalid
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	int frameNumber;								// valid if this entry is for the second level page table (level = 2)
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	unsigned int virtualPageNumber;			// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight; // for LRU circular doubly linked list
};

struct invertedPageTableEntry {
	int pid;					// process id
	unsigned int virtualPageNumber;		// virtual page number
	int frameNumber;			// frame number allocated
	struct invertedPageTableEntry *next;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAccess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};

struct framePage *oldestFrame; // the oldest frame pointer

int firstLevelBits,secondLevelBits, phyMemSizeBits, numProcess;

void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
    for(i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].lruLeft = &phyMem[(i-1+nFrame) % nFrame];//point this -1 frame
		phyMem[i].lruRight = &phyMem[(i+1+nFrame) % nFrame];//point this +1 frame
	}
	oldestFrame = &phyMem[0];
}

void secondLevelVMSim(struct procEntry *procTable) {
//print 2ndLv page info	
	int i;
    for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim(struct procEntry *procTable) {
//print inverted page info
	int i;
    for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAccess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAccess == procTable[i].ntraces);
	}
}

void initProcTable(struct procEntry * procTable, int numProcess, char* argv[]){
	int i, j;
    for(i = 0; i < numProcess; i++) {
		// opening a tracefile for the process
        procTable[i].traceName = argv[i+3];
        procTable[i].pid = i;
        procTable[i].ntraces = 0;
        procTable[i].num2ndLevelPageTable = 0;
        procTable[i].numIHTConflictAccess = 0;
        procTable[i].numIHTNULLAccess = 0;
        procTable[i].numIHTNonNULLAccess = 0;
        procTable[i].numPageFault = 0;
        procTable[i].numPageHit = 0;
        procTable[i].firstLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1<<firstLevelBits));
        for(j = 0; j < 1<<firstLevelBits; j++){
            procTable[i].firstLevelPageTable[j].valid = '0';
        }
        procTable[i].tracefp = fopen(argv[i+3], "r");
	}
}

//simulate muti Level page
void simul_2ndLevelPage(struct procEntry *procTable, struct framePage *phyMemFrames){
    int j;
    while(1){    
        int endOfFile = 0;
        for(j = 0; j < numProcess; j++){
            char rw;
            unsigned int addr;//change 16hex to decimal integer
            unsigned int firstBits;
            unsigned int secondBits;
            unsigned int physicalAddress = 0;
            
            if((endOfFile = fscanf(procTable[j].tracefp, "%x %c", &addr, &rw)) < 0) break;;
            (procTable[j].ntraces)++;
            firstBits = addr>>(secondLevelBits + PAGESIZEBITS);
            secondBits = (addr << firstLevelBits)>>(PAGESIZEBITS+firstLevelBits);
            //if no 2nd Level Page, make Page.
            if(procTable[j].firstLevelPageTable[firstBits].valid == '0'){
                procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable = 
                    (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry)*(1<<(VIRTUALADDRBITS-PAGESIZEBITS-firstLevelBits)));
                procTable[j].firstLevelPageTable[firstBits].valid = '1';
                procTable[j].num2ndLevelPageTable++;
                procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].level = 2;
                procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].valid = '0';
                procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber = -1;
            }
            //if VA is valid
            if(procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].valid == '1'){
                unsigned int frameNum = procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber;
                physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
                procTable[j].numPageHit++;
                if(oldestFrame == &phyMemFrames[frameNum])
                    oldestFrame = oldestFrame->lruRight;
                else if (oldestFrame->lruLeft == &phyMemFrames[frameNum]){}
                else{
                    (*(phyMemFrames[frameNum].lruLeft)).lruRight = phyMemFrames[frameNum].lruRight;
                    (*(phyMemFrames[frameNum].lruRight)).lruLeft = phyMemFrames[frameNum].lruLeft;
                    phyMemFrames[frameNum].lruRight = oldestFrame;
                    phyMemFrames[frameNum].lruLeft = (*oldestFrame).lruLeft;
                    (*(*oldestFrame).lruLeft).lruRight = &phyMemFrames[frameNum];
                    (*oldestFrame).lruLeft = &phyMemFrames[frameNum];
                }
            }
            //if VA is not valid
            else{
                procTable[j].numPageFault++;
                //if frame is free
                if(oldestFrame->pid == -1 ){
                    procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].valid = '1';
                    procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber = (*oldestFrame).number;
                    (*oldestFrame).virtualPageNumber = (addr>>PAGESIZEBITS);
                    (*oldestFrame).pid = procTable[j].pid;
                    oldestFrame = (*oldestFrame).lruRight;
                    unsigned int frameNum = procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber;
                    physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
                }
                //if frame is already allocated
                else{
                    //free already allocated VA
                    int oldPid = (*oldestFrame).pid;
                    unsigned int FLB = (*oldestFrame).virtualPageNumber >> secondLevelBits;
                    unsigned int SLB = ((*oldestFrame).virtualPageNumber<<(firstLevelBits + PAGESIZEBITS))>>(PAGESIZEBITS+firstLevelBits);
                    procTable[oldPid].firstLevelPageTable[FLB].secondLevelPageTable[SLB].valid = '0';
                    procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].valid = '1';
                    procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber = (*oldestFrame).number;
                    (*oldestFrame).virtualPageNumber = (addr>>PAGESIZEBITS);
                    (*oldestFrame).pid = procTable[j].pid;
                    unsigned int frameNum = procTable[j].firstLevelPageTable[firstBits].secondLevelPageTable[secondBits].frameNumber;
                    oldestFrame = oldestFrame->lruRight;
                    physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
                }
            }
            printf("2Level procID %d traceNumber %d virtual addr %x pysical addr %x\n",j, procTable[j].ntraces, addr, physicalAddress);
        }
        if(endOfFile == EOF)
            break;
    }
}


//simulate inverted page
void simul_invertedPage(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame){
    int  j;
    struct invertedPageTableEntry* IHPT = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry)*(nFrame));

    while(1){
        int endOfFile = 0;
        for(j = 0; j < numProcess; j++){
            char  rw;
            unsigned int addr, VPN;//change 16hex to decimal integer
            unsigned int index;
            unsigned int physicalAddress=0;
            unsigned int frameNum;
            struct invertedPageTableEntry* ptr_IPTE;

            if((endOfFile = fscanf(procTable[j].tracefp, "%x %c", &addr, &rw)) < 0) break;
            (procTable[j].ntraces)++;
            VPN = addr>>PAGESIZEBITS;
            index = (VPN + procTable[j].pid) % nFrame;
            ptr_IPTE = IHPT[index].next;

            //find whether finding page is valid
            while(ptr_IPTE != NULL){
                (procTable[j].numIHTConflictAccess)++;
                if((*ptr_IPTE).pid == procTable[j].pid && (*ptr_IPTE).virtualPageNumber == VPN){
                    break;
                }
                else
                    ptr_IPTE =(*ptr_IPTE).next;
            }
            //if page found(valid)
            if(ptr_IPTE != NULL){
                (procTable[j].numPageHit)++;
                (procTable[j].numIHTNonNULLAccess)++;
                //update lru
                frameNum = (*ptr_IPTE).frameNumber;
                if(oldestFrame == &phyMemFrames[frameNum])
                    oldestFrame = oldestFrame->lruRight;
                else if (oldestFrame->lruLeft == &phyMemFrames[frameNum]){

                }
                else{
                    phyMemFrames[frameNum].lruLeft->lruRight = phyMemFrames[frameNum].lruRight;
                    phyMemFrames[frameNum].lruRight->lruLeft = phyMemFrames[frameNum].lruLeft;
                    phyMemFrames[frameNum].lruRight = oldestFrame;
                    phyMemFrames[frameNum].lruLeft = (*oldestFrame).lruLeft;
                    oldestFrame->lruLeft->lruRight = &phyMemFrames[frameNum];
                    oldestFrame->lruLeft = &phyMemFrames[frameNum];
                }
                physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
            }
            //if page not found(invalid)
            //empty table or not on memory
            else{
                (procTable[j].numPageFault)++;
                //lmake node at empty hash table
                if(IHPT[index].next == NULL){
                    procTable[j].numIHTNULLAccess++;
                    IHPT[index].next = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
                    IHPT[index].next->pid = procTable[j].pid;
                    IHPT[index].next->virtualPageNumber = VPN;
                    IHPT[index].next->frameNumber = oldestFrame->number;
                    IHPT[index].next ->next = NULL;// prevent infinite loop
                    frameNum = IHPT[index].next->frameNumber;
                    physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
                }
                //link node in not empty hash table
                else{
                    procTable[j].numIHTNonNULLAccess++;
                    ptr_IPTE = IHPT[index].next;
                    IHPT[index].next = (struct invertedPageTableEntry*)malloc(sizeof(struct invertedPageTableEntry));
                    IHPT[index].next->pid = procTable[j].pid;
                    IHPT[index].next->virtualPageNumber = VPN;
                    IHPT[index].next->frameNumber = oldestFrame->number;
                    IHPT[index].next->next = ptr_IPTE;
                    unsigned int frameNum = IHPT[index].next->frameNumber;
                    physicalAddress = (frameNum << PAGESIZEBITS) | (addr & 4095);
                }
                //if not allocated frame, allocate frame
                if(oldestFrame->pid == -1){
                    oldestFrame->pid = procTable[j].pid;
                    oldestFrame->virtualPageNumber = VPN;
                    oldestFrame = oldestFrame->lruRight;
                }
                //allocated frame, free and allocate
                else{
                    //free frame
                    unsigned int oldVPN = oldestFrame->virtualPageNumber;
                    int oldPid = oldestFrame->pid;
                    oldestFrame->pid = procTable[j].pid;
                    oldestFrame->virtualPageNumber = VPN;
                    oldestFrame = oldestFrame->lruRight;
                    unsigned int back_index = (oldVPN + oldPid) % nFrame;
                    struct invertedPageTableEntry* followPtr;
                    ptr_IPTE = IHPT[back_index].next;
                    followPtr = NULL;
                    int k = 1;
                    while(ptr_IPTE != NULL){
                        if(oldVPN == ptr_IPTE->virtualPageNumber && oldPid == ptr_IPTE->pid){    
                            break;
                        }
                        followPtr = ptr_IPTE;
                        ptr_IPTE = ptr_IPTE->next;
                        k++;
                    }
                    //delete
                    if(followPtr == NULL){
                        //only one node
                        if(ptr_IPTE->next == NULL){
                            IHPT[back_index].next = NULL;
                            free(ptr_IPTE);
                        }
                        //more than one node
                        else{
                            IHPT[back_index].next = ptr_IPTE->next;
                            free(ptr_IPTE);
                        }
                    }
                    //second node
                    else{
                        //last node
                        if(ptr_IPTE->next == NULL){
                            followPtr->next = NULL;
                            free(ptr_IPTE);
                        }
                        //middle node
                        else{
                            followPtr->next = ptr_IPTE->next;
                            free(ptr_IPTE);
                        }
                    }

                }
            }       
            printf("IHT procID %d traceNumber %d virtual addr %x pysical addr %x\n", j, procTable[j].ntraces, addr, physicalAddress);
        }
        if(endOfFile == EOF)
            break;
    }
}

int main(int argc, char *argv[]) {
    // ERROE if the options are incorrectly put
	int i;
    if (argc < 4){
	     printf("Usage : %s firstLevelBits PhysicalMemorySizeBits TraceFileNames\n",argv[0]); exit(1);
	}
    firstLevelBits = atoi(argv[1]);
    phyMemSizeBits = atoi(argv[2]);
    secondLevelBits = VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits;
    numProcess = argc -3;
    //ERROR if the Mem size is smaller then Page size
	if (phyMemSizeBits < PAGESIZEBITS) { 
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n",phyMemSizeBits,PAGESIZEBITS); exit(1);
	}
    //ERROR 32bit - offset - firstLevelBits = secondLevelBits. secondBits must be bigger then 0
	if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0 ) {    
		printf("firstLevelBits %d is too Big\n",firstLevelBits); exit(1);
	}
    for(i = 0; i < numProcess;i++){
		printf("process %d opening %s\n",i,argv[i+3]);
    }
    //make process Table
    struct procEntry *procTable = (struct procEntry*)malloc(sizeof(struct procEntry) * (argc - 3));
    //calculate the number of Frame
	int nFrame = (1<<(phyMemSizeBits-PAGESIZEBITS)); assert(nFrame>0);
    struct framePage *phyMemFrames = (struct framePage*)malloc(sizeof(struct framePage)*nFrame);
	printf("\nNum of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
	

/////////////////////////////////////////////////////////    
/*       t w o   l e v e l  p a g e   t a b l e        */   
/////////////////////////////////////////////////////////
    //initialize procTable for two-level page table
	initProcTable(procTable, numProcess, argv);
    initPhyMem(phyMemFrames, nFrame);//initialize physical memory
    //2nd level page 
    printf("=============================================================\n");
	printf("The 2nd Level Page Table Memory Simulation Starts .....\n");
    printf("=============================================================\n");
    simul_2ndLevelPage(procTable, phyMemFrames);
    secondLevelVMSim(procTable);//printf  2nd level page
    

/////////////////////////////////////////////////////////    
/*       i n v e r d t e d   p a g e   t a b l e       */    
/////////////////////////////////////////////////////////
    // initialize procTable for the inverted Page Table
    for(i = 0; i < numProcess; i++) {
		//rewind tracefiles
        //go back to start point of file. Cause we already read file at 2nd level page
		rewind(procTable[i].tracefp);
	}
    // initialize procTable for inverted page table
	initProcTable(procTable, numProcess, argv);
    initPhyMem(phyMemFrames, nFrame);//initialize physical memory
    //inverted page
	printf("=============================================================\n");
	printf("The Inverted Page Table Memory Simulation Starts .....\n");
	printf("=============================================================\n");
    simul_invertedPage(procTable, phyMemFrames, nFrame);
    invertedPageVMSim(procTable);//printf inverted page
	
    free(procTable);   
    free(phyMemFrames);
    return(0);
}