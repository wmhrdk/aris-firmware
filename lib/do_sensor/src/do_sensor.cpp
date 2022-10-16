#include "do_sensor.hpp"

namespace aris {

DissolvedOxygenSensor::DissolvedOxygenSensor(std::uint8_t pin) {
	pin_ = pin;
	temp_sensor_ = nullptr;
}

bool DissolvedOxygenSensor::init(void) {
	voltage_ = 0.0f;
	sat_voltage_ = 0;
	cal_voltage_ = 0;
	cal_temp_ = 0;
	data_ = 0.0f;
	pinMode(pin_, INPUT);
	return true;
}

bool DissolvedOxygenSensor::update(void) {
	if (temp_sensor_ == nullptr) {
		return false;
	}
	std::uint16_t current_voltage_ = (uint16_t)(getVoltage() * 1000.0f);
	std::uint16_t current_temp_ = (uint16_t)temp_sensor_->getData();
	sat_voltage_ = cal_voltage_ + 35 * current_temp_ - cal_temp_ * 35;
	data_ = (float)(current_voltage_ * g_do_table[current_temp_] / sat_voltage_);
	return true;
}

bool DissolvedOxygenSensor::attachTemperatureSensor(std::shared_ptr<Sensor<float>>& ptr) {
	temp_sensor_ = ptr;
	if (temp_sensor_ == ptr) {
		return true;
	}
	else {
		return false;
	}
}

void DissolvedOxygenSensor::setCalibrationVoltage(std::uint16_t voltage) {
	cal_voltage_ = voltage;
}

void DissolvedOxygenSensor::setCalibrationTemperature(std::uint16_t temperature) {
	cal_temp_ = temperature;
}

}
