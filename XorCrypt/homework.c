#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>

#include "homework.h"

struct {
    byte * Key;
    size_t KeyLength;
    pthread_mutex_t ReadLock;
    size_t ReadOffset;
    pthread_mutex_t WriteLock;
    pthread_cond_t WriteEvent;
    size_t WriteOffset;
} Globals;

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

    int Index;
    OPTIONS Options;
    int Result;
    WORKER_CONTEXT WorkerContext;
    
    pthread_t * WorkerThreads;

    if (ParseCommandLine(argc, argv, &Options) == 0) {

        // 
        // Read the key.
        //

        Result = ReadKey(Options.KeyFileName, &Globals.Key, &Globals.KeyLength);

        if (Result != 0) {
            fprintf(stderr, "Failure reading key\n");
            exit (1);
        }
        
        WorkerThreads = calloc(Options.ThreadCount, sizeof(pthread_t));
        if (WorkerThreads == NULL) {
            fprintf(stderr, "Failure allocating thread pool\n");
            exit (1);
        }
        
        WorkerContext.InputStream = stdin;
        WorkerContext.OutputStream = stdout;
        WorkerContext.Key = Globals.Key;
        WorkerContext.KeyLength = Globals.KeyLength;
        WorkerContext.Options = &Options;
        
        for (Index = 0; Index < Options.ThreadCount - 1; ++Index) {
            pthread_create(&WorkerThreads[Index],
                           NULL, 
                           WorkerThreadRoutine,
                           &WorkerContext);
        }
        
        WorkerThreadRoutine(&WorkerContext);
        
        for (Index = 0; Index < Options.ThreadCount - 1; ++Index) {
            fprintf(stderr, "Wait for thread %d\n", Index);
            pthread_join(WorkerThreads[Index], NULL);
        }
        
        fprintf(stderr, 
                " ReadOffset: %d\n"
                "WriteOffset: %d\n",
                Globals.ReadOffset,
                Globals.WriteOffset);
        
        //
        // Clean up as necessary.
        //

        free(Globals.Key);
    }
                        
    exit(0);
}

//
// ------------------------------------------------------------------- File I/O
//

int
ReadBlock(
    FILE * Stream,
    byte * Buffer,
    size_t BufferLength,
    size_t * Offset,
    size_t * BytesRead
    )
{

    pthread_mutex_lock(&Globals.ReadLock);

    *BytesRead = fread(Buffer, 1, BufferLength, Stream);
    *Offset = Globals.ReadOffset;
    Globals.ReadOffset += *BytesRead;

    pthread_mutex_unlock(&Globals.ReadLock);
    
    if (*BytesRead != BufferLength) {
        if (feof(Stream)) {
            fprintf(stderr, "Reached eof\n");
            return 1; // rwurdack_todo - eof
        } else {
            err(ferror(Stream), "Read Error\n");
            return 2; // rwurdack_todo - fatal
        }
    } else {
        return 0; // rwurdack_todo - ok
    }
}

int
WriteBlock(
    FILE * Stream,
    byte * Buffer,
    size_t BufferLength,
    size_t Offset
    )
{
    size_t BytesWritten;
    
    pthread_mutex_lock(&Globals.WriteLock);
    
    for (;;) {
        if (Globals.WriteOffset >= Offset) {
            assert(Globals.WriteOffset == Offset);
            BytesWritten = fwrite(Buffer, 
                                  1, 
                                  BufferLength, 
                                  Stream);
    
            // rwurdack_todo -- should abort
            assert(BytesWritten == BufferLength); 
            Globals.WriteOffset += BufferLength;
            pthread_cond_broadcast(&Globals.WriteEvent);
            break;

        } else {
            pthread_cond_wait(&Globals.WriteEvent, &Globals.WriteLock);
        }
    }
    
    pthread_mutex_unlock(&Globals.WriteLock);
    
    return 0;
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

int
ReadKey(
    char const * FileName,
    byte ** Key,
    size_t * KeyLength
    )
{
    FILE * File;
    struct stat FileStats;
    int Result;
    byte * KeyMemory;
    
    Result = stat(FileName, &FileStats);
    if (Result != 0) {
        fprintf(stderr, "File stats not retrieved for %s\n", FileName);
        return 1;
    }
    
    KeyMemory = malloc(FileStats.st_size);
    if (KeyMemory == NULL) {
        fprintf(stderr, "Memory allocation failure for key\n");
        return 1;
    }

    File = fopen(FileName, "rb");
    if (File == NULL) {
        fprintf(stderr, "Failure opening keyfile %s\n", FileName);
        free(KeyMemory);
        return 1;
    }
    
    Result = fread(KeyMemory, 1, FileStats.st_size, File);
    
    if (Result != FileStats.st_size) {
        fprintf(stderr, "Failure reading key from file %s\n", FileName);
        free(KeyMemory);
        fclose(File);
        return 1;
    }
    
    *Key = KeyMemory;
    *KeyLength = FileStats.st_size;
    fclose(File);
    
    return 0;
}

void
IterateKey(
    byte * Key,
    size_t KeyLength,
    int Iteration
    )
{
    int Index;
    int Shift;
    
    Shift = Iteration % 8;
    for (Index = 0; Index < KeyLength; ++Index) {
        //
        // Left rotate this byte.
        //
        
        Key[Index] = (Key[Index] << Shift) | (Key[Index] >> (8 - Shift));
        
        //
        // Add the Index.
        //
        
        Key[Index] += Iteration & 0xff;
    }
}

byte *
CreateBlockKey(
    byte const * Key,
    size_t KeyLength,
    size_t BlockOffset
    )
    
/*++

Description:

    This routine calculates an appropriate initial key for a given file
    offset.
    
Parameters:

    Key - Supplies a pointer the key material.
    
    KeyLength - Supplies the length of the key material.
    
    BlockOffset - Supplies the offset of the block requiring a new key.
       
Return Value:

    Returns zero (0) on success.
    
--*/

{

    int Index;
    int KeyIndex;
    int KeyOffset; 
    byte * KeyOut;
    byte * NthKey;
    int SubIndex;

    KeyOut = malloc(KeyLength * 2);
    if (KeyOut != NULL) {
        NthKey = KeyOut + KeyLength;
        
        // 
        // Find the Index of the key that overlaps the beginning of this block.
        // 
        
        KeyIndex = BlockOffset / KeyLength;

        //
        // Calculate the Nth key
        //
        
        memcpy(NthKey, Key, KeyLength);
        IterateKey(NthKey, KeyLength, KeyIndex);
            
        // 
        // Find the offset into the NthKey to start the new key.
        //
        
        KeyOffset = BlockOffset % KeyLength;
        
        //
        // Copy the end bytes of the Nth key to the start of the new key.
        //
        
        for (Index = 0; Index < KeyLength - KeyOffset; ++Index) {
            KeyOut[Index] = NthKey[Index + KeyOffset];
        }
        
        //
        // Copy the beginning bytes of the (N+1)the key to the end of the new
        // key.
        //
        
        IterateKey(NthKey, KeyLength, 1);
        for (; Index < KeyLength; ++Index) {
            KeyOut[Index] = NthKey[Index - KeyOffset];
        }
    }
    
    return KeyOut;
}

void
Encrypt(
    byte * Block,
    size_t BlockLength,
    byte * Key,
    size_t KeyLength
    )
    
/*++
    
Description:

    This routine encrypts a block of memory with a given key.
    
Parameters:

    Block - Supplies the block of memory to encrypt.
    
    BlockLength - Supplies the length of the block of memory.
    
    Key - Supplies the key material with which the block is encrypted. The key
        material is destroyed by this operation.
        
    KeyLength - Supplies the length of the key.
    
Return Value:

    None.

--*/

{
    int Index;
    int KeyIndex;
    
    for (KeyIndex = 0, Index = 0;
         Index < BlockLength; 
         ++Index, ++KeyIndex) {
        
        if (KeyIndex == KeyLength) {
            KeyIndex = 0;
            IterateKey(Key, KeyLength, 1);
        }
        
        Block[Index] = Block[Index] ^ Key[KeyIndex];
    }
}

//
// -------------------------------------------------------------- Worker Thread
//

void *
WorkerThreadRoutine(
    void * Context
    )
{
    byte * Buffer;
    size_t BytesRead;
    byte * Key;
    size_t Offset;
    int Result;
    WORKER_CONTEXT * WorkerContext;

    WorkerContext = (WORKER_CONTEXT*)Context;
    Buffer = malloc(WorkerContext->Options->BlockSize);
    
    for (;;) {
    
        //
        // Read a block from the stream.
        //
        
        Result = ReadBlock(WorkerContext->InputStream,
                           Buffer,
                           WorkerContext->Options->BlockSize,
                           &Offset,
                           &BytesRead);
                           
        if (Result == 2) {
            fprintf(stderr, "WorkerThreadRoutine barfed\n");
            exit(2);
        }
 
        if (Result == 1) {
            break;
        }

        assert(BytesRead > 0); // rwurdack_todo -- handle this.
        
        //
        // Calculate the key appropriate to this block.
        //
         
        Key = CreateBlockKey(WorkerContext->Key,
                             WorkerContext->KeyLength,
                             Offset);

        if (Key == NULL) {
            fprintf(stderr, "Allocation failure in worker thread\n");
            exit(2);       
        }
        
        Encrypt(Buffer, BytesRead, Key, WorkerContext->KeyLength);
        
        //
        // Write out the encrypted block.
        //
        
        Result = WriteBlock(WorkerContext->OutputStream,
                            Buffer,
                            BytesRead,
                            Offset);
    }

    free(Buffer);
        
    return NULL;
}

