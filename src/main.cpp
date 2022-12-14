#define SerialAT         Serial2
#define GSM_AUTOBAUD_MIN 9600
#define GSM_AUTOBAUD_MAX 115200
#define GSM_PIN          ""

#include <auth.h>
#include <conf.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <TinyGSM.h>
#include <TinyGsmClient.h>
#include <WiFi.h>

#include <memory>
#include <string>
#include <vector>
#include <cmath>

#include <data_acquisition.hpp>
#include <do_sensor.hpp>
#include <ec_sensor.hpp>
#include <ph_sensor.hpp>
#include <sensor.hpp>
#include <temperature_sensor.hpp>
#include <turbidity_sensor.hpp>

using SensorPointer         = std::shared_ptr<aris::Sensor>;
using SensorInterface       = std::shared_ptr<aris::SensorInterface>;
using SensorPointerVector   = std::vector<SensorPointer>;
using SensorInterfaceVector = std::vector<SensorInterface>;

using aris::AnalogPhSensor;
using aris::ConductivitySensor;
using aris::DissolvedOxygenSensor;
using aris::TemperatureSensor;
using aris::TurbiditySensor;

aris::DataAcquisitionManager acquisition_manager;

SensorPointerVector   sensor_pointers;
SensorInterfaceVector sensor_interfaces;


// const std::string apn = "";
// const std::string gprs_user = "";
// const std::string gprs_pass = "";
const std::string ssid           = WIFI_SSID;
const std::string pass           = WIFI_PASSWORD;
const std::string mqtt_server_ip = MQTT_BROKER_IP;

// TinyGsm modem(SerialAT);
WiFiClient wifi_client;
// TinyGsmClient gsm_client(modem);
PubSubClient  mqtt_client(wifi_client);

unsigned long publish_timestamp = 0ul;
unsigned long publish_interval  = 100ul;

template <class Type>
SensorPointer createSensorInstance(std::uint8_t pin) {
	SensorPointer ptr = std::make_shared<Type>(pin);
	return ptr;
}

// void setupModem() {
// 	Serial.println("Starting SerialAT ...");
// 	TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
// 	delay(6000);
// 	Serial.println("Initializing ...");
// 	modem.restart();
// 	Serial.println(modem.getModemInfo());
// 	if (GSM_PIN && modem.getSimStatus() != 3) {
// 		modem.simUnlock(GSM_PIN);
// 	}
// 	Serial.println("Waiting for connection ...");
// 	while (!modem.isGprsConnected()) {
// 		if (!modem.gprsConnect("3gprs", "3gprs", "3gprs")) {
// 			Serial.println("Failed to connect");
// 			delay(5000);
// 		}
// 	}
// 	Serial.println("Connected!");
// }

void setup_wifi() {
	delay(10);
	WiFi.begin(ssid.c_str(), pass.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}
}

void callback(char* topic, byte* message, std::uint32_t length) {
	;
	;
}

void reconnectToMqttBroker() {
	while (!mqtt_client.connected()) {
		if (!mqtt_client.connect("device001", MQTT_USER, MQTT_PASS)) {
			delay(5000);
		}
	}
}

JsonObject packSensorReading(SensorInterfaceVector& si) {
	StaticJsonDocument<196> reading_buffer;
	reading_buffer["temperature"] = si.at(0)->getData();
	reading_buffer["turbidity"] = si.at(1)->getData();
	reading_buffer["ph"] = si.at(2)->getData();
	reading_buffer["conductivity"] = si.at(3)->getData();
	reading_buffer["do"] = si.at(4)->getData();
	JsonObject sensor_reading = reading_buffer.as<JsonObject>();
	return sensor_reading;
}


void setup() {
	Serial.begin(115200);

	sensor_pointers.push_back(
		createSensorInstance<TemperatureSensor>(DS18B20_PIN)
	);
	sensor_pointers.push_back(
		createSensorInstance<TurbiditySensor>(SEN0189_PIN)
	);
	sensor_pointers.push_back(
		createSensorInstance<AnalogPhSensor>(SEN0161_PIN)
	);
	sensor_pointers.push_back(
		createSensorInstance<ConductivitySensor>(DFR0300_PIN)
	);
	sensor_pointers.push_back(
		createSensorInstance<DissolvedOxygenSensor>(SEN0237_PIN)
	);

	sensor_pointers.at(DS18B20_ID)->setLabel("temperature");
	sensor_pointers.at(SEN0189_ID)->setLabel("turbidity");
	sensor_pointers.at(SEN0161_ID)->setLabel("ph");
	sensor_pointers.at(DFR0300_ID)->setLabel("conductivity");
	sensor_pointers.at(SEN0237_ID)->setLabel("do");

	(*sensor_pointers.at(SEN0161_ID)).setOffset(SEN0161_OFFSET);

	sensor_pointers.at(DFR0300_ID)->attach(sensor_pointers.at(DS18B20_ID));
	sensor_pointers.at(SEN0237_ID)->attach(sensor_pointers.at(DS18B20_ID));

	acquisition_manager.spawn(
		DS18B20_ID, sensor_pointers[DS18B20_ID], 2.00f, true
	);
	acquisition_manager.spawn(
		SEN0189_ID, sensor_pointers[SEN0189_ID], 20.0f, true
	);
	acquisition_manager.spawn(
		SEN0161_ID, sensor_pointers[SEN0161_ID], 50.0f, true
	);
	acquisition_manager.spawn(
		DFR0300_ID, sensor_pointers[DFR0300_ID], 1.00f, true
	);
	acquisition_manager.spawn(
		SEN0237_ID, sensor_pointers[SEN0237_ID], 40.0f, true
	);

	for (std::size_t i = 0; i < sensor_pointers.size(); i++) {
		sensor_interfaces.push_back(sensor_pointers.at(i));
	}

	// setupModem();
	Serial.println("Setting up wifi");
	setup_wifi();
	Serial.println("Setting up mqtt server");
	mqtt_client.setServer(mqtt_server_ip.c_str(), MQTT_BROKER_PORT);
	Serial.println("Setting up mqtt callback");
	mqtt_client.setCallback(callback);
}


void loop() {
	if (!mqtt_client.connected()) {
		reconnectToMqttBroker();
	}

	mqtt_client.loop();
	acquisition_manager.asyncRun();

	StaticJsonDocument<96> device_info_json;
	JsonObject device_info = device_info_json.to<JsonObject>();
	device_info["deviceid"] = 1;
	device_info["location"] = "Makassar";

	if ((millis() - publish_timestamp) > publish_interval) {
		StaticJsonDocument<256> message_json;
		JsonArray message_array = message_json.to<JsonArray>();
		message_array.add(packSensorReading(sensor_interfaces));
		message_array.add(device_info);
		char message_buffer[256];
		std::size_t buffer_size = serializeJson(message_json, message_buffer);
		mqtt_client.publish("/sensors/water", message_buffer, buffer_size);
		publish_timestamp = millis();
	}
}

