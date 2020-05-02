/*
Eknechukwu Nwannunu
Ricky Schrombeck


*/
/*
     This program accepts commands that cause it to perform virtual memory
     operations. The commands are read from standard input, but it is better
     to put the commands in a "script file" and use the operating system's
     command line to redirect the script file to this program's standard input
     (as in "C:\VMdriver < VMcmds.txt").

     The commands that this program accepts are of the form

     time, vmOp, vmAddress, units, access

     The five parameters have the following meaning:

     time - Seconds to wait after reading the command before performing the VM operation.
     vmOp - Code that represents the VM operation to perform.
     vmAddress - virtual memory address (in hex) where the VM operation is to be performed
     units - The number of units to use in the VM operation.
             For reserving memory, each unit represents 65536 bytes of memory.
             For committing memory, each unit represents 4096 bytes of memory.
     access - Code that represents the access protection.

     The vmOp codes and their meanings are:
     1 - Reserve a region of virtual memory.
     2 - Commit a block of pages.
     3 - Touch pages in a block.
     4 - Lock a block of pages.
     5 - Unlock a block of pages.
     6 - Create a guard page.
     7 - Decommit a block of pages.
     8 - Release a region.

     The access codes and their meaning are:
     1 - PAGE_READONLY
     2 - PAGE_READWRITE
     3 - PAGE_EXECUTE
     4 - PAGE_EXECUTE_READ
     5 - PAGE_EXECUTE_READWRITE
     6 - PAGE_NOACCESS

     Most of the commands are described in the file
        "Virtual Memory from 'Beginning Windows NT Programming' by Julian Templeman.pdf".
     The only command not mentioned there is the "Touch pages in a block" command. 


     This program should create a process that runs the program VMmapper.exe so that
     you can observe the memory operations as they happen. The program VMmapper takes
     a PID on its command line and then it repeatedly maps and displays (once a second)
     the virtual memory space of the process with that PID. This program should pass
     its own PID on the command line to the VMmapper program.

     When this program has completed all of its operations, it goes into an infinite
     loop.
*/
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define RESERVE_MEM  65536
#define COMMITT_MEM  4096

// prototype for the function, defined below, that prints err messages
void printError(char* functionName);

int main( )
{
   int time, vmOp, units, access, x;
   LPVOID vmAddress;
   LPVOID lpvResult;
   DWORD pid = 0; 
   char lpCommandLine[256];
   PROCESS_INFORMATION processInfo;
   STARTUPINFO startInfo;
   ZeroMemory(&startInfo, sizeof(startInfo));
   startInfo.cb = sizeof(startInfo);     
   SYSTEM_INFO sSysInfo;
   DWORD dwPageSize = 0;
   DWORD accessLevel;
   GetSystemInfo(&sSysInfo);
   
   // Starts up the VMmapper.exe program with the PID of this program
   // on the command line.
   
   pid = GetCurrentProcessId(); 


   sprintf(lpCommandLine, "VMmapper.exe %d", pid);
   if(! CreateProcess(NULL,
                             lpCommandLine,
                             NULL,
                             NULL,
                             FALSE,
                             CREATE_NEW_CONSOLE,
                             NULL,
                             NULL,
                             &startInfo,
                             &processInfo))
       {
          printError("CreateProcess");
       }
       else
       {
         ;
       }


   Sleep(5000);  // give VMmapper.exe time to start


   // Process loop.
   printf("next VM command: ");
   while(scanf("%d%d%p%d%d", &time, &vmOp, &vmAddress, &units, &access) == 5)
   {
      // Wait until it is time to execute the command.
      Sleep(time*1000);
      
      
      dwPageSize = sSysInfo.dwPageSize;
      
      // Parse the command and execute it.
      switch(access)
      {
         case 1:
         accessLevel = PAGE_READONLY;
         break;
         case 2:
         accessLevel = PAGE_READWRITE;
         break;
         case 3:
         accessLevel = PAGE_EXECUTE;
         break;
         case 4:
         accessLevel = PAGE_EXECUTE_READ;
         break;
         case 5:
         accessLevel = PAGE_EXECUTE_READWRITE;
         break;
         case 6:
         accessLevel = PAGE_NOACCESS;
         break;
         default:
         printf("Access Level input is invalid\n");
         break;
      }
      
      switch (vmOp)
      {
         case 1:  // Reserve a region.
         
            
          lpvResult = VirtualAlloc(
                     (LPVOID)vmAddress, // Systems selects address
                     (units * RESERVE_MEM),         // Page size, in bytes
                     MEM_RESERVE,         // Allocate reserved pages
                     accessLevel);
          if(lpvResult == NULL )
            {
               printf("VirtualAlloc reserve failed.\n");
            }
           break;
         case 2:  // Commit a block of pages.
         
            
          lpvResult = VirtualAlloc(
                     (LPVOID)vmAddress, // Systems selects address
                     (units * COMMITT_MEM),         // Page size, in bytes
                     MEM_COMMIT,         // Allocate committied pages
                     accessLevel);
          if(lpvResult == NULL )
            {
               printf("VirtualAlloc commit failed.\n");
            }
           break;   
           
         case 3:  // "Touch" pages in a block.

            for(int i = 0; i < units; i++)
            {
               int* add = (int*)(vmAddress + (COMMITT_MEM * i));
               
               x = *add;
               printf("Touching 0x%p \n", add);
            }
            break;
         case 4:  // Lock a block of pages.
            
            lpvResult = (int*)VirtualLock(
                                    (LPVOID)vmAddress,
                                    units * COMMITT_MEM);
            if(lpvResult == 0)
            {
               printf("VirtualLock failed.\n");
            }
            break;
         case 5:  // Unlock a block of pages.

            lpvResult = (int*)VirtualUnlock(
                                    (LPVOID)vmAddress,
                                    units * COMMITT_MEM);
            if(lpvResult == 0)
            {
               printf("VirtualUnlock failed.\n");
            }
            break;
         case 6:  // Create a guard page.
            if(access == 6)
            {
               printf("Guard Page not allowed with NO_ACCESS flag \n");
               break;
            }
            
             lpvResult = VirtualAlloc(
                        (LPVOID)vmAddress, // Systems selects address
                        (units * RESERVE_MEM),         // Page size, in bytes
                        MEM_RESERVE | MEM_COMMIT,         // Allocate reserved pags
                        accessLevel | PAGE_GUARD);
              if(lpvResult == NULL)
              {
                 printf("VirtualAlloc Page Guard failed.\n");
              }            
               break;
         case 7:  // Decommit a block of pages.
           
            lpvResult = (int*)VirtualFree(
                                   (LPVOID)vmAddress,
                                    (units * COMMITT_MEM),
                                    MEM_DECOMMIT);
            if(lpvResult == 0)
            {
               printf("VirtualFree Decommit failed. \n");
            }
            break;
         case 8:  // Release a region.
           
            lpvResult = (int*)VirtualFree(
                                   (LPVOID)vmAddress,
                                    0,
                                    MEM_RELEASE);
            if(lpvResult == 0)
            {
               printf("VirtualFree Release failed. \n");
            }            
            break;
      }//switch
      printf("Processed %d %d 0x%p %d %d\n", time, vmOp, vmAddress, units, access);
      printf("next VM command: ");
   }//while

   while (1) Sleep(1000); // spin until killed

   return 0;

}//main()



/*******************************************************************
   This function prints out "meaningful" error messages. If you call
   a Windows function and it returns with an error condition, then
   call this function right away and pass it a string containing the
   name of the Windows function that failed. This function will print
   out a reasonable text message explaining the error.
*/
void printError(char* functionName)
{
   LPSTR lpMsgBuf = NULL;
   int error_no;
   error_no = GetLastError();
   FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,
      error_no,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
      (LPTSTR)&lpMsgBuf,
      0,
      NULL
   );
   /* Display the string. */
   fprintf(stderr, "\n%s failed on error %d: %s", functionName, error_no, lpMsgBuf);
   //MessageBox(NULL, lpMsgBuf, functionName, MB_OK);
   /* Free the buffer. */
   LocalFree( lpMsgBuf );
}




