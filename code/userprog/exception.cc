	// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"
#include "bitmap.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	is in machine.h.
//----------------------------------------------------------------------

static Bitmap *bitmap = new Bitmap(NumPhysPages);

static int threadId = 10;



void handle_Page_Fault(int page){

	IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);	
	int vpn = (unsigned) page/PageSize;
	TranslationEntry *pageTable = kernel->currentThread->space->getPageTable();	
	OpenFile *swapFile = kernel->fileSystem->Open(kernel->swapFileName);
	char *buffer = new char[PageSize];
	int avail = bitmap->FindAndSet();

	if(avail != -1){
		bzero(&kernel->machine->mainMemory[avail * PageSize], PageSize);
		swapFile->ReadAt(buffer, PageSize, pageTable[vpn].swapPage*PageSize);
		pageTable[vpn].physicalPage = avail;
		pageTable[vpn].valid = TRUE;
		kernel->physicalPageList->Append(avail);
		kernel->physicalMap[avail] = kernel->currentThread;						

		for(int i =0; i < PageSize; i++){
			kernel->machine->mainMemory[avail*PageSize + i] = buffer[i];
		}		        

	}
	else{
		int ppn = kernel->physicalPageList->RemoveFront();			
		Thread *mappedThread = kernel->physicalMap.find(ppn)->second;	
		if(!(kernel->scheduler->readyList->IsInList(mappedThread) || mappedThread == kernel->currentThread)){
			bitmap->Clear(ppn);
			return;
		}

		TranslationEntry *pageTableOther = mappedThread->space->getPageTable();	
		int a = 0;

		for(int i =0; i < mappedThread->space->numOfPages(); i++){
			if(pageTableOther[i].physicalPage == ppn){
				a = i;
				break;
			}
		}
				
		pageTable[a].physicalPage = -1;
		pageTable[a].valid = FALSE;
		int swapPage = pageTableOther[a].swapPage;

		for(int i =0; i < PageSize; i++){
			buffer[i] = kernel->machine->mainMemory[ppn*PageSize + i];
		}

		swapFile->WriteAt(buffer, PageSize, swapPage*PageSize);	
		bzero(&kernel->machine->mainMemory[ppn * PageSize], PageSize);
		swapFile->ReadAt(buffer, PageSize, pageTable[vpn].swapPage*PageSize);
		pageTable[vpn].physicalPage = ppn;
		pageTable[vpn].valid = TRUE;
		kernel->physicalPageList->Append(ppn);
		kernel->physicalMap[ppn] = kernel->currentThread;					

		for(int i =0; i < PageSize; i++){
			kernel->machine->mainMemory[ppn*PageSize + i] = buffer[i];
		}
	}		
	(void) kernel->interrupt->SetLevel(oldLevel);			
	
}

void Exit_POS(int childId){		
	IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);	
	ListIterator<Thread *> *iter = new ListIterator<Thread *>(kernel->scheduler->readyList);
	
	if(kernel->currentThread->id == childId){				
		if(kernel->currentThread->parent != NULL){
			kernel->currentThread->parent->waitList->Remove(childId);
			kernel->scheduler->ReadyToRun(kernel->currentThread->parent);
		}		
	}

	for (; !iter->IsDone(); iter->Next()) {
	    if(iter->Item()->id == childId){
	    	if(kernel->currentThread->parent != NULL){	    	
		    	iter->Item()->parent->waitList->Remove(childId);
		    	kernel->scheduler->ReadyToRun(iter->Item()->parent);
		    	break;
	    	}
	    }	    
    }

    kernel->currentThread->Finish();    
	
	(void) kernel->interrupt->SetLevel(oldLevel);

}

void ForkTest1(int id)
{
	printf("ForkTest1 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest1 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void ForkTest2(int id)
{
	printf("ForkTest2 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest2 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void ForkTest3(int id)
{
	printf("ForkTest3 is called, its PID is %d\n", id);
	for (int i = 0; i < 3; i++)
	{
		printf("ForkTest3 is in loop %d\n", i);
		for (int j = 0; j < 100; j++) 
			kernel->interrupt->OneTick();
	}
	Exit_POS(id);
}

void handle_Fork_POS(int id){

	switch (id){
		case 1:
			{
				IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
				Thread *newThread = new Thread("1");
				newThread->id = threadId;					
				newThread->Fork((VoidFunctionPtr) ForkTest1,(void *) threadId);
				kernel->machine->WriteRegister(2, (int)threadId);
				threadId++;
				(void) kernel->interrupt->SetLevel(oldLevel);
			}
		break;

		case 2:
			{
				IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
				Thread *newThread = new Thread("2");
				newThread->id = threadId;					
				newThread->Fork((VoidFunctionPtr) ForkTest2,(void *) threadId);
				kernel->machine->WriteRegister(2, (int)threadId);
				threadId++;
				(void) kernel->interrupt->SetLevel(oldLevel);
			}
		break;

		case 3:
			{
				IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);
				Thread *newThread = new Thread("3");
				newThread->id = threadId;					
				newThread->Fork((VoidFunctionPtr) ForkTest3,(void *) threadId);
				kernel->machine->WriteRegister(2, (int)threadId);
				threadId++;
				(void) kernel->interrupt->SetLevel(oldLevel);
			}
		break;
	}
	kernel->machine->PCIncreament();
}

void handle_Wait_POS(int childId){
	IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);		
	kernel->currentThread->waitList->Append(childId);
	ListIterator<Thread *> *iter = new ListIterator<Thread *>(kernel->scheduler->readyList);

	for (; !iter->IsDone(); iter->Next()) {
	    if(iter->Item()->id == childId){	    	
	    	iter->Item()->parent = kernel->currentThread;	    	
	    	break;
	    }	    
    }	
	kernel->currentThread->Sleep(false);	
	(void) kernel->interrupt->SetLevel(oldLevel);
	kernel->machine->PCIncreament();
	
}


void handle_Write(int i, int j){
	int flag = 0;  	
	for(i; i<j ; i++)
	{
		if(!kernel->machine->ReadMem(i, 1, &flag)){
			return;
		}					
		printf("%c", flag);					
	} 				
	cout << "\n";	
	kernel->machine->PCIncreament();
}

void handle_Exit(int b){
	IntStatus oldLevel = kernel->interrupt->SetLevel(IntOff);		
	cout << b << "\n";  					
	kernel->currentThread->Finish();
	(void) kernel->interrupt->SetLevel(oldLevel);
}

void
ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);  	
  	int b = kernel->machine->ReadRegister(4);
  	int i = kernel->machine->ReadRegister(4);
  	int j = kernel->machine->ReadRegister(4) + kernel->machine->ReadRegister(5);  
  	int page = kernel->machine->ReadRegister(39);	  	
  	

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which) {
		case SyscallException:
		switch(type) {
			case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

			SysHalt();

			ASSERTNOTREACHED();
			break;

			case SC_Add:
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */(int)kernel->machine->ReadRegister(4),
			/* int op2 */(int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);		
			kernel->machine->PCIncreament();

			return;

			ASSERTNOTREACHED();

			break;

			case SC_Fork_POS:	
					handle_Fork_POS(b);									
			break;

			case SC_Wait_POS:
					handle_Wait_POS(b);									
			break;

			case SC_Exit:    
				 	handle_Exit(b);			
          	break;
			
			case SC_Write:
			   		handle_Write(i,j);				   	
          	break;

			default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}
		break;

		case PageFaultException:
			{				
				handle_Page_Fault(page);				
				return;		
			}		
		break;		

		default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}	
	//ASSERTNOTREACHED();
}



