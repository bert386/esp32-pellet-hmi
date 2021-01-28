
#ifndef __NEXHARDWARE_H__
#define __NEXHARDWARE_H__
#include <Arduino.h>
#include "NexConfig.h"
#include "NexTouch.h"

/**
 * @addtogroup CoreAPI 
 * @{ 
 */

/**
 * Init Nextion.  
 * 
 * @return true if success, false for failure. 
 */
bool nexInit(void);

/**
 * Listen touch event and calling callbacks attached before.
 * 
 * Supports push and pop at present. 
 *
 * @param nex_listen_list - index to Nextion Components list. 
 * @return none. 
 *
 * @warning This function must be called repeatedly to response touch events
 *  from Nextion touch panel. Actually, you should place it in your loop function. 
 */
bool nexLoop(NexTouch *nex_listen_list[]);

/**
 * @}
 */

bool recvRetNumber(uint32_t *number, uint32_t timeout = 500);
uint16_t recvRetString(char *buffer, uint16_t len, uint32_t timeout = 500);
void sendCommand(const char *cmd);
bool recvRetCommandFinished(uint32_t timeout = 500);

extern NexTouch *nex_listen[];

#endif /* #ifndef __NEXHARDWARE_H__ */
