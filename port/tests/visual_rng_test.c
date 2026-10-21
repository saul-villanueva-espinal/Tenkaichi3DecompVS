#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define setenv(k,v,o) _putenv(k "=" v)
#define unsetenv(k) _putenv(k "=")
#endif
extern unsigned long long gPortRandNext;
extern void Port_Srand(unsigned);
extern int Port_Rand(void),Port_VisualRand(void),Port_VisualRandIntRange(int,int);
int main(void) {
    unsigned long long before;int i,expected;
    setenv("BT3_COOP_HOST","29740",1);unsetenv("BT3_TOURNAMENT");
    Port_Srand(123);before=gPortRandNext;
    for(i=0;i<1000;i++) assert(Port_VisualRandIntRange(-5,5)>=-5);
    assert(gPortRandNext==before);
    unsetenv("BT3_COOP_HOST");
    Port_Srand(123);expected=Port_Rand();Port_Srand(123);
    assert(Port_VisualRand()==expected); /* classic offline generator is unchanged */
    setenv("BT3_COOP_JOIN","127.0.0.1:29740",1);setenv("BT3_TOURNAMENT","4",1);
    Port_Srand(123);assert(Port_VisualRand()==expected);
    puts("visual_rng_test: personal raid effects leave simulation RNG unchanged; classic RNG preserved");
    return 0;
}
