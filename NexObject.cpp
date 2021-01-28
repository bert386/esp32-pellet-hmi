
#include "NexObject.h"

NexObject::NexObject(uint8_t pid, uint8_t cid, const char *name)
{
    this->__pid = pid;
    this->__cid = cid;
    this->__name = name;
}

uint8_t NexObject::getObjPid(void)
{
    return __pid;
}

uint8_t NexObject::getObjCid(void)
{
    return __cid;
}

const char *NexObject::getObjName(void)
{
    return __name;
}

void NexObject::printObjInfo(void)
{
    dbSerialPrint("[");
    dbSerialPrint((uint32_t)this);
    dbSerialPrint(":");
    dbSerialPrint(__pid);
    dbSerialPrint(",");
    dbSerialPrint(__cid);
    dbSerialPrint(",");
    if (__name)
    {
        dbSerialPrint(__name);
    }
    else
    {
        dbSerialPrint("(null)");
    }
    dbSerialPrintln("]");
}
