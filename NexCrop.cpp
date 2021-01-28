
#include "NexCrop.h"

NexCrop::NexCrop(uint8_t pid, uint8_t cid, const char *name)
    : NexTouch(pid, cid, name)
{
}

bool NexCrop::Get_background_crop_picc(uint32_t *number)
{
    String cmd = String("get ");
    cmd += getObjName();
    cmd += ".picc";
    sendCommand(cmd.c_str());
    return recvRetNumber(number);
}

bool NexCrop::Set_background_crop_picc(uint32_t number)
{
    char buf[10] = {0};
    String cmd;

    utoa(number, buf, 10);
    cmd += getObjName();
    cmd += ".picc=";
    cmd += buf;

    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}

bool NexCrop::getPic(uint32_t *number)
{
    String cmd = String("get ");
    cmd += getObjName();
    cmd += ".picc";
    sendCommand(cmd.c_str());
    return recvRetNumber(number);
}

bool NexCrop::setPic(uint32_t number)
{
    char buf[10] = {0};
    String cmd;

    utoa(number, buf, 10);
    cmd += getObjName();
    cmd += ".picc=";
    cmd += buf;

    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}
