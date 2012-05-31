/*
    Homework.h
*/

typedef unsigned char byte;

//
// ------------------------------------------------------------ Program Options
//

typedef struct _OPTIONS {
    int ThreadCount;
    char const* KeyFileName;
    int BlockSize;
} OPTIONS;

int
ParseCommandLine(
    int argc,
    char* argv[],
    OPTIONS * Options
    );

//
// Some options are currently compile time constants.
//

#define DEFAULT_BLOCKSIZE 128

//
// ------------------------------------------------------------------- File I/O
//

typedef struct _IO_SYNCHRONIZATION_BLOCK {

    FILE * InputStream;
    pthread_mutex_t ReadLock;
    size_t ReadOffset;
    
    FILE * OutputStream;
    pthread_mutex_t WriteLock;
    pthread_cond_t WriteEvent;
    size_t WriteOffset;
    
} IO_SYNCHRONIZATION_BLOCK;


int
ReadBlock(
    IO_SYNCHRONIZATION_BLOCK * IoSyncBlock,
    byte * Buffer,
    size_t BufferLength,
    size_t * Offset,
    size_t * BytesRead
    );

int
WriteBlock(
    IO_SYNCHRONIZATION_BLOCK * IoSyncBlock,
    byte * Buffer,
    size_t BufferLength,
    size_t Offset
    );
    
//
// ----------------------------------------------------------------- Encryption 
//

int
ReadKey(
    char const * FileName,
    byte ** Key,
    size_t * KeyLength
    );

int
IterateKey(
    byte * Key,
    size_t KeyLength,
    int Iteration
    );

//
// -------------------------------------------------------------- Worker Thread
//

typedef struct _WORKER_CONTEXT {
    IO_SYNCHRONIZATION_BLOCK * IoSyncBlock;
    OPTIONS const * Options;
    byte const * Key;
    size_t KeyLength;
} WORKER_CONTEXT;

void *
WorkerThreadRoutine(
    void * Context
    );
    
void
ErrorExit(
    int ErrorNumber,
    char const * Format,
    ...
    );


