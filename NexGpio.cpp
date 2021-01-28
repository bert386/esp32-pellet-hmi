

#include "NexGpio.h"

bool NexGpio::pin_mode(uint32_t port, uint32_t mode, uint32_t control_id)
{
    char buf;
    String cmd;

    cmd += "cfgpio ";
    buf = port + '0';
    cmd += buf;
    cmd += ',';
    buf = mode + '0';
    cmd += buf;
    cmd += ',';
    buf = control_id = '0';
    cmd += buf;

    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}

bool NexGpio::digital_write(uint32_t port, uint32_t value)
{
    String cmd;
    char buf;

    cmd += "pio";
    buf = port + '0';
    cmd += buf;
    cmd += '=';
    buf = value + '0';
    cmd += buf;

    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}

uint32_t NexGpio::digital_read(uint32_t port)
{
    uint32_t number;
    char buf;

    String cmd = String("get ");
    cmd += "pio";
    buf = port + '0';
    cmd += buf;

    sendCommand(cmd.c_str());
    recvRetNumber(&number);
    return number;
}

bool NexGpio::analog_write(uint32_t port, uint32_t value)
{
    char buf[10] = {0};
    char c;
    String cmd;

    utoa(value, buf, 10);
    cmd += "pwm";
    c = port + '0';
    cmd += c;
    cmd += '=';
    cmd += buf;

    Serial.print(cmd);
    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}

bool NexGpio::set_pwmfreq(uint32_t value)
{
    char buf[10] = {0};
    String cmd;

    utoa(value, buf, 10);
    cmd += "pwmf";
    cmd += '=';
    cmd += buf;

    sendCommand(cmd.c_str());
    return recvRetCommandFinished();
}

uint32_t NexGpio::get_pwmfreq(uint32_t *number)
{
    String cmd = String("get pwmf");
    sendCommand(cmd.c_str());
    return recvRetNumber(number);
}