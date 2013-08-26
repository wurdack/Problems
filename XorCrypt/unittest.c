#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>

#include "homework.h"

byte NullKey[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte BlackWhiteKey[] = {0xff, 0x00};
byte WhiteBlackKey[] = {0x00, 0xff};
byte GreyKey[] = {0x55, 0x55, 0x55};
byte GreyKey2[] = {0xaa, 0xaa, 0xaa};
byte RandomishKey[] = {0x87, 0xde, 0x2e, 0x87, 0x6a, 0xd7, 0xdc, 0xcf, 0xc8};

void
TestIterateKey(
    void
    )
{
    byte WorkingKey[128];
    int Index;

    printf("Test IterateKey\n");

    //
    // Iterating null key shouldn't do anything.
    //

    for (Index = 0; Index < 20; ++Index) {
        printf("NullKey ROL %d\n", Index);
        memcpy(WorkingKey, NullKey, sizeof(NullKey));
        IterateKey(WorkingKey, sizeof(NullKey), Index);
        assert(memcmp(WorkingKey, NullKey, sizeof(NullKey)) == 0);
    }

    //
    // Iterating Black and White keys should do something.
    //

    memcpy(WorkingKey, BlackWhiteKey, sizeof(BlackWhiteKey));
    printf("BlackWhiteKey ROL 8\n");
    IterateKey(WorkingKey, sizeof(BlackWhiteKey), 8);
    assert(memcmp(WorkingKey, WhiteBlackKey, sizeof(WhiteBlackKey)) == 0);

    printf("WhiteBlackKey ROL 7\n");
    IterateKey(WorkingKey, sizeof(BlackWhiteKey), 7);
    assert(WorkingKey[0] == 0x7f);
    assert(WorkingKey[1] == 0x80);

    //
    // Iterating a grey key an even number of times should get you the same
    // grey key. Iterating it an odd number of times should get you the other
    // grey key.
    //

    for (Index = 0; Index < 20; ++Index) {
        printf("GreyKey ROL %d\n", Index);
        memcpy(WorkingKey, GreyKey, sizeof(GreyKey));
        IterateKey(WorkingKey, sizeof(GreyKey), Index);
        if (Index % 2) {
            assert(memcmp(WorkingKey, GreyKey2, sizeof(GreyKey)) == 0);

        } else {
            assert(memcmp(WorkingKey, GreyKey, sizeof(GreyKey)) == 0);
        }
    }

    //
    // ROL a random(ish) key every possible amount.
    //

    memcpy(WorkingKey, RandomishKey, sizeof(RandomishKey));
    for (Index = 0; Index < sizeof(RandomishKey) * 8; ++Index) {
        printf("RandomishKey ROL %d\n", Index);
        IterateKey(WorkingKey, sizeof(RandomishKey), Index);
        IterateKey(WorkingKey,
                   sizeof(RandomishKey),
                   (sizeof(RandomishKey) * 8) - Index);
        assert(memcmp(WorkingKey,
                      RandomishKey,
                      sizeof(RandomishKey)) == 0);
    }

    printf("TestIterateKey finished.\n");
}

int main(int argc, char* argv[])
{
    TestIterateKey();
    printf("Done.\n");
    return 0;
}
