

#include "NexPage.h"

NexPage::NexPage(uint8_t pid, uint8_t cid, const char *name)
    : NexTouch(pid, cid, name)
{
}

bool NexPage::show(void)
{
    uint8_t buffer[4] = {0};

    const char *name = getObjName();
    if (!name)
    {
        return false;
    }

    String cmd = String("page ");
    cmd += name;
    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}
