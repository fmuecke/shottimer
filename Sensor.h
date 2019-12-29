#ifndef _TEMPERATURE_SENSOR_H_INCLUDED_
#define _TEMPERATURE_SENSOR_H_INCLUDED_

#include <TSIC.h> // https://github.com/Schm1tz1/arduino-tsic

struct Sensor {
	
	Sensor(int pin) : _sensor {2} {
		uint16_t v = 0;
		_sensor.getTemperature(&v);
		_isConnected = _sensor.calc_Celsius(&v) > 0;
		_resetValues();
	}
	
	bool IsConnected() const { return _isConnected; }
	
	void ReadNext() {
		uint16_t data = 0;

		if(_sensor.getTemperature(&data)) {
			auto tmp = data;
			int r = tmp - previousValue;
			if( r < 0) {
				r = -r;
			}

			if (r < 50 || errorCount >= 5) {
				previousValue = tmp;
				errorCount = 0;
			}
			else {
				tmp = previousValue;
				errorCount++;
			}

			total -= _values[_currentIndex];
			_values[_currentIndex] = tmp;  
			total += _values[_currentIndex];

			++_currentIndex;

			if (_currentIndex >= Sensor::MaxValues) {
				_currentIndex = 0;
			}

			data = total / Sensor::MaxValues;
		}

		_value = _sensor.calc_Celsius(&data);
	}	
	
	float GetValue() const { return _value; }
	uint16_t GetRawValue() const { return _values[_currentIndex]; }
	
	static const int MaxValues = 10;
	int total = 0;
	uint16_t previousValue = 0;
	unsigned int errorCount = 0;
  
private:
	void _resetValues() {
		for(auto& value : _values) value = 0;    
	}

	uint16_t _values[MaxValues];      // the readings from the analog input
	bool _isConnected { false } ;
	float _value = 0;
	unsigned int _currentIndex = 0;              // the index of the current reading

	TSIC _sensor;
};

#endif // _TEMPERATURE_SENSOR_H_INCLUDED_