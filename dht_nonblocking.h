/*
 * DHT11, DHT21, and DHT22 non-blocking library.
 */


#ifndef _DHT_NONBLOCKING_H
#define _DHT_NONBLOCKING_H

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif


#define DHT_TYPE_11  0
#define DHT_TYPE_21  1
#define DHT_TYPE_22  2


class DHT_nonblocking
{
  public:
    DHT_nonblocking( uint8_t pin, uint8_t type );
    bool measure( float *temperature, float *humidity );

  private:
    bool read_data( );
    bool read_nonblocking( );
    float read_temperature( ) const;
    float read_humidity( ) const;

    uint8_t dht_state;
    unsigned long dht_timestamp;
    uint8_t data[ 6 ];
    const uint8_t _pin, _type, _bit, _port;
    const uint32_t _maxcycles;

    uint32_t expect_pulse( bool level ) const;
};


class DHT_interrupt
{
  public:
  DHT_interrupt( )
  {
    noInterrupts( );
  }
  ~DHT_interrupt( )
  {
    interrupts( );
  }
};


#endif /* _DHT_NONBLOCKING_H */

