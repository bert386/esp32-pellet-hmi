
#ifndef DHT_U_H
#define DHT_U_H

#include "Adafruit_Sensor.h"
#include "DHT.h""

#define DHT_SENSOR_VERSION 1

class DHT_Unified {
public:
  DHT_Unified(uint8_t pin, uint8_t type, uint8_t count=6, int32_t tempSensorId=-1, int32_t humiditySensorId=-1);
  void begin();

  class Temperature : public Adafruit_Sensor {
  public:
    Temperature(DHT_Unified* parent, int32_t id);
    bool getEvent(sensors_event_t* event);
    void getSensor(sensor_t* sensor);

  private:
    DHT_Unified* _parent;
    int32_t _id;

  };

  class Humidity : public Adafruit_Sensor {
  public:
    Humidity(DHT_Unified* parent, int32_t id);
    bool getEvent(sensors_event_t* event);
    void getSensor(sensor_t* sensor);

  private:
    DHT_Unified* _parent;
    int32_t _id;

  };

  Temperature temperature() {
    return _temp;
  }

  Humidity humidity() {
    return _humidity;
  }

private:
  DHT _dht;
  uint8_t _type;
  Temperature _temp;
  Humidity _humidity;

  void setName(sensor_t* sensor);
  void setMinDelay(sensor_t* sensor);

};

#endif
