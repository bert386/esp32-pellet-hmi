
#ifndef __NEXOBJECT_H__
#define __NEXOBJECT_H__
#include <Arduino.h>
#include "NexConfig.h"
/**
 * @addtogroup CoreAPI 
 * @{ 
 */

/**
 * Root class of all Nextion components. 
 *
 * Provides the essential attributes of a Nextion component and the methods accessing
 * them. At least, Page ID(pid), Component ID(pid) and an unique name are needed for
 * creating a component in Nexiton library. 
 */
class NexObject
{
public: /* methods */
    /**
     * Constructor. 
     *
     * @param pid - page id. 
     * @param cid - component id.    
     * @param name - pointer to an unique name in range of all components. 
     */
    NexObject(uint8_t pid, uint8_t cid, const char *name);

    /**
     * Print current object'address, page id, component id and name. 
     *
     * @warning this method does nothing, unless debug message enabled. 
     */
    void printObjInfo(void);

protected: /* methods */
    /*
     * Get page id.
     *
     * @return the id of page.  
     */
    uint8_t getObjPid(void);

    /*
     * Get component id.
     *
     * @return the id of component.  
     */
    uint8_t getObjCid(void);

    /*
     * Get component name.
     *
     * @return the name of component. 
     */
    const char *getObjName(void);

private:                /* data */
    uint8_t __pid;      /* Page ID */
    uint8_t __cid;      /* Component ID */
    const char *__name; /* An unique name */
};
/**
 * @}
 */

#endif /* #ifndef __NEXOBJECT_H__ */
