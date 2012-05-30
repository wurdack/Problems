all : XorCrypt UnitTest XorCryptRef

clean : 
	rm XorCrypt UnitTest
        
XorCrypt : homework.c homework.h
	cc -g -o XorCrypt -lpthread homework.c

XorCryptRef : homework.c homework.h
	cc -g -o XorCryptRef -lpthread -DREFERENCE_IMPL homework.c
	
UnitTest: homework.c homework.h unittest.c
	cc -g -o UnitTest -lpthread -DUNIT_TEST homework.c unittest.c 
    
    
