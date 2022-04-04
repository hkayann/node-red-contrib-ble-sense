/*
 * Example to send all data in one struct
 */

#include "ArduinoBLE.h"
#include "Arduino_LSM9DS1.h"
#include "Arduino.h"
#include "Arduino_BHY2.h"
#include "Nicla_System.h"

//----------------------------------------------------------------------------------------------------------------------
// BLE UUIDS
//----------------------------------------------------------------------------------------------------------------------

// IMU UUIDS
#define BLE_UUID_IMU_SERVICE              "5543e0d651ca11ecbf630242ac130002"
#define BLE_UUID_ACC_CHAR                 "5543e32e51ca11ecbf630242ac130002"
#define BLE_UUID_GYRO_CHAR                "5543e55451ca11ecbf630242ac130002"
#define BLE_UUID_MAG_CHAR                 "5543e64451ca11ecbf630242ac130002"

// ENV UUIDS
#define BLE_UUID_ENVIRONMENTAL_SENSING_SERVICE    "181A"
#define BLE_UUID_TEMPERATURE                      "2A6E"
#define BLE_UUID_HUMIDITY                         "2A6F"
#define BLE_UUID_PRESSURE                         "2A6D"

// QUATERNION UUID
#define BLE_UUID_Q_SERVICE              "a2278074b3fe11ecb9090242ac120002"
#define BLE_UUID_Q_CHAR                 "a2278362b3fe11ecb9090242ac120002"

//----------------------------------------------------------------------------------------------------------------------
// APP & I/O
//----------------------------------------------------------------------------------------------------------------------

//#define NUMBER_OF_SENSORS 3

//#define ACC_SENSOR_UPDATE_INTERVAL                (500) // node-red-dashboard can't handle 1 ms, can handle 100 ms.
//#define MAG_SENSOR_UPDATE_INTERVAL                (500)
//#define GYRO_SENSOR_UPDATE_INTERVAL               (500)

#define IMU_UPDATE_INTERVAL                         (500)
#define ENV_UPDATE_INTERVAL                         (500)
#define Q_UPDATE_INTERVAL                           (500)

// IMU parameters
union imu_data {
  struct __attribute__((packed)) {
    float values[3]; // float array for data (it holds 3)
    bool updated = false;
  };
  uint8_t bytes[3 * sizeof(float)]; // size as byte array 
};
union imu_data accData;
union imu_data gyroData;
union imu_data magData;

SensorXYZ accelerometer(SENSOR_ID_ACC);
SensorXYZ gyro(SENSOR_ID_GYRO);
SensorXYZ mag(SENSOR_ID_MAG);

float accX, accY, accZ, gyroX, gyroY, gyroZ, magX, magY, magZ;

// ENV parameters
typedef struct __attribute__( ( packed ) )
{
  float temperatureValue;
  float humidityValue;
  float pressureValue;
  bool updated = false;
} env_data_t;

env_data_t envData;

Sensor temperature(SENSOR_ID_TEMP);
Sensor humidity(SENSOR_ID_HUM);
Sensor pressure(SENSOR_ID_BARO);

float temp, hum, pres;

// IMU parameters
union q_data {
  struct __attribute__((packed)) {
    float values[4]; // float array for data (it holds 3)
    bool updated = false;
  };
  uint8_t bytes[4 * sizeof(float)]; // size as byte array 
};
union q_data qData;

SensorQuaternion rotation(SENSOR_ID_RV);

float qX, qY, qZ, qW;

//const int BLE_LED_PIN = LED_BUILTIN;
//const int RSSI_LED_PIN = LED_PWR;
//----------------------------------------------------------------------------------------------------------------------
// BLE
//----------------------------------------------------------------------------------------------------------------------

#define BLE_DEVICE_NAME                           "Nicla"
#define BLE_LOCAL_NAME                            "Nicla"

BLEService IMUService(BLE_UUID_IMU_SERVICE);
BLECharacteristic accCharacteristic(BLE_UUID_ACC_CHAR, BLERead | BLENotify, sizeof accData.bytes);
BLECharacteristic gyroCharacteristic(BLE_UUID_GYRO_CHAR, BLERead | BLENotify, sizeof gyroData.bytes);
BLECharacteristic magCharacteristic(BLE_UUID_MAG_CHAR, BLERead | BLENotify, sizeof magData.bytes);

BLEService environmentalSensingService(BLE_UUID_ENVIRONMENTAL_SENSING_SERVICE);
BLEShortCharacteristic temperatureCharacteristic(BLE_UUID_TEMPERATURE, BLERead | BLENotify);
BLEUnsignedShortCharacteristic humidityCharacteristic(BLE_UUID_HUMIDITY, BLERead | BLENotify);
BLEUnsignedLongCharacteristic pressureCharacteristic(BLE_UUID_PRESSURE, BLERead | BLENotify);

BLEService qService(BLE_UUID_Q_SERVICE);
BLECharacteristic qCharacteristic(BLE_UUID_Q_CHAR, BLERead | BLENotify, sizeof qData.bytes);

#define BLE_LED_PIN                               LED_BUILTIN

//----------------------------------------------------------------------------------------------------------------------
// SETUP
//----------------------------------------------------------------------------------------------------------------------

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  Serial.println("BLE Example - IMU Service (ESS)");
  pinMode(BLE_LED_PIN, OUTPUT);
  digitalWrite(BLE_LED_PIN, LOW);
  initialize();
}

void loop() {

  BHY2.update();
  bleTask();
  if (accSensorTask()) {
    accPrintTask();
  }
  if (gyroSensorTask()){
    gyroPrintTask();
  }
  if (magSensorTask()){
    magPrintTask();
  }
  if (envSensorTask()){
    envPrintTask();
  }
  if (qSensorTask()){
    qPrintTask();
  }
}

//----------------------------------------------------------------------------------------------------------------------
// SENSOR TASKS
/*
 * We define bool function for each sensor.
 * Function returns true if sensor data are updated.
 * Allows us to define different update intervals per sensor data.
 */
//----------------------------------------------------------------------------------------------------------------------

bool accSensorTask() {
  static long previousMillis2 = 0;
  unsigned long currentMillis2 = millis();
  if (currentMillis2 - previousMillis2 < IMU_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis2 = currentMillis2;
  
  // get raw acc.
  accX = accelerometer.x();
  accY = accelerometer.y();
  accZ = accelerometer.z();
  // conver to m/s-2
  accX = (accX / 4096) * 9.80665;
  accY = (accY / 4096) * 9.80665;
  accZ = (accZ / 4096) * 9.80665;
  // set the values
  accData.values[0] = accX;
  accData.values[1] = accY;
  accData.values[2] = accZ;
  accData.updated = true;
  
  return accData.updated;
}

bool gyroSensorTask() {
  static long previousMillis2 = 0;
  unsigned long currentMillis2 = millis();
  if (currentMillis2 - previousMillis2 < IMU_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis2 = currentMillis2;
  // get raw gyro
  gyroX = gyro.x();
  gyroY = gyro.y();
  gyroZ = gyro.z();
  // convert to dps
  gyroX = gyroX / (65536 / 4000);
  gyroY = gyroY / (65536 / 4000);
  gyroZ = gyroZ / (65536 / 4000);
  // set the values
  gyroData.values[0] = gyroX;
  gyroData.values[1] = gyroY;
  gyroData.values[2] = gyroZ;
  gyroData.updated = true;
  
  return gyroData.updated;
}

bool magSensorTask() {
  static long previousMillis3 = 0;
  unsigned long currentMillis3 = millis();
  if (currentMillis3 - previousMillis3 < IMU_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis3 = currentMillis3;
  // get raw mag
  magX = mag.x();
  magY = mag.y();
  magZ = mag.z();
  //convert to uT
  magX = magX / (65536 / 2600); 
  magY = magY / (65536 / 2600); 
  magZ = magZ / (65536 / 5000);
  // set the values  
  magData.values[0] = magX;
  magData.values[1] = magY;
  magData.values[2] = magZ;
  magData.updated = true;
  
  return magData.updated;
}

bool envSensorTask() {
  static long previousMillis4 = 0;
  unsigned long currentMillis4 = millis();
  if (currentMillis4 - previousMillis4 < ENV_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis4 = currentMillis4;
   
  envData.temperatureValue = temperature.value();
  envData.humidityValue = humidity.value();
  envData.pressureValue = pressure.value();
  envData.updated = true;

  return envData.updated;
}

bool qSensorTask() {
  static long previousMillis5 = 0;
  unsigned long currentMillis5 = millis();
  if (currentMillis5 - previousMillis5 < Q_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis5 = currentMillis5;
  // get raw quaternions
  qX = rotation.x();
  qY = rotation.y();
  qZ = rotation.z();
  qW = rotation.w();

  // set the values  
  qData.values[0] = qX;
  qData.values[1] = qY;
  qData.values[2] = qZ;
  qData.values[3] = qW;
  qData.updated = true;
  
  return qData.updated;
}

//----------------------------------------------------------------------------------------------------------------------
//  BLE SETUP
/*
 * Determine which services/characteristics to be advertised.
 * Determine the device name.
 * Set event handlers.
 * Set inital value for characteristics.
 */
//----------------------------------------------------------------------------------------------------------------------

bool setupBleMode() {
  if (!BLE.begin()) {
    return false;
  }

  // set advertised local name and service UUID:
  BLE.setDeviceName(BLE_DEVICE_NAME);
  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(IMUService);
  BLE.setAdvertisedService(environmentalSensingService);
  BLE.setAdvertisedService(qService);
  
  // BLE add IMU characteristics
  IMUService.addCharacteristic(accCharacteristic);
  IMUService.addCharacteristic(gyroCharacteristic);
  IMUService.addCharacteristic(magCharacteristic);

  // BLE add ENV characteristics
  environmentalSensingService.addCharacteristic(temperatureCharacteristic);
  environmentalSensingService.addCharacteristic(humidityCharacteristic);
  environmentalSensingService.addCharacteristic(pressureCharacteristic);

  // BLE add Quaternion characteristics
  qService.addCharacteristic(qCharacteristic);
  
  // add service
  BLE.addService(IMUService);
  BLE.addService(environmentalSensingService);
  BLE.addService(qService);

  // set the initial value for characteristics:
  accCharacteristic.writeValue(accData.bytes, sizeof accData.bytes);
  gyroCharacteristic.writeValue(gyroData.bytes, sizeof gyroData.bytes);
  magCharacteristic.writeValue(magData.bytes, sizeof magData.bytes);
  temperatureCharacteristic.writeValue(0);
  humidityCharacteristic.writeValue(0);
  pressureCharacteristic.writeValue(0);
  qCharacteristic.writeValue(qData.bytes, sizeof qData.bytes);
  
  // set BLE event handlers
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // start advertising
  BLE.advertise();

  return true;
}

void bleTask()
{
  const uint32_t BLE_UPDATE_INTERVAL = 10;
  static uint32_t previousMillis = 0;
  uint32_t currentMillis = millis();
  if (currentMillis - previousMillis >= BLE_UPDATE_INTERVAL) {
    previousMillis = currentMillis;
    BLE.poll();
  }
  if (accData.updated) {
    // Bluetooth does not define accelerometer
    // Units in G
    int16_t accelerometer_X = round(accData.values[0] * 100.0);
    int16_t accelerometer_Y = round(accData.values[1] * 100.0);
    int16_t accelerometer_Z = round(accData.values[2] * 100.0);
    accCharacteristic.writeValue(accData.bytes, sizeof accData.bytes);
    accData.updated = false;
  }

  if (gyroData.updated) {
    // Bluetooth does not define gyroscope
    // Units in dps
    int16_t gyro_X = round(gyroData.values[0] * 100.0);
    int16_t gyro_Y = round(gyroData.values[1] * 100.0);
    int16_t gyro_Z = round(gyroData.values[2] * 100.0);
    gyroCharacteristic.writeValue(gyroData.bytes, sizeof gyroData.bytes);
    gyroData.updated = false;
  }

  if (magData.updated) {
    // Bluetooth does not define magnetometer
    // Units in uT
    int16_t mag_X = round(magData.values[0] * 100.0);
    int16_t mag_Y = round(magData.values[1] * 100.0);
    int16_t mag_Z = round(magData.values[2] * 100.0);
    magCharacteristic.writeValue(magData.bytes, sizeof magData.bytes);
    magData.updated = false;
  }

    if (envData.updated)
  {
    // Unit is in degrees Celsius with a resolution of 0.01 degrees Celsius
    int16_t temperature = round(envData.temperatureValue * 100.0);
    temperatureCharacteristic.writeValue(temperature);

    // Unit is in percent with a resolution of 0.01 percent
    uint16_t humidity = round(envData.humidityValue * 100.0);
    humidityCharacteristic.writeValue(humidity);

    // Unit is in Pascal with a resolution of 0.1 Pa
    uint32_t pressure = round(envData.pressureValue * 10.0);
    pressureCharacteristic.writeValue(pressure);
    envData.updated = false;
  }

  if (qData.updated) {
    // Bluetooth does not define quaternions
    // Units in complex numbers
    int16_t qX = round(qData.values[0] * 100.0);
    int16_t qY = round(qData.values[1] * 100.0);
    int16_t qZ = round(qData.values[2] * 100.0);
    int16_t qW = round(qData.values[3] * 100.0);
    qCharacteristic.writeValue(qData.bytes, sizeof qData.bytes);
    qData.updated = false;
  }
}

//----------------------------------------------------------------------------------------------------------------------
// PRINT TASKS
/*
 * Print tasks per sensor type.
 * Useful to test accuracy of sensor data before sending over BLE.
 */
//----------------------------------------------------------------------------------------------------------------------

void accPrintTask() {
  Serial.print("AccX = ");
  Serial.print(accData.values[0], 10);
  Serial.println(" G");

  Serial.print("AccY = ");
  Serial.print(accData.values[1], 10);
  Serial.println(" G");

  Serial.print("AccZ = ");
  Serial.print(accData.values[2], 10);
  Serial.println(" G");

  //Serial.print("Acc. Subscription Status: ");
  //Serial.println(accCharacteristic.subscribed());
}

void gyroPrintTask() {
  Serial.print("gyroX = ");
  Serial.print(gyroData.values[0], 10);
  Serial.println(" dps");

  Serial.print("gyroY = ");
  Serial.print(gyroData.values[1], 10);
  Serial.println(" dps");

  Serial.print("gyroZ = ");
  Serial.print(gyroData.values[2], 10);
  Serial.println(" dps");

  //Serial.print("Gyro. Subscription Status: ");
  //Serial.println(gyroCharacteristic.subscribed());
}

void magPrintTask() {
  Serial.print("magX = ");
  Serial.print(magData.values[0], 10);
  Serial.println(" uT");

  Serial.print("magY = ");
  Serial.print(magData.values[1], 10);
  Serial.println(" uT");

  Serial.print("magZ = ");
  Serial.print(magData.values[2], 10);
  Serial.println(" uT");

  //Serial.print("Mag. Subscription Status: ");
  //Serial.println(magCharacteristic.subscribed());
}

void envPrintTask()
{
  Serial.print("Temperature = ");
  Serial.print(envData.temperatureValue);
  Serial.println( " °C" );

  Serial.print("Humidity    = ");
  Serial.print(envData.humidityValue);
  Serial.println(" %");

  Serial.print("Pressure = ");
  Serial.print(envData.pressureValue);
  Serial.println( " Pa" );
}

void qPrintTask() 
{
  Serial.print("qX = ");
  Serial.println(qData.values[0], 10);

  Serial.print("qY = ");
  Serial.println(qData.values[1], 10);

  Serial.print("qZ = ");
  Serial.println(qData.values[2], 10);

  Serial.print("qW = ");
  Serial.println(qData.values[3], 10);

}

//----------------------------------------------------------------------------------------------------------------------
// Event Handlers & Initializers
/*
 * These are handlers that inform connection status
 * Useful when testing, might be removed later on.
 */
//----------------------------------------------------------------------------------------------------------------------

void blePeripheralConnectHandler(BLEDevice central) {
  digitalWrite(BLE_LED_PIN, HIGH);
  Serial.print(F( "Connected to central: " ));
  Serial.println(central.address());
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  digitalWrite(BLE_LED_PIN, LOW);
  Serial.print(F("Disconnected from central: "));
  Serial.println(central.address());
}

void initialize() {
  if (!BHY2.begin()) {
    Serial.println("Failed to initialize accelerometer!");
    while (1);
  }
  if (!accelerometer.begin()) {
    Serial.println("Failed to initialize accelerometer!");
    while (1);
  }
  if (!gyro.begin()) {
    Serial.println("Failed to initialize gyroscope!");
    while (1);
  }
  if (!mag.begin()) {
    Serial.println("Failed to initialize magnetometer!");
    while (1);
  }
  if (!temperature.begin()) {
    Serial.println("Failed to initialize temperature!");
    while (1);
  }
  if (!humidity.begin()) {
    Serial.println("Failed to initialize humidity!");
    while (1);
  }
  if (!pressure.begin()) {
    Serial.println("Failed to initialize pressure!");
    while (1);
  }
  if (!rotation.begin()) {
    Serial.println("Failed to initialize rotation!");
    rotation.configure(10, 1);
    while (1);
  }
  if (!setupBleMode()) {
    while (1);
  } else {
    Serial.println("BLE initialized. Waiting for clients to connect.");
  }
  
  // write initial values
  for (int i = 0; i < 3; i++) 
  {
    accData.values[i] = i;
    gyroData.values[i] = i;
    magData.values[i] = i;
  }
  temp = 0.0; hum = 0.0; pres = 0.0;
  for (int j = 0; j < 4; j++) 
  {
    qData.values[j] = j;
  }
}
