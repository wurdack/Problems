/*++

Description:

    This program encrypts a given stream with a given key. The algorithm is
    as follows:
    
        K = K_0 = Key initial value
        K_i = ith Key iteration
        K_ij = jth byte of ith Key iteration
        
        P_k = kth byte of plain text
        E_k = kth byte of encrypted text 
    
        |X| = length of X
    
    Iteration of keys:    
    
        K_i+1 <== k_i ROL 1
    
    Encryption of P:
    
        i <== 0
        j <== 0
        while k < |P|
            E_k <== P_k XOR K_ij
            k <== k+1
            j <== j+1
            if j = |K| 
                i <== i+1
                j <== 0
         
    This algorithm has a couple of nice properties for implementation:
    
      * It trivially easy to implement a single threaded version of the
        algorithm, providing a reference for a more complicated version.
        
      * It is trival to demonstrate that E(E(P)) = P, providing an easy way
        to verify an implementation against itself. 
      
      * The offsets of a character in P and its encrypted counterpart in E are 
        the same. In the multithreaded implementation below, this allows a 
        writer to know always to which file position its output needs to be
        written.
        
      * The key material used to encrypt a character at a given offset can
        be calculated in constant time.
        
    The implementation below divides the encryption work amongst N threads,
    as specified on the command line. Because the problem statement didn't 
    clearly specify whether the N threads included the main thread, the 
    implementation creates N-1 threads (in addition to the main thread) and
    performs work on all N threads.
    
    The simplified description of each thread's work is:
    
      * Read a block
      * Given the offset of the block, calculate the key material with which to
        encrypt the block.
      * Encrypt the block.
      * Write the block.
    
    IO is managed by a struct called the IO_SYNCHRONIZATION_BLOCK. This struct
    contains the condition variables, mutices, and stream information necessary
    to perform concurrent IO.
    
    Reads are synchronized by a reader mutex. A read returns the stream data and
    its offset. The current stream offset is updated under the mutex as well.
    
    Writes are synchronized by a writer mutex. A writer must specify the offset
    in the stream to write to. If the current write stream offset matches the
    requested offset, the write is satisfied immediately, and a condition 
    variable is signaled. If the current write stream offset does not match, 
    the write will block until the condition is signalled, test the current
    write offset for the stream, and satisfy the write or block again as
    appropriate.

Analysis:

    There are obvious improvements that can be made with the implementation.
    
    Key iteration requires one memory allocations. This allocation could be
    done up front once rather than for every iteration. 
    
    Error handling is always fatal, as there is no way to gracefully handle an
    error and leave the encrypted stream in a useable state. At best, some
    (|P| - n) bytes could be output, and given the triviality of the encryption
    mechanism, these would be recoverable. Routines in the implementation 
    will attempt to propagate errors out to the worker thread routine, but it
    would be equally reasonable simply to abort() or exit() when any error 
    condition is detected.
    
    Because the encryption scheme is so simple, computation isn't expected to
    be the bottleneck for the operation. Threads spend most of their time 
    blocked on IO as opposed to doing usefull encryption work in parallel.
    Threads likely have a convoy pattern like this:
    
    T1 |----- Read -----|- E -|----- Write -----|
    T2                  |----- Read -----|- E -||----- Write -----|
    T3                                   |----- Read -----|- E -| |- Write ...
    
    If the input and output streams are on the same spindle of a filesystem,
    it's likely Reads and Writes serialize against one another as well.
    
    It's conceivable that writes could occur out of order. The implementation
    handles this by blocking out of order writes. This will result in idle CPUs
    when the number of threads is less than or equal to number of processors.
    
    The implementation handles encryption in fixed size blocks. Tuning these 
    blocks to some ideal size for the given filesystem would likely affect 
    performance. The current implementation uses a block size of 64k. An
    implementation that took the block size from the command line would allow
    one to anyalyze performance for various block sizes.

--*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "homework.h"

#if !defined(UNIT_TEST)

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

    The process exits with a non-zero error code on failure, or zero if the
    stream was successfully encrypted to end-of-file.

*/

{

    int Index;
    IO_SYNCHRONIZATION_BLOCK IoSyncBlock;
    byte * Key;
    size_t KeyLength;
    OPTIONS Options;
    int Result;
    WORKER_CONTEXT WorkerContext;
    pthread_t * WorkerThreads;

    if (ParseCommandLine(argc, argv, &Options) != 0) {
        exit(1);
    }
    
    // 
    // Read the key.
    //

    if (ReadKey(Options.KeyFileName, &Key, &KeyLength) != 0) {
        exit(1);
    }

    if (KeyLength == 0) {
        fprintf(stderr, "Keyfile has zero bytes\n");
        exit(1);
    }

#if defined(REFERENCE_IMPL)

    //
    // This is the trivial reference implementation. It reads one byte at a 
    // time, XORs it with key material, and writes it out. The key is rotated 
    // one bit once the material is exhausted.
    //
    
    for (Index = 0;; ++Index) {
        int Input;
        
        Input = fgetc(stdin);
        if (Input == EOF) {
            break;
        }

        if (Index == KeyLength) {
            IterateKey(Key, KeyLength, 1);
            Index = 0;
        }
        
        assert(Input <= 0xff);
        Input ^= Key[Index];
        fputc(Input, stdout);
    }

#else
    
    //
    // This is the multithreaded implementation. The implementation spawns
    // N-1 threads, and runs one instance of the algorithm on the current 
    // thread (resulting in N threads running the algorithm).
    //
    
    //
    // Initialize the IO state.
    //

    IoSyncBlock.ReadOffset = 0;                
    IoSyncBlock.WriteOffset = 0;
    IoSyncBlock.InputStream = stdin;
    IoSyncBlock.OutputStream = stdout;
    pthread_mutex_init(&IoSyncBlock.ReadLock, NULL);
    pthread_mutex_init(&IoSyncBlock.WriteLock, NULL);
    pthread_cond_init(&IoSyncBlock.WriteEvent, NULL);

    //
    // All threads will get a pointer to the same context.
    //
    
    WorkerContext.IoSyncBlock = &IoSyncBlock;
    WorkerContext.Key = Key;
    WorkerContext.KeyLength = KeyLength;
    WorkerContext.Options = &Options;

    //
    // Create additional worker threads as necessary.
    //
    
    if (Options.ThreadCount > 1) {
        WorkerThreads = calloc(Options.ThreadCount - 1, sizeof(pthread_t));
        if (WorkerThreads == NULL) {
            ErrorExit(errno, "Failure allocating thread array.\n");
        }
        
        for (Index = 0; Index < Options.ThreadCount - 1; ++Index) {
            Result = pthread_create(&WorkerThreads[Index],
                                    NULL, 
                                    WorkerThreadRoutine,
                                    &WorkerContext);
                                    
            if (Result != 0) {
                ErrorExit(Result, "Failed creating thread\n");
            }
        }
    }
    
    //
    // This thread gets to work, too.
    //
    
    WorkerThreadRoutine(&WorkerContext);
    
    //
    // Wait for the other worker threads to complete.
    //
    
    if (Options.ThreadCount > 1) {
        for (Index = 0; Index < Options.ThreadCount - 1; ++Index) {
            pthread_join(WorkerThreads[Index], NULL);
        }
        
        free(WorkerThreads);
    }

#endif
    
    //
    // Clean up as necessary.
    //

    free(Key);
    exit(0);
}

#endif

//
// ------------------------------------------------------------------- File I/O
//

int
ReadBlock(
    IO_SYNCHRONIZATION_BLOCK * IoSyncBlock,
    byte * Buffer,
    size_t BufferLength,
    size_t * Offset,
    size_t * BytesRead
    )
    
/*++

Description:

    This routine reads a block of bytes from a stream synchronously with other
    reads. If another read is in progress, this routine will block until it 
    completes. After reading the block, the ReadOffset in the IoSyncBlock will
    be updated with the current offset of the input stream.
    
Arguments:

    IoSyncBlock - Supplies a structure with the current state of the input
        stream. The routine updates the structure as necessary on exit.
        
    Buffer - Supplies a pointer to the buffer that will receive the stream 
        bytes.
        
    BufferLength - Supplies the length of the buffer. The routine will attempt
        to read up to BufferLength bytes.
        
    Offset - Supplies a pointer to memory that receives the stream offset from 
        which the bytes were read.
        
    BytesRead - Supplies a pointer to memory that receives the number of bytes
        read from the stream.

Return Value:

    The routine returns 0 on success, or non-zero if there was an error reading
    the input stream.
    
--*/

{

    pthread_mutex_lock(&IoSyncBlock->ReadLock);

    *BytesRead = fread(Buffer, 1, BufferLength, IoSyncBlock->InputStream);
    *Offset = IoSyncBlock->ReadOffset;
    IoSyncBlock->ReadOffset += *BytesRead;

    pthread_mutex_unlock(&IoSyncBlock->ReadLock);
    
    if ((*BytesRead != BufferLength) && ferror(IoSyncBlock->InputStream)) {
        return 1;

    } else {
        return 0;
    }
}

int
WriteBlock(
    IO_SYNCHRONIZATION_BLOCK * IoSyncBlock,
    byte * Buffer,
    size_t BufferLength,
    size_t Offset
    )
    
/*++

Description:

    This routine writes a block of bytes to the output stream specified in the
    supplied IoSyncBlock. If the current offset of the output stream is not 
    aligned with the offset of the buffer, the routine will block until other
    threads have output the bytes that precede the buffer. Accordingly, writes
    are serialized as one would expect.
    
Arguments:

    IoSyncBlock - Supplies the current state of the output stream.
    
    Buffer - Supplies a pointer to the bytes to be written.
    
    BufferLength - Supplies the number of bytes to be written.
    
    Offset - Supplies the desired offset of the buffer in the output stream.
    
Return Value:

    Returns 0 on success.
    
--*/
    
{
    size_t BytesWritten;
    
    pthread_mutex_lock(&IoSyncBlock->WriteLock);
    
    //
    // Repeat until the block is written.
    //
    
    for (;;) {
    
        //
        // If the offset of this block matches the current write stream offset,
        // write the block out immediately.
        //
        
        if (IoSyncBlock->WriteOffset >= Offset) {
        
            //
            // If the current offset somehow passed this block, then some
            // earlier write failed. Abandon.
            //
                  
            if (IoSyncBlock->WriteOffset != Offset) {
                fprintf(stderr, "Mismatched offsets writing stream\n");
                pthread_mutex_unlock(&IoSyncBlock->WriteLock);
                return 1;
            }
            
            BytesWritten = fwrite(Buffer, 
                                  1, 
                                  BufferLength, 
                                  IoSyncBlock->OutputStream);
    
            if (BytesWritten != BufferLength) {
                fprintf(stderr, "Stream write failed\n");
                pthread_mutex_unlock(&IoSyncBlock->WriteLock);
                return 1;
            }
            
            //
            // Update the current write offset, and broadcast to waiters that
            // the stream offset has been updated.
            //
            
            IoSyncBlock->WriteOffset += BufferLength;
            pthread_cond_broadcast(&IoSyncBlock->WriteEvent);
            break;

        } else {
        
            //
            // Otherwise this write needs to wait at least one other write 
            // earlier in the stream to complete.
            //
            
            pthread_cond_wait(&IoSyncBlock->WriteEvent, 
                              &IoSyncBlock->WriteLock);
        }
    }
    
    pthread_mutex_unlock(&IoSyncBlock->WriteLock);
    
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

/*++

Description:

    This routine parses the command line options, defined in the comments for
    main().
    
Arguments:

    argc - Supplies the count of arguments.
    
    argv - Supplies the array of arguments.
    
    Options - Supplies a pointer to a structure that receives the parsed 
        arguments.
        
Return Value:

    Returns zero (0) on success, or non-zero otherwise.
    
--*/

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
    
    if (ThreadCount <= 0) {
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
    
/*++

Description:

    This routine reads key material from a given file.
    
Arguments:

    FileName - Supplies the name of the file from which to read the key.
    
    Key - Supplies a pointer to memory that will receive a pointer to the key.
        The returned key must be freed with free().
        
    KeyLength - Supplies a pointer to memory that receives the length of the 
        key.
        
Return Value:

    Returns zero (0) on success, non-zero on failure.

--*/

{

    FILE * File;
    struct stat FileStats;
    byte * KeyMemory;
    int Result;
    
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
    
    fclose(File);
    *Key = KeyMemory;
    *KeyLength = FileStats.st_size;
    return 0;
}

int
IterateKey(
    byte * Key,
    size_t KeyLength,
    int Iteration
    )
    
/*++

Description:

    This routine iterates a key a given number of times. A key is iterated by
    rotating all of its bits to the left by one bit. This routine overwrites
    the input key.
    
Arguments:

    Key - Supplies the key.
    
    KeyLength - Supplies the length of the key.
    
    Iteration - Supplies the number of bits to rotate the key to the left.
    
Return Value:

    Returns zero (0) on success, non-zero on failure to allocate working memory
    to iterate the key.
    
--*/

{

    int Index;
    int Shift;
    byte * WorkingKey;

    //
    // Allocate working space for new key.
    //
    
    WorkingKey = malloc(KeyLength);
    if (WorkingKey == NULL) {
        fprintf(stderr, "Memory allocation failed while iterating key\n");
        return 1;
    }
    
    //
    // Do a byte-wise rotation of the key.
    //

    Shift = Iteration / 8;
    Shift %= KeyLength;
    for (Index = 0; Index < KeyLength; ++Index) {
        if (Index + Shift < KeyLength) {
            WorkingKey[Index] = Key[Index + Shift];
        
        } else {
            WorkingKey[Index] = Key[Index + Shift - KeyLength];
        }
    }
    
    //
    // Do the bit-wise rotation of the key
    //
    
    Shift = Iteration % 8;
    for (Index = 0; Index < KeyLength; ++Index) {
        Key[Index] = WorkingKey[Index] << Shift;
        if (Index + 1 < KeyLength) {
            Key[Index] |= WorkingKey[Index + 1] >> (8 - Shift);
           
        } else {
            Key[Index] |= WorkingKey[0] >> (8 - Shift);
        }
    }

    free(WorkingKey);
    
    return 0;
}

int
Encrypt(
    byte * Block,
    size_t BlockLength,
    size_t BlockOffset,
    byte * Key,
    size_t KeyLength
    )
    
/*++
    
Description:

    This routine encrypts a block of memory with a given key.
    
Parameters:

    Block - Supplies the block of memory to encrypt.
    
    BlockLength - Supplies the length of the block of memory.
    
    BlockOffset - Offset of the block in the original stream.
    
    Key - Supplies the key material with which the block is encrypted. The key
        material is destroyed by this operation.
        
    KeyLength - Supplies the length of the key.
    
Return Value:

    Returns zero (0) on success, or non-zero on failure.

--*/

{
    int Index;
    int KeyIndex;

    //
    // Calculate the first key to overlap the start of this block.
    //
    
    if (IterateKey(Key, KeyLength, BlockOffset / KeyLength) != 0) {
        return 1;
    }
    
    //
    // Calculate the offset into the key.
    //
    
    KeyIndex = BlockOffset % KeyLength;

    //
    // Encrypt
    //
    
    for (Index = 0;
         Index < BlockLength; 
         ++Index, ++KeyIndex) {
        
        if (KeyIndex == KeyLength) {
            KeyIndex = 0;
            if (IterateKey(Key, KeyLength, 1) != 0) {
                return 1;
            }
        }
        
        Block[Index] = Block[Index] ^ Key[KeyIndex];
    }
    
    return 0;
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
    byte * BlockKey;
    size_t Offset;
    int Result;
    WORKER_CONTEXT * WorkerContext;

    WorkerContext = (WORKER_CONTEXT*)Context;
    Buffer = malloc(WorkerContext->Options->BlockSize);
    if (Buffer == NULL) {
        ErrorExit(errno, "Buffer allocation failure in WorkerThreadRoutine\n");
    }
    
    BlockKey = malloc(WorkerContext->KeyLength);
    if (BlockKey == NULL) {
        ErrorExit(errno, "Key allocation failure in WorkerThreadRoutine\n");
    }
    
    for (;;) {
    
        //
        // Read a block from the stream.
        //
        
        Result = ReadBlock(WorkerContext->IoSyncBlock,
                           Buffer,
                           WorkerContext->Options->BlockSize,
                           &Offset,
                           &BytesRead);
                           
        if (Result != 0) {
            ErrorExit(ferror(WorkerContext->IoSyncBlock->InputStream),
                      "An error occured while reading the input stream\n");
        }

        if (BytesRead > 0) {
            memcpy(BlockKey, WorkerContext->Key, WorkerContext->KeyLength);
            Result = Encrypt(Buffer, 
                             BytesRead, 
                             Offset,
                             BlockKey, 
                             WorkerContext->KeyLength);
                             
            if (Result != 0) {
                exit(1);
            }
            
            //
            // Write out the encrypted block.
            //
            
            Result = WriteBlock(WorkerContext->IoSyncBlock,
                                Buffer,
                                BytesRead,
                                Offset);

            if (Result != 0) {
                exit(1);
            }
        }

        //
        // Work is complete when EOF is reached.
        //
        
        if (feof(WorkerContext->IoSyncBlock->InputStream)) {
            break;
        }
    }

    free(BlockKey);
    free(Buffer);
        
    return NULL;
}

void
ErrorExit(
    int ErrorNumber,
    char const * Format,
    ...
    )

/*++

Description:

    This routine formats an error code, prints a formatted message, then exits
    the process with exit code 1.
    
Arguments:

    ErrorNumber - Supplies the error value to format. Usually this will be 
        errno.
        
    Format - Supplies a format string.
    
    ... - Supplies arguments for the format string.
    
Return Value:

    None. The routine does not return.
    
--*/

{
    va_list Args;
    char ErrorString[256];
    
    va_start(Args, Format);
    vfprintf(stderr, Format, Args);
    va_end(Args);

    //
    // In normal circumstances, ErrorString would be allocated on demand to
    // accommodate the string returned by strerror_r. Besides being overkill 
    // for a failure situation, it's likely the caller of this routine is 
    // handling a low memory condition. Another alloc isn't likely to succeed.
    //
    
    if (strerror_r(ErrorNumber, ErrorString, sizeof(ErrorString)) == 0) {
        fprintf(stderr, "Aborting: %s\n", ErrorString);
    
    } else {
        fprintf(stderr, "Aborting: ErrorNumber = %d\n", ErrorNumber);
    }
    
    exit(1);
}

