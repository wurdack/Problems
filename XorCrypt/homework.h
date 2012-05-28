/*
    Homework.h
*/

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

#define DEFAULT_BLOCKSIZE 65536

//
// ------------------------------------------------------------------- File I/O
//

int
ReadBlock(
    FILE * Stream,
    char * Buffer,
    size_t BufferLength,
    size_t * Offset,
    size_t * BytesRead
    );

int
WriteBlock(
    FILE * Stream,
    char * Buffer,
    size_t BufferLength,
    size_t Offset
    );
    
//
// ----------------------------------------------------------------- Encryption 
//

int
ReadKey(
    char const * FileName,
    char ** Key,
    size_t * KeyLength
    );

//
// -------------------------------------------------------------- Worker Thread
//

typedef struct _WORKER_CONTEXT {
    FILE * InputStream;
    FILE * OutputStream;
    OPTIONS const * Options;
} WORKER_CONTEXT;

void *
WorkerThreadRoutine(
    void * Context
    );

