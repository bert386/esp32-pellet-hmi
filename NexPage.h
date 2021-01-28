

#ifndef __NEXPAGE_H__
#define __NEXPAGE_H__

#include "NexTouch.h"
#include "NexHardware.h"
/**
 * @addtogroup Component 
 * @{ 
 */

/**
 * A special component , which can contain other components such as NexButton, 
 * NexText and NexWaveform, etc. 
 */
class NexPage : public NexTouch
{
public: /* methods */
    /**
     * @copydoc NexObject::NexObject(uint8_t pid, uint8_t cid, const char *name);
     */
    NexPage(uint8_t pid, uint8_t cid, const char *name);

    /**
     * Show itself. 
     * 
     * @return true if success, false for faileure.
     */
    bool show(void);
};
/**
 * @}
 */

#endif /* #ifndef __NEXPAGE_H__ */
