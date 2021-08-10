/*
  The yellow LED shows the BLE module is connected to a central.

  The circuit:
  - Arduino Nano 33 BLE Sense

  You can use a generic BLE central app, like BLE Scanner (iOS and Android) or
  nRF Connect, to interact with the services and characteristics
  created in this sketch.

  This example code is in the public domain.

  This code is provided by @Klaus_K from Arduino community.
*/

#include <ArduinoBLE.h>
#include <Arduino_HTS221.h>
#include <Arduino_LPS22HB.h>

//----------------------------------------------------------------------------------------------------------------------
// BLE UUIDs
//----------------------------------------------------------------------------------------------------------------------

// https://www.bluetooth.com/specifications/assigned-numbers/environmental-sensing-service-characteristics/
// https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.temperature.xml
// https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.humidity.xml
// https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Characteristics/org.bluetooth.characteristic.pressure.xml

#define BLE_UUID_ENVIRONMENTAL_SENSING_SERVICE    "181A"
#define BLE_UUID_TEMPERATURE                      "2A6E"
#define BLE_UUID_HUMIDITY                         "2A6F"
#define BLE_UUID_PRESSURE                         "2A6D"

//----------------------------------------------------------------------------------------------------------------------
// BLE
//----------------------------------------------------------------------------------------------------------------------

#define BLE_DEVICE_NAME                           "Nano"
#define BLE_LOCAL_NAME                            "Nano"

BLEService environmentalSensingService( BLE_UUID_ENVIRONMENTAL_SENSING_SERVICE );
BLEShortCharacteristic temperatureCharacteristic( BLE_UUID_TEMPERATURE, BLERead | BLENotify );
BLEUnsignedShortCharacteristic humidityCharacteristic( BLE_UUID_HUMIDITY, BLERead | BLENotify );
BLEUnsignedLongCharacteristic pressureCharacteristic( BLE_UUID_PRESSURE, BLERead | BLENotify );

//----------------------------------------------------------------------------------------------------------------------
// APP & I/O
//----------------------------------------------------------------------------------------------------------------------

#define SENSOR_UPDATE_INTERVAL                    (5000)

typedef struct __attribute__( ( packed ) )
{
  float temperature;
  float humidity;
  float pressure;
  bool updated = false;
} sensor_data_t;

sensor_data_t sensorData;

#define BLE_LED_PIN                               LED_BUILTIN


void setup()
{
  Serial.begin( 9600 );
  while ( !Serial );
  Serial.println( "BLE Example - Environmental Sensing Service (ESS)" );
  pinMode( BLE_LED_PIN, OUTPUT );
  digitalWrite( BLE_LED_PIN, LOW );
  // Without Serial when using USB power bank HTS sensor seems to needs some time for setup
  delay( 10 );
  if ( !HTS.begin() )
  {
    Serial.println( "Failed to initialize humidity temperature sensor!" );
    while ( 1 );
  }
  if ( !BARO.begin() )
  {
    Serial.println( "Failed to initialize pressure sensor!" );
    while ( 1 );
  }
  if ( !setupBleMode() )
  {
    while ( 1 );
  }
  else
  {
    Serial.println( "BLE initialized. Waiting for clients to connect." );
  }
}

void loop()
{
  bleTask();
  if ( sensorTask() )
  {
    printTask();
  }
}

bool sensorTask()
{
  static long previousMillis = 0;
  unsigned long currentMillis = millis();
  if ( currentMillis - previousMillis < SENSOR_UPDATE_INTERVAL )
  {
    return false;
  }
  previousMillis = currentMillis;
  sensorData.temperature = HTS.readTemperature();
  sensorData.humidity = HTS.readHumidity();
  sensorData.pressure = BARO.readPressure() * 1000; // kPa -> Pa
  sensorData.updated = true;
  return sensorData.updated;
}


void printTask()
{
  Serial.print( "Temperature = " );
  Serial.print( sensorData.temperature );
  Serial.println( " °C" );

  Serial.print( "Humidity    = " );
  Serial.print( sensorData.humidity );
  Serial.println( " %" );

  Serial.print( "Pressure = " );
  Serial.print( sensorData.pressure );
  Serial.println( " Pa" );

  Serial.println(temperatureCharacteristic.subscribed());
  //Serial.println(temperatureCharacteristic.descriptorCount());

}

bool setupBleMode()
{
  if ( !BLE.begin() )
  {
    return false;
  }

  // set advertised local name and service UUID
  BLE.setDeviceName( BLE_DEVICE_NAME );
  BLE.setLocalName( BLE_LOCAL_NAME );
  BLE.setAdvertisedService( environmentalSensingService );

  // BLE add characteristics
  environmentalSensingService.addCharacteristic( temperatureCharacteristic );
  environmentalSensingService.addCharacteristic( humidityCharacteristic );
  environmentalSensingService.addCharacteristic( pressureCharacteristic );

  // add service
  BLE.addService( environmentalSensingService );

  // set the initial value for the characeristic
  temperatureCharacteristic.writeValue( 0 );
  humidityCharacteristic.writeValue( 0 );
  pressureCharacteristic.writeValue( 0 );

  // set BLE event handlers
  BLE.setEventHandler( BLEConnected, blePeripheralConnectHandler );
  BLE.setEventHandler( BLEDisconnected, blePeripheralDisconnectHandler );

  // start advertising
  BLE.advertise();

  return true;
}

void bleTask()
{
  const uint32_t BLE_UPDATE_INTERVAL = 10;
  static uint32_t previousMillis = 0;
  uint32_t currentMillis = millis();
  if ( currentMillis - previousMillis >= BLE_UPDATE_INTERVAL )
  {
    previousMillis = currentMillis;
    BLE.poll();
  }

  if ( sensorData.updated )
  {
    // BLE defines Temperature UUID 2A6E Type sint16 ( see XML links )
    // Unit is in degrees Celsius with a resolution of 0.01 degrees Celsius
    int16_t temperature = round( sensorData.temperature * 100.0 );
    temperatureCharacteristic.writeValue( temperature );

    // BLE defines Humidity UUID 2A6F Type uint16
    // Unit is in percent with a resolution of 0.01 percent
    uint16_t humidity = round( sensorData.humidity * 100.0 );
    humidityCharacteristic.writeValue( humidity );

    // BLE defines Pressure UUID 2A6D Type uint32
    // Unit is in Pascal with a resolution of 0.1 Pa
    uint32_t pressure = round( sensorData.pressure * 10.0 );
    pressureCharacteristic.writeValue( pressure );
    sensorData.updated = false;
  }
}

void blePeripheralConnectHandler( BLEDevice central )
{
  digitalWrite( BLE_LED_PIN, HIGH );
  Serial.print( F ( "Connected to central: " ) );
  Serial.println( central.address() );
}

void blePeripheralDisconnectHandler( BLEDevice central )
{
  digitalWrite( BLE_LED_PIN, LOW );
  Serial.print( F( "Disconnected from central: " ) );
  Serial.println( central.address() );
}
