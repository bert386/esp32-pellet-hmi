

#ifndef __NEXHOTSPOT_H__
#define __NEXHOTSPOT_H__

#include "NexTouch.h"
#include "NexHardware.h"
/**
 * @addtogroup Component 
 * @{ 
 */

/**
 * NexHotspot component. 
 */
class NexHotspot : public NexTouch
{
public: /* methods */
    /**
     * @copydoc NexObject::NexObject(uint8_t pid, uint8_t cid, const char *name);
     */
    NexHotspot(uint8_t pid, uint8_t cid, const char *name);
};
/**
 * @}
 */

#endif /* #ifndef __NEXHOTSPOT_H__ */
