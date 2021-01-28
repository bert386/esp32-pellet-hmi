

#ifndef __NEXPROGRESSBAR_H__
#define __NEXPROGRESSBAR_H__

#include "NexTouch.h"
#include "NexHardware.h"
/**
 * @addtogroup Component 
 * @{ 
 */

/**
 * NexProgressBar component. 
 */
class NexProgressBar : public NexObject
{
public: /* methods */
    /**
     * @copydoc NexObject::NexObject(uint8_t pid, uint8_t cid, const char *name);
     */
    NexProgressBar(uint8_t pid, uint8_t cid, const char *name);

    /**
     * Get the value of progress bar. 
     * 
     * @param number - an output parameter to save the value of porgress bar.  
     * 
     * @retval true - success. 
     * @retval false - failed. 
     */
    bool getValue(uint32_t *number);

    /**
     * Set the value of progress bar.
     *
     * @param number - the value of progress bar.  
     *
     * @retval true - success. 
     * @retval false - failed. 
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

#endif /* #ifndef __NEXPROGRESSBAR_H__ */
