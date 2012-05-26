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

#define DEFAULT_BLOCKSIZE 4096

//
// ----------------------------------------------------------- Queue Management
//

typedef struct _QUEUE_ENTRY {
    struct _QUEUE_ENTRY * FLink;
    struct _QUEUE_ENTRY * BLink;
} QUEUE_ENTRY;

typedef struct _QUEUE_HEAD {
    QUEUE_ENTRY Sentinel;
    pthread_mutex_t Mutex;
} QUEUE_HEAD;

void
InitializeQueue(
    QUEUE_HEAD * QueueHead
    );

void
Enqueue(
    QUEUE_HEAD * QueueHead,
    QUEUE_ENTRY * Entry
    );

QUEUE_ENTRY *
Dequeue(
    QUEUE_HEAD * QueueHead
    );

void
PrintQueue(
    QUEUE_HEAD * QueueHead
    );

//
// ------------------------------------------------------------------- File I/O
//

typedef struct _DESCRIPTOR {
    struct _QUEUE_ENTRY Linkage;
    long StreamOffset;
    size_t Length;
    char * Buffer;
} DESCRIPTOR;

