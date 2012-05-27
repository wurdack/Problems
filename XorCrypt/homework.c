#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "homework.h"

QUEUE_HEAD FreeDescriptorQueue;
QUEUE_HEAD FlushDescriptorQueue;

main(int argc, char* argv[])

/*

Description:

    This is the main routine for the process. It parses command line arguments,
    then creates a set of threads to perform the work of encrypting the the
    stream.

Arguments:

    argc - Supplies the count of arguments.

    argv - Supplies an array of command line arguments. Valid arguments for this
        routine are:

        -k <filename>   Specifies the key file
        -n <count>      Specifies the number of threads to create.

    N.B.: Additionally, this routine reads input from stdin, and writes its 
          output to stdout.

Return Value:

    None.

*/

{

    DESCRIPTOR* Descriptor;
    size_t DescriptorSize;
    int Index;
    OPTIONS Options;
    int Result;
    
    pthread_t WorkerThread;

    if (ParseCommandLine(argc, argv, &Options) == 0) {

        //
        // Allocate descriptors and initialize the free descriptor queue.
        //

        InitializeQueue(&FreeDescriptorQueue);
        InitializeQueue(&FlushDescriptorQueue);

        fprintf(stderr, " FreeQueue: %p\n", &FreeDescriptorQueue);
        fprintf(stderr, "FlushQueue: %p\n", &FlushDescriptorQueue);
       
        DescriptorSize = sizeof(DESCRIPTOR) + Options.BlockSize;
        
        //                       V-- rwurdack_todo
        for (Index = 0; Index < 128; ++Index) {
            Descriptor = calloc(DescriptorSize, sizeof(char));
            if (Descriptor == NULL) {
                fprintf(stderr, "Descriptor allocation failure\n");
                exit(0); // rwurdack_todo -- yes this leaks.
            }
            
            Descriptor->Buffer = (char *)Descriptor + sizeof(DESCRIPTOR);
            Enqueue(&FreeDescriptorQueue, &Descriptor->Linkage);
        }
        
        //
        // Create worker thread
        //
        pthread_create(&WorkerThread, NULL, EncryptionWorker, NULL);
        
        //
        // Read data into descriptor ring.
        //
        for (;;) {
            Result = ReadBlock(stdin, Options.BlockSize, &Descriptor);
            if (Result == 1) {
                fprintf(stderr, "Done reading.\n");
                ShutdownQueue(&FlushDescriptorQueue);
                break;
            } else if (Result == 2) {
                fprintf(stderr, "Unexpected read error.\n");
                exit (1);
            }
            
            Enqueue(&FlushDescriptorQueue, &Descriptor->Linkage);
        }
    }
                
    //
    // Wait for the worker to finish.
    //
    pthread_join(WorkerThread, NULL);
    
    //
    // Clean up descriptor allocations (rwurdack_todo)
    //
                        
    exit(0);
}

//
// ------------------------------------------------------------------- File I/O
//

int
ReadBlock(
    FILE * Stream,
    size_t BlockLength,
    DESCRIPTOR ** Descriptor
    )
{
    size_t BytesRead;
    DESCRIPTOR * NewDescriptor;

    for (;;) {
        NewDescriptor = (DESCRIPTOR *)Dequeue(&FreeDescriptorQueue);
        if (NewDescriptor != NULL) {
            break;
        }
    }

    BytesRead = fread(NewDescriptor->Buffer, 1, BlockLength, Stream);
    NewDescriptor->Length = BytesRead;
    if (BytesRead == 0) {
        if (feof(Stream)) {
            return 1; // rwurdack_todo - eof
        } else {
            return 2; // rwurdack_todo - fatal
        }
    } else {
        *Descriptor = NewDescriptor;
        return 0; // rwurdack_todo - ok
    }
}

int
WriteDescriptor(
    DESCRIPTOR* Descriptor
    )
{
    size_t BytesWritten;
    
    // rwurdack_todo -- parameterize stream
    BytesWritten = fwrite(Descriptor->Buffer, 1, Descriptor->Length, stdout);
    
    return !(BytesWritten == Descriptor->Length);
}

//
// ------------------------------------------------------------ Program Options
//

int
ParseCommandLine(
    int argc,
    char* argv[],
    OPTIONS * Options
    )
// return != 0 on failure
{

    int Index;
    int ThreadCount;
    char const* KeyFileName;

    ThreadCount = 0;
    KeyFileName = NULL;

    //
    // Parse the command line arguments.
    //

    for (Index = 1; Index < argc; ++Index) {
        if (strcmp(argv[Index], "-k") == 0) {
            ++Index;
            if (Index >= argc) {
                fprintf(stderr, "Missing key file name after -k\n");
                return 1;
            }
            
            KeyFileName = argv[Index];
            continue;
        }

        if (strcmp(argv[Index], "-n") == 0) {
            ++Index;
            if (Index >= argc) {
                fprintf(stderr, "Missing thread count after -n\n");
                return 1;
            }
            
            ThreadCount = atoi(argv[Index]);
            continue;
        }

        fprintf(stderr, "Invalid option: %s\n", argv[Index]);
        return 1;
    }        
    
    if (ThreadCount == 0) {
        fprintf(stderr, "Thread count was unspecified\n");
        return 1;
    }

    if (KeyFileName == NULL) {
        fprintf(stderr, "Key filename was unspecified\n");
        return 1;
    }

    Options->ThreadCount = ThreadCount;
    Options->KeyFileName = KeyFileName;
    Options->BlockSize = DEFAULT_BLOCKSIZE;    

    return 0;
}

//
// ----------------------------------------------------------------- Encryption 
//

void
EncryptDescriptor(
    DESCRIPTOR * Descriptor,
    char * Key,
    size_t KeyLength
    )
{
    //
    // for now, do nothing.
    //
}

void *EncryptionWorker(
    void * Context
    )
{
    DESCRIPTOR* Descriptor;
    WORKER_CONTEXT* WorkerContext;
    
    WorkerContext = (WORKER_CONTEXT*)Context;
    
    for (;;) {
        Descriptor = (DESCRIPTOR*)Dequeue(&FlushDescriptorQueue);
        if (Descriptor == NULL) {
            break;
        }
        
        // rwurdack_todo -- this has no accomodation for ordering.
        WriteDescriptor(Descriptor);
        Enqueue(&FreeDescriptorQueue, &Descriptor->Linkage);
    }
}

//
// ----------------------------------------------------------- Queue Management
//

void
InitializeQueue(
    QUEUE_HEAD * QueueHead
    )
{
    QueueHead->Sentinel.FLink = &QueueHead->Sentinel;
    QueueHead->Sentinel.BLink = &QueueHead->Sentinel;
    pthread_mutex_init(&QueueHead->Mutex, NULL);
    pthread_cond_init(&QueueHead->NotEmpty, NULL);
    QueueHead->Running = 1;
}

void
Enqueue(
    QUEUE_HEAD * QueueHead,
    QUEUE_ENTRY * Entry
    )
{
    QUEUE_ENTRY * FLink;
    QUEUE_ENTRY * BLink;

    assert(QueueHead->Running != 0);

    pthread_mutex_lock(&QueueHead->Mutex);
    Entry->FLink = QueueHead->Sentinel.FLink;
    Entry->BLink = &QueueHead->Sentinel;
    Entry->FLink->BLink = Entry;
    QueueHead->Sentinel.FLink = Entry;
    pthread_cond_signal(&QueueHead->NotEmpty);
    pthread_mutex_unlock(&QueueHead->Mutex);
}

QUEUE_ENTRY *
Dequeue(
    QUEUE_HEAD * QueueHead
    )
{
    QUEUE_ENTRY * Entry;

    Entry = NULL;
    pthread_mutex_lock(&QueueHead->Mutex);

    //
    // Block if the queue is empty and still running.
    //

    if ((QueueHead->Sentinel.BLink == &QueueHead->Sentinel) &&
        (QueueHead->Running == 1))
    {
/*
        fprintf(stderr, 
                "Waiting for Queue %p to fire\n"
                "    Running: %d\n"
                "  Sentinels: %x %x\n",
                QueueHead,
                QueueHead->Running,
                QueueHead->Sentinel.FLink,
                QueueHead->Sentinel.BLink);
*/
        pthread_cond_wait(&QueueHead->NotEmpty, &QueueHead->Mutex);
    }

    //
    // If there is something in the queue, return it.
    //
    
    if (QueueHead->Sentinel.BLink != &QueueHead->Sentinel)
    {
        //
        // Remove the first queue element.
        //

        Entry = QueueHead->Sentinel.BLink;
        QueueHead->Sentinel.BLink = Entry->BLink;
        Entry->BLink->FLink = &QueueHead->Sentinel;

        //
        // Clean up stale pointers.
        //

        Entry->BLink = NULL;
        Entry->FLink = NULL;
    }
    
    pthread_mutex_unlock(&QueueHead->Mutex);
    return Entry;
}

void
ShutdownQueue(
    QUEUE_HEAD * QueueHead
    )
{
    pthread_mutex_lock(&QueueHead->Mutex);

//    fprintf(stderr, "ShutdownQueue %x\n", QueueHead);
    
    assert(QueueHead->Running != 0);
    
    // rwurdack_todo -- now we need to rename this member...
    // rwurdack_todo -- needs to broadcast I think...
    QueueHead->Running = 0;
    pthread_cond_signal(&QueueHead->NotEmpty);
    pthread_mutex_unlock(&QueueHead->Mutex);
}

void
PrintQueue(
    QUEUE_HEAD * QueueHead
    )
{
    QUEUE_ENTRY * Entry;

    printf("Head %p\n", QueueHead);
    for (Entry = QueueHead->Sentinel.FLink; 
         Entry != &QueueHead->Sentinel; 
         Entry = Entry->FLink) {

        printf("%p %p %p\n", Entry, Entry->FLink, Entry->BLink);
    }
}

