
#ifndef __NEXCHECKBOX_H__
#define __NEXCHECKBOX_H__

#include "NexTouch.h"
#include "NexHardware.h"
/**
 * @addtogroup Component 
 * @{ 
 */

/**
 * NexButton component. 
 *
 * Commonly, you want to do something after push and pop it. It is recommanded that only
 * call @ref NexTouch::attachPop to satisfy your purpose. 
 * 
 * @warning Please do not call @ref NexTouch::attachPush on this component, even though you can. 
 */
class NexCheckbox : public NexTouch
{
public: /* methods */
    /**
     * @copydoc NexObject::NexObject(uint8_t pid, uint8_t cid, const char *name);
     */
    NexCheckbox(uint8_t pid, uint8_t cid, const char *name);

    /**
     * Get val attribute of component
     *
     * @param number - buffer storing data retur
     * @return the length of the data 
     */
    uint32_t getValue(uint32_t *number);

    /**
     * Set val attribute of component
     *
     * @param number - To set up the data
     * @return true if success, false for failure
     */
    bool setValue(uint32_t number);

    /**
     * Get bco attribute of component
     *
     * @param number - buffer storing data retur
     * @return the length of the data 
     */
    uint32_t Get_background_color_bco(uint32_t *number);

    /**
     * Set bco attribute of component
     *
     * @param number - To set up the data
     * @return true if success, false for failure
     */
    bool Set_background_color_bco(uint32_t number);

    /**
     * Get pco attribute of component
     *
     * @param number - buffer storing data retur
     * @return the length of the data 
     */
    uint32_t Get_font_color_pco(uint32_t *number);

    /**
     * Set pco attribute of component
     *
     * @param number - To set up the data
     * @return true if success, false for failure
     */
    bool Set_font_color_pco(uint32_t number);
};
/**
 * @}
 */

#endif /* #ifndef __NEXCHECKBOX_H__ */