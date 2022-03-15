/*********************************************************************

*********************************************************************/

#include <bluefruit.h>
#include <Adafruit_LSM6DS33.h>
#include <Adafruit_LIS3MDL.h>
#include <Adafruit_Sensor.h>

//----------------------------------------------------------------------------------------------------------------------
// BLE UUIDS
//----------------------------------------------------------------------------------------------------------------------

#define BLE_UUID_IMU_SERVICE              "5543e0d651ca11ecbf630242ac130002"
#define BLE_UUID_ACC_CHAR                 "5543e32e51ca11ecbf630242ac130002"
#define BLE_UUID_GYRO_CHAR                "5543e55451ca11ecbf630242ac130002"
#define BLE_UUID_MAG_CHAR                 "5543e64451ca11ecbf630242ac130002"

//----------------------------------------------------------------------------------------------------------------------
// Data Pins
//----------------------------------------------------------------------------------------------------------------------

// For SPI mode, we need a CS pin
#define LSM_CS 10
// For software-SPI mode we need SCK/MOSI/MISO pins
#define LSM_SCK 13
#define LSM_MISO 12
#define LSM_MOSI 11

Adafruit_LSM6DS33 lsm6ds33;
Adafruit_LIS3MDL lis3mdl;

BLEService        imu = BLEService(BLE_UUID_IMU_SERVICE);
BLECharacteristic acc = BLECharacteristic(BLE_UUID_ACC_CHAR);
BLECharacteristic gyroscope = BLECharacteristic(BLE_UUID_GYRO_CHAR);
BLECharacteristic magno = BLECharacteristic(BLE_UUID_MAG_CHAR);

BLEDis bledis;    // DIS (Device Information Service) helper class instance
BLEBas blebas;    // BAS (Battery Service) helper class instance

void setup(void)
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb
  setAccGyro();
  setMag();
  Serial.println("Bluefruit52 HRM Example");
  Serial.println("-----------------------\n");

  // Initialise the Bluefruit module
  Serial.println("Initialise the Bluefruit nRF52 module");
  Bluefruit.begin();

  // Set the connect/disconnect callback handlers
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start the Device Information Service
  Serial.println("Configuring the Device Information Service");
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Start the BLE Battery Service and set it to 100%
  Serial.println("Configuring the Battery Service");
  blebas.begin();
  blebas.write(100);

  // Setup the Heart Rate Monitor service using
  // BLEService and BLECharacteristic classes
  Serial.println("Configuring the IMU Service");
  setupIMU();

  // Setup the advertising packet(s)
  Serial.println("Setting up the advertising payload(s)");
  startAdv();

  Serial.println("Ready Player One!!!");
  Serial.println("\nAdvertising");

}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include HRM Service UUID
  Bluefruit.Advertising.addService(imu);

  // Include Name
  Bluefruit.Advertising.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

/**************************************************************************/
/*!
    @brief  Constantly poll for new command or response data
*/
/**************************************************************************/

void setupIMU(void)
{
  imu.begin(); // start the service
  acc.setProperties(CHR_PROPS_NOTIFY); // sets characteristics of this service
  acc.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  acc.setFixedLen(20); // number of bytes of the characteristic, 20 bytes is the limit.
  acc.setCccdWriteCallback(cccd_callback);  // Optionally capture CCCD updates
  acc.begin();
  gyroscope.setProperties(CHR_PROPS_NOTIFY); 
  gyroscope.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  gyroscope.setFixedLen(20); 
  gyroscope.setCccdWriteCallback(cccd_callback);
  gyroscope.begin();
  magno.setProperties(CHR_PROPS_NOTIFY); 
  magno.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  magno.setFixedLen(20); 
  magno.setCccdWriteCallback(cccd_callback);
  magno.begin();
}

//----------------------------------------------------------------------------------------------------------------------
// Callbacks
//----------------------------------------------------------------------------------------------------------------------

void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  Serial.print("Connected to ");
  Serial.println(central_name);
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  Serial.println("Advertising!");
}

void cccd_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint16_t cccd_value)
{
  // Display the raw request packet
  Serial.print("CCCD Updated: ");
  //Serial.printBuffer(request->data, request->len);
  Serial.print(cccd_value);
  Serial.println("");
  // Check the characteristic this CCCD update is associated with in case
  // this handler is used for multiple CCCD records.
  if (chr->uuid == acc.uuid) {
    if (chr->notifyEnabled(conn_hdl)) {
      Serial.println("Acc. 'Notify' enabled");
    } else {
        Serial.println("Acc. 'Notify' disabled"); //systemctl disable --now hciuart
      }
  }
  if (chr->uuid == gyroscope.uuid) {
    if (chr->notifyEnabled(conn_hdl)) {
      Serial.println("Gyro. 'Notify' enabled");
    } else {
        Serial.println("Gyro. 'Notify' disabled"); //systemctl disable --now hciuart
      }
  }
  if (chr->uuid == magno.uuid) {
    if (chr->notifyEnabled(conn_hdl)) {
      Serial.println("Mag. 'Notify' enabled");
    } else {
        Serial.println("Mag. 'Notify' disabled"); //systemctl disable --now hciuart
      }
  }
}

//----------------------------------------------------------------------------------------------------------------------
// Loop
//----------------------------------------------------------------------------------------------------------------------
void loop(void)
{
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;
  lsm6ds33.getEvent(&accel, &gyro, &temp);
  sensors_event_t magEvent; 
  lis3mdl.getEvent(&magEvent);
  /*
  * Create string from accelerometer data 
  * Order is X, Y, Z.
  * Unit is m/s^2.
  */
  float accelerometerData[3] = { accel.acceleration.x, accel.acceleration.y, accel.acceleration.z };
  char dataAccArray[20] = "";
  int accVal1, accVal2, accVal3;
  int accDec1, accDec2, accDec3;
  accVal1 = (int) accelerometerData[0];
  accVal2 = (int) accelerometerData[1];
  accVal3 = (int) accelerometerData[2];
  accDec1 = (abs(accelerometerData[0]) - abs(accVal1)) * 10000;
  accDec2 = (abs(accelerometerData[1]) - abs(accVal2)) * 10000;
  accDec3 = (abs(accelerometerData[2]) - abs(accVal3)) * 10000;
  sprintf (dataAccArray, "%d.%d,%d.%d,%d.%d", accVal1, accDec1, accVal2, accDec2, accVal3, accDec3);
  /*
  * Create string from gyroscope data. 
  * Order is X, Y, Z.
  * We need to convert from rad/s to dps and set offsets.
  */
  float gx, gy, gz;
  gx = (gyro.gyro.x) * SENSORS_RADS_TO_DPS - 4.220581807; // 
  gy = (gyro.gyro.y) * SENSORS_RADS_TO_DPS + 2.606835148; // DONT FORGET to change these offsets!
  gz = (gyro.gyro.z) * SENSORS_RADS_TO_DPS + 4.147467841; //
  float gyroData[3] = { gx, gy, gz };
  char dataGyroArray[20] = "";
  int gyroVal1, gyroVal2, gyroVal3;
  int gyroDec1, gyroDec2, gyroDec3;
  gyroVal1 = (int) gyroData[0];
  gyroVal2 = (int) gyroData[1];
  gyroVal3 = (int) gyroData[2];
  gyroDec1 = (abs(gyroData[0]) - abs(gyroVal1)) * 10000;
  gyroDec2 = (abs(gyroData[1]) - abs(gyroVal2)) * 10000;
  gyroDec3 = (abs(gyroData[2]) - abs(gyroVal3)) * 10000;
  sprintf (dataGyroArray, "%d.%d,%d.%d,%d.%d", gyroVal1, gyroDec1, gyroVal2, gyroDec2, gyroVal3, gyroDec3);
   /*
  * Create string from magnetometer data. 
  * Order is X, Y, Z.
  * Unit is uT.
  */
  float magData[3] = { magEvent.magnetic.x, magEvent.magnetic.y, magEvent.magnetic.z };
  char dataMagArray[20] = "";
  int magVal1, magVal2, magVal3;
  int magDec1, magDec2, magDec3;
  magVal1 = (int) magData[0];
  magVal2 = (int) magData[1];
  magVal3 = (int) magData[2];
  magDec1 = (abs(magData[0]) - abs(magVal1)) * 1000; //
  magDec2 = (abs(magData[1]) - abs(magVal2)) * 1000; // Need to decrease precision due to 20 byte limit.
  magDec3 = (abs(magData[2]) - abs(magVal3)) * 1000; //
  sprintf (dataMagArray, "%d.%d,%d.%d,%d.%d", magVal1, magDec1, magVal2, magDec2, magVal3, magDec3);

  if ( Bluefruit.connected()) {    
    if ( acc.notify(dataAccArray) && gyroscope.notify(dataGyroArray) && magno.notify(dataMagArray)){
      Serial.println("Notification is sent."); 
    }else{
      Serial.println("ERROR: Notify not set in the CCCD or not connected!");
    }
  }
  // Only send update once per second
  delay(1000);
}

//----------------------------------------------------------------------------------------------------------------------
// SENSOR SETTINGS
//----------------------------------------------------------------------------------------------------------------------
void setMag() {
    // Try to initialize!
  if (! lis3mdl.begin_I2C()) {          // hardware I2C mode, can pass in address & alt Wire
    Serial.println("Failed to find LIS3MDL chip");
    while (1) { delay(10); }
  }
  Serial.println("LSM6DS33 Found!");
  lis3mdl.setPerformanceMode(LIS3MDL_MEDIUMMODE);
  lis3mdl.setOperationMode(LIS3MDL_CONTINUOUSMODE);
  lis3mdl.setDataRate(LIS3MDL_DATARATE_155_HZ);
  lis3mdl.setRange(LIS3MDL_RANGE_4_GAUSS);
  lis3mdl.setIntThreshold(500);
  lis3mdl.configInterrupt(false, false, true, // enable z axis
                          true, // polarity
                          false, // don't latch
                          true); // enabled!
}
void setAccGyro() {
  if (!lsm6ds33.begin_I2C()) {
    Serial.println("Failed to find LSM6DS33 chip");
    while (1) {
      delay(10);
    }
  }
  lsm6ds33.configInt1(false, false, true); // accelerometer DRDY on INT1
  lsm6ds33.configInt2(false, true, false); // gyro DRDY on INT2
}
