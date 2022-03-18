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

#define BLE_UUID_IMU_SERVICE              "5543e0d651ca11ecbf630242ac130002"
#define BLE_UUID_ACC_CHAR                 "5543e32e51ca11ecbf630242ac130002"
#define BLE_UUID_GYRO_CHAR                "5543e55451ca11ecbf630242ac130002"
#define BLE_UUID_MAG_CHAR                 "5543e64451ca11ecbf630242ac130002"

//----------------------------------------------------------------------------------------------------------------------
// APP & I/O
//----------------------------------------------------------------------------------------------------------------------

//#define NUMBER_OF_SENSORS 3

#define ACC_SENSOR_UPDATE_INTERVAL                (500) // node-red-dashboard can't handle 1 ms, can handle 100 ms.
#define MAG_SENSOR_UPDATE_INTERVAL                (500)
#define GYRO_SENSOR_UPDATE_INTERVAL               (500)

union sensor_data {
  struct __attribute__((packed)) {
    float values[3]; // float array for data (it holds 3)
    bool updated = false;
  };
  uint8_t bytes[3 * sizeof(float)]; // size as byte array 
};

union sensor_data accData;
union sensor_data gyroData;
union sensor_data magData;

SensorXYZ accelerometer(SENSOR_ID_ACC);
SensorXYZ gyro(SENSOR_ID_GYRO);
SensorXYZ mag(SENSOR_ID_MAG);

float accX, accY, accZ, gyroX, gyroY, gyroZ, magX, magY, magZ;

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
  
  // write initial value
  for (int i = 0; i < 3; i++) {
    accData.values[i] = i;
    gyroData.values[i] = i;
    magData.values[i] = i;
  }
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
  if (currentMillis2 - previousMillis2 < ACC_SENSOR_UPDATE_INTERVAL) {
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
  if (currentMillis2 - previousMillis2 < GYRO_SENSOR_UPDATE_INTERVAL) {
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
  if (currentMillis3 - previousMillis3 < MAG_SENSOR_UPDATE_INTERVAL) {
    return false;
  }
  previousMillis3 = currentMillis3;
  // get raw mag
  magX = mag.x();
  magY = mag.y();
  magZ = mag.z();
  //convert to uT
  magX = magX / (65536 / 4000); 
  magY = magY / (65536 / 4000); 
  magZ = magZ / (65536 / 4000);
  // set the values  
  magData.values[0] = magX;
  magData.values[1] = magY;
  magData.values[2] = magZ;
  magData.updated = true;
  
  return magData.updated;
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

  // BLE add characteristics
  IMUService.addCharacteristic(accCharacteristic);
  IMUService.addCharacteristic(gyroCharacteristic);
  IMUService.addCharacteristic(magCharacteristic);

  // add service
  BLE.addService(IMUService);

  // set the initial value for the characteristic:
  accCharacteristic.writeValue(accData.bytes, sizeof accData.bytes);
  gyroCharacteristic.writeValue(gyroData.bytes, sizeof gyroData.bytes);
  magCharacteristic.writeValue(magData.bytes, sizeof magData.bytes);

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

  Serial.print("Acc. Subscription Status: ");
  Serial.println(accCharacteristic.subscribed());
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

  Serial.print("Gyro. Subscription Status: ");
  Serial.println(gyroCharacteristic.subscribed());
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

  Serial.print("Mag. Subscription Status: ");
  Serial.println(magCharacteristic.subscribed());
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
  if (!setupBleMode()) {
    while (1);
  } else {
    Serial.println("BLE initialized. Waiting for clients to connect.");
  }
}
