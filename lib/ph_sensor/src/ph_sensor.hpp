#ifndef _ARIS_PH_SENSOR_HPP_
#define _ARIS_PH_SENSOR_HPP_

#include <Arduino.h>
#include <sensor.hpp>

namespace aris {

class AnalogPhSensor : public Sensor {
	private:
		float offset_;

	public:
		AnalogPhSensor(std::uint8_t pin, float offset);
		bool init(void) override;
		bool update(void) override;
};

}

#endif
