#include "FBD.h"

// When any condition is true, it returns true
void TONFunc(TON *pTP)
{
	if (pTP->IN != pTP->PRE)
	{
		pTP->PRE = pTP->IN;
		if (pTP->IN == 1)
			pTP->ET = millis();
	}

	if (pTP->IN)
	{
		if ((pTP->ET + pTP->PT) <= millis())
			pTP->Q = 1;
	}
	else
	{
		pTP->ET = millis();
		pTP->Q = 0;
	}
}

void TOFFunc(TOF *pTP)
{
	if (pTP->IN != pTP->PRE)
	{
		pTP->PRE = pTP->IN;
		if (pTP->IN == false)
			pTP->ET = millis();
	}

	if (pTP->IN == false)
	{
		if ((pTP->ET + pTP->PT) <= millis())
			pTP->Q = false;
		else
			pTP->Q = true;
	}
	else
	{
		pTP->ET = millis();
		pTP->Q = true;
	}
}

// When any condition is true, it returns true
void TPFunc(TP *pTP)
{
	if (pTP->IN != pTP->PRE)
	{
		pTP->PRE = pTP->IN;
		if (pTP->IN == true)
			pTP->ET = pTP->PT + millis();
	}

	if (pTP->ET > millis())
		pTP->Q = true;
	else
	{
		pTP->Q = false;
		pTP->ET = millis();
	}
}

// It should be used with TONFunc together
void RTrgFunc(Rtrg *pTrg)
{
	pTrg->Q = 0;
	if (pTrg->IN != pTrg->PRE)
	{
		pTrg->PRE = pTrg->IN;
		if (pTrg->PRE == 1)
		{
			pTrg->Q = 1;
		}
	}
}

// It should be used with TONFunc together
void FTrgFunc(Ftrg *pTrg)
{
	pTrg->Q = 0;
	if (pTrg->IN != pTrg->PRE)
	{
		pTrg->PRE = pTrg->IN;
		if (pTrg->IN == 0)
		{
			pTrg->Q = 1;
		}
	}
}
