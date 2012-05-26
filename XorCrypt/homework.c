#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

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

    if (ParseCommandLine(argc, argv, &Options) == 0) {

        //
        // Do useful work here.
        //

        fprintf(stderr, "Thread Count: %d\n", Options.ThreadCount);
        fprintf(stderr, "    Key File: %s\n", Options.KeyFileName);

        //
        // Allocate descriptors and initialize the free descriptor queue.
        //

        InitializeQueue(&FreeDescriptorQueue);
        InitializeQueue(&FlushDescriptorQueue);
        
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
        // Read data into descriptor ring.
        //
        for (;;) {
            Result = ReadBlock(stdin, Options.BlockSize, &Descriptor);
            if (Result == 1) {
                fprintf(stderr, "Done reading.\n");
                exit (0);
            } else if (Result == 2) {
                fprintf(stderr, "Unexpected read error.\n");
                exit (1);
            }
            
            Enqueue(&FlushDescriptorQueue, &Descriptor->Linkage);
        }
    }
                
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

    NewDescriptor = (DESCRIPTOR *)Dequeue(&FreeDescriptorQueue);
    if (NewDescriptor == NULL) {
        // rwurdack_todo -- should block until one is freed or allocate on 
        //                  demand.
        fprintf(stderr, "Unexpected allocation failure\n");
        exit (1);
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
}

void
Enqueue(
    QUEUE_HEAD * QueueHead,
    QUEUE_ENTRY * Entry
    )
{
    // locking?

    QUEUE_ENTRY * FLink;
    QUEUE_ENTRY * BLink;

    Entry->FLink = QueueHead->Sentinel.FLink;
    Entry->BLink = &QueueHead->Sentinel;
    Entry->FLink->BLink = Entry;
    QueueHead->Sentinel.FLink = Entry;
}

QUEUE_ENTRY *
Dequeue(
    QUEUE_HEAD * QueueHead
    )
{
    QUEUE_ENTRY * Entry;

    // locking?

    Entry = NULL;    
    if (QueueHead->Sentinel.BLink != &QueueHead->Sentinel) {
        // Remove the first thing in
        Entry = QueueHead->Sentinel.BLink;
        QueueHead->Sentinel.BLink = Entry->BLink;
        Entry->BLink->FLink = &QueueHead->Sentinel;

        // Nuke stale links
        Entry->BLink = NULL;
        Entry->FLink = NULL;
    }

    return Entry;    
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

