
#ifndef FBD_H_
#define FBD_H_

#include <arduino.h>

typedef struct
{
    unsigned IN : 1;  // IN option
    unsigned PRE : 1; // IN option
    unsigned Q : 1;   // Output
    unsigned long PT; // Set Timeout
    unsigned long ET; // Elapsed time
} TON;

typedef struct
{
    unsigned IN : 1;  // IN option
    unsigned PRE : 1; // IN option
    unsigned Q : 1;   // Output
    unsigned long PT; // Set Timeout
    unsigned long ET; // Elapsed time
} TOF;

typedef struct
{
    unsigned EN : 1;  // Enable option
    unsigned IN : 1;  // IN option
    unsigned PRE : 1; // PRE option
    unsigned Q : 1;   // Output
    unsigned long PT; // Set Timeout
    unsigned long ET; // Elapsed time
} TP;

typedef struct
{
    unsigned IN : 1;
    unsigned PRE : 1;
    unsigned Q : 1;
    unsigned : 5;
} Rtrg;

typedef struct
{
    unsigned IN : 1;
    unsigned PRE : 1;
    unsigned Q : 1;
    unsigned : 5;
} Ftrg;

void TONFunc(TON *pTP);
void TPFunc(TP *pTP);
void RTrgFunc(Rtrg *pTrg);
void FTrgFunc(Ftrg *pTrg);
void TOFFunc(TOF *pTP);

#endif
