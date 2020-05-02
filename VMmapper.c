/*
    This program scans the user part of virtual memory space and
    classifies each contigous block of pages that have the same status.

    This program takes a PID on the command line (and optional upper and
    lower bounds on the address space) and scans the memory space of the
    process with that PID.

    This program updates the console screen periodically. Double buffering
    of console image buffers is used to prevent the screeen from jumping.
*/
#include <windows.h>
#include <stdio.h>

// set the interval of time (in seconds) between memory map updates
#define UPDATE_INTERVAL 1

// define the size of the console buffers
#define COLUMNS 120
#define LINES 700

// set the beginning and ending addresses of the memory map
// these can be used to save space in the console window
// these addresses should be on 64K allocation region boundries
// the maximum value for DEFAULT_END_MEMORY_MAP_ADDRESS is 0x7fff0000
#define DEFAULT_BEGIN_MEMORY_MAP_ADDRESS 0x00000000
#define DEFAULT_END_MEMORY_MAP_ADDRESS   0x7fff0000


// function prototypes
void memoryMapper(HANDLE, void*, void*, char**);
int printRegionInfo(MEMORY_BASIC_INFORMATION, char**);
void accumulateStatistics(MEMORY_BASIC_INFORMATION, int*, int*, int*);
void printStatistics(int, int, int, char**);
void printError(char*);


/*************************************************************************************
    The main method gets a handle for the process to map by using the processes PID
    from the command line. Then main sets up the double buffering of the console image
    buffers. Then main enters an infinite loop calling the memoryMapper function, printing
    the results to one of the console buffers, switching the console buffers, displaying
    a console buffer, and then sleeping for the designated interval of time.

    For information about double buffering console image buffers, see pages 487-490
    of "Beginning Windows NT Programming", by J. Templeman, Wrox Press, 1998.
*/
int main(int argc, char *argv[])
{
   HANDLE hactive, hinactive, htemp; // handles for console image buffers
   COORD c, size;    // used for console image buffers
   SMALL_RECT rect;  // used for console image buffers
   CONSOLE_SCREEN_BUFFER_INFO info;

   // character buffer printed to the console image buffers
   char outBuf[COLUMNS*LINES];
   // character buffer used to erase the console image buffers
   char blankBuf[COLUMNS*LINES];
   char* outPtr;
   int numberWritten;

   HANDLE processHandle; // handle for process to be mapped
   int processID;        // pid of process to be mapped

   void* begin_memory_map_address = (void*)DEFAULT_BEGIN_MEMORY_MAP_ADDRESS;
   void*   end_memory_map_address = (void*)DEFAULT_END_MEMORY_MAP_ADDRESS;

   int i, j;

   if (argc < 2)
   {
      printf("usage: VMmapper <pid> [<begin address>] [<end address>]");
      MessageBox(NULL, "usage: VMmapper <pid> [<begin address>] [<end address>]", "Error", MB_OK);
      return -1;
   }
   else
   {
      processID = atoi(argv[1]);
      processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processID);
      if (NULL == processHandle)
      {
          printError("OpenProcess");
          return -1;
      }
   }

   if (argc > 2) // the beginning address should be rounded to an allocation boundary
      begin_memory_map_address = (void*)strtoul(argv[2], NULL, 16);
   if (argc > 3)
        end_memory_map_address = (void*)strtoul(argv[3], NULL, 16);

   // free the original console
   if (!FreeConsole())
      printError("FreeConsole");
   // create a new console
   if (!AllocConsole())
      printError("AllocConsole");
   // get handle to console image buffer
   hinactive = GetStdHandle(STD_OUTPUT_HANDLE);
   // create a second console image buffer for double buffering
   hactive = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, 0, CONSOLE_TEXTMODE_BUFFER, NULL);
   if (hactive == INVALID_HANDLE_VALUE)
      printError("CreateConsoleScreenBuffer");
   // set the size of the two buffers
   size.X = COLUMNS;
   size.Y = LINES;
   if (!SetConsoleScreenBufferSize(hinactive, size))
      printError("SetConsoleScreenBufferSize(hinactive)");
   if (!SetConsoleScreenBufferSize(hactive, size))
      printError("SetConsoleScreenBufferSize(hactive)");
   // get the maximum size for a console window
   size = GetLargestConsoleWindowSize(hactive);
   if ( (size.X == 0) && (size.Y == 0) )
      printError("GetLargestConsoleWindowSize");
   // set the size of each console buffer's window
   rect.Left = 0;
   rect.Top = 0;
   rect.Right = COLUMNS - 1;
   rect.Bottom = size.Y - 3;  // this seems safe
   if (!SetConsoleWindowInfo(hinactive, TRUE, &rect))
       printError("SetConsoleWindowInfo (hinactive)");
   if (!SetConsoleWindowInfo(hactive, TRUE, &rect))
       printError("SetConsoleWindowInfo (hactive)");
   // set the coord strut to be the upper left hand corner
   c.X = 0;
   c.Y = 0;
   // put lines of spaces in the blankBuf array
   for (i = 0; i < LINES; i++)
   {
      for (j = 0; j < COLUMNS-1; j++)
         blankBuf[i*COLUMNS + j] = ' ';
      blankBuf[i*COLUMNS + j] = '\n';
   }
   blankBuf[COLUMNS*LINES-1] = '\0';  // null terminate the buffer

   // set output mode to handle virtual terminal sequences
   DWORD dwMode = 0;
   if (! GetConsoleMode(hactive, &dwMode))
       printError("GetConsoleMode");
   dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
   if (! SetConsoleMode(hactive, dwMode))
       printError("SetConsoleMode");
   dwMode = 0;
   if (! GetConsoleMode(hinactive, &dwMode))
       printError("GetConsoleMode");
   dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
   if (! SetConsoleMode(hinactive, dwMode))
       printError("SetConsoleMode");

   while(1)  // repeatedly map the process's virtual memory space
   {
      outPtr = outBuf;
      memoryMapper(processHandle, begin_memory_map_address, end_memory_map_address, &outPtr);
      // write the information gathered to the inactive buffer
      if (!WriteConsole(hinactive, outBuf, outPtr-outBuf, (PDWORD)&numberWritten, NULL))
         printError("WriteConsole");
      if (!SetConsoleCursorPosition(hinactive, c))
         printError("SetConsoleCursorPosition");
      // move the scrolling information from one screen buffer to the other
      if (!GetConsoleScreenBufferInfo(hactive, &info))
         printError("GetConsoleScreenBufferInfo");
      if (!SetConsoleWindowInfo(hinactive, TRUE, &(info.srWindow)))
         printError("SetConsoleWindowInfo");
      // swap the two buffers
      htemp = hactive;
      hactive = hinactive;
      hinactive = htemp;
      // display the new active buffer
      if (!SetConsoleActiveScreenBuffer(hactive))
         printError("SetConsoleActiveScreenBuffer");
      // blank out the inactive buffer
      if (!WriteConsole(hinactive, blankBuf, LINES*COLUMNS, (PDWORD)&numberWritten, NULL))
         printError("WriteConsole (blanking)");
      if (!SetConsoleCursorPosition(hinactive, c))
         printError("SetConsoleCursorPosition");
      Sleep(UPDATE_INTERVAL*1000);
   }
   return 0;
}//main()


/********************************************************************************
     This function walks its way through virtual memory getting information
     about each distinct region of memory by using the function VirtualQueryEx.
     This function will quit if the output buffer (pointed to by outPtr) gets full.

     This function presupposes that the parameter value for begin_memory_map_address
     is at a 64KB allocation region boundary.
*/
void memoryMapper(HANDLE processHandle,
                  void* begin_memory_map_address,
                  void* end_memory_map_address,
                  char** outPtr)
{
   int lineCount = 0;
   MEMORY_BASIC_INFORMATION mbi;
   int reserved = 0, committed = 0, free = 0;
   void* regionAddress = begin_memory_map_address;

   while (regionAddress < end_memory_map_address)
   {
      // get information about the current region in virtual memory
      if (VirtualQueryEx(processHandle, regionAddress, &mbi, sizeof(mbi)) != 0)
      {
         lineCount += printRegionInfo(mbi, outPtr);  // add the info to the output buffer
         if (lineCount == LINES)
         {
            MessageBox(NULL, "Too many lines.", "Error", MB_OK);
            return;
         }
         accumulateStatistics(mbi, &reserved, &committed, &free);
         // point regionAddress at the beginning of the next region in virtual memory
         regionAddress = (char*)regionAddress + mbi.RegionSize;
      }
      else
      {
         printError("VirtualQueryEx");
         ExitProcess(-1);
      }
   }
   printStatistics(reserved, committed, free, outPtr);
}


/****************************************************************************
    This function will be called once for every distinct block of virtual memory.
    This function formats the information from a MEMORY_BASIC_INFORMATION data
    structure so that the information can be printed out. This function will
    append one line of information to the string pointed to by outPtr. If the
    information is from a new allocation region or from a free region, then a
    blank line is also inserted into the output. This function returns either
    1 or 2, depending on whether 1 or 2 lines of output are generated.
*/
int printRegionInfo(MEMORY_BASIC_INFORMATION mbi, char** outPtr)
{
   static void* oldAllocationBase; // used to determine allocation region boundries
   char* state = NULL;
   char* type = NULL;
   char* protect = NULL;
   int returnCount = 1;

   if ( (mbi.State != MEM_FREE) && (mbi.AllocationBase != oldAllocationBase) )
   {
      *outPtr += sprintf(*outPtr, "\n");  // blank line separates allocation regions
      returnCount = 2;
   }

   if (mbi.State != MEM_FREE)  // find either a reserved or a committed block of pages
   {
      oldAllocationBase = mbi.AllocationBase;  //remember the current allocation base

      // determine the type of the block
      if      (mbi.Type == MEM_PRIVATE) type = "private";
      else if (mbi.Type == MEM_MAPPED)  type = "mapped";
      else if (mbi.Type == MEM_IMAGE)   type = "image";

      // determine the state of the block of pages
      if      (mbi.State == MEM_COMMIT)  state = "commit";
      else if (mbi.State == MEM_RESERVE) state = "reserve";

      // determine the protection status of the block
      protect = "______\t";  // a default value
      if (mbi.State == MEM_COMMIT)
      {
         if      (mbi.Protect == PAGE_READONLY)     protect = "read-only";
         else if (mbi.Protect == PAGE_READWRITE)    protect = "read-write";
         else if (mbi.Protect == PAGE_WRITECOPY)    protect = "write-copy";
         else if (mbi.Protect == PAGE_EXECUTE)      protect = "execute";
         else if (mbi.Protect == PAGE_EXECUTE_READ) protect = "execute-read";
         else if (mbi.Protect == PAGE_EXECUTE_READWRITE) protect = "execute-read-write";
         else if (mbi.Protect == PAGE_EXECUTE_WRITECOPY) protect = "execute-write-copy";
         else if (mbi.Protect == (PAGE_READONLY|PAGE_GUARD))
            protect = "read-only-pageGuard";
         else if (mbi.Protect == (PAGE_READWRITE|PAGE_GUARD))
            protect = "read-write-pageGuard";
         else if (mbi.Protect == (PAGE_WRITECOPY|PAGE_GUARD))
            protect = "write-copy-pageGuard";
         else if (mbi.Protect == (PAGE_EXECUTE|PAGE_GUARD))
            protect = "execute-pageGuard";
         else if (mbi.Protect == (PAGE_EXECUTE_READ|PAGE_GUARD))
            protect = "execute-read-pageGuard";
         else if (mbi.Protect == (PAGE_EXECUTE_READWRITE|PAGE_GUARD))
            protect = "execute-read-write-pageGuard";
         else if (mbi.Protect == (PAGE_EXECUTE_WRITECOPY|PAGE_GUARD))
            protect = "execute-write-copy-pageGuard";
         else if (mbi.Protect == PAGE_NOACCESS) protect = "no-access";
      }
      else if (mbi.State == MEM_RESERVE)
      {
         if      (mbi.AllocationProtect == PAGE_READONLY)  protect = "read-only";
         else if (mbi.AllocationProtect == PAGE_READWRITE) protect = "read-write";
         else if (mbi.AllocationProtect == PAGE_WRITECOPY) protect = "write-copy";
         else if (mbi.AllocationProtect == PAGE_EXECUTE)   protect = "execute";
         else if (mbi.AllocationProtect == PAGE_EXECUTE_READ) protect = "execute-read";
         else if (mbi.AllocationProtect == PAGE_EXECUTE_READWRITE)
            protect = "execute-read-write";
         else if (mbi.AllocationProtect == PAGE_EXECUTE_WRITECOPY)
            protect = "execute-write-copy";
         else if (mbi.AllocationProtect == PAGE_NOACCESS) protect = "no-access";
      }

      // print out the info for the reserved or commited block
      *outPtr += sprintf(*outPtr, "0x%p size=%d\t%s\t%s\t%s\n",
                               mbi.BaseAddress,
                          (int)mbi.RegionSize,
                          state, protect, type);
   }
   else  // found a free region
   {
      int flag;
      returnCount = 2;
      *outPtr += sprintf(*outPtr, "\n"); // blank line separates free region
    //if ( (int)mbi.RegionSize > 1000000000 ) // greater than 1 GB
      if ( (int)mbi.RegionSize >  500000000 ) // greater than 0.5 GB
         flag = 1;
      else
         flag = 0;
      // print out the info for the free region
      *outPtr += sprintf(*outPtr, "0x%p size=%d\t%s\n",
                               mbi.BaseAddress,
                          (int)mbi.RegionSize,
                          flag ? "\x1b[31m****FREE****\x1b[m" : "free");
   }
   return returnCount;
}


/****************************************************************************
    This function is called once for every distinct region in the
    memory map. This function uses the data in the MEMORY_BASIC_INFORMATION
    block from the most recently mapped region to updates running totals for
    the amounts of reserved (but not committed), committed, and free memory.
*/
void accumulateStatistics(MEMORY_BASIC_INFORMATION mbi,
                          int* reserved, int* committed, int* free)
{
   if (mbi.State == MEM_RESERVE) *reserved += mbi.RegionSize;
   if (mbi.State == MEM_COMMIT) *committed += mbi.RegionSize;
   if (mbi.State == MEM_FREE)        *free += mbi.RegionSize;

}


/*******************************************************************************
    This function is called when a complete pass of the memory map is done.
    This function prints out the running totals kept by the accumulateStatistics
    function.
*/
void printStatistics(int reserved, int committed, int free, char** outPtr)
{
   *outPtr += sprintf(*outPtr, "\n");
   *outPtr += sprintf(*outPtr, "Total reserved: %u\n", reserved);
   *outPtr += sprintf(*outPtr, "Total committed: %u\n", committed);
   *outPtr += sprintf(*outPtr, "Total free: %u\n", free);
   *outPtr += sprintf(*outPtr, "Total memory: %u\n", reserved+committed+free);
}



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
 //  fprintf(stderr, "\n%s failed on error %d: %s", functionName, error_no, lpMsgBuf);
   MessageBox(NULL, lpMsgBuf, functionName, MB_OK);
   /* Free the buffer. */
   LocalFree( lpMsgBuf );
}
