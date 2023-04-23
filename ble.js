module.exports = function (RED) {

    "use strict";
    //const EventEmitter = require('events');
    require('events').defaultMaxListeners = 50;
    //EventEmitter.defaultMaxListeners = 50;
    const noble = require('@abandonware/noble');

    // Dictionary for all discovered event objects.
    let vars = {};
    // Array to hold MAC addresses.
    let macArray = [];
    // Array to hold discovered peripherals.
    let peripheralArray = [];

    vars.macArray = macArray;

    /*==========================*/
    // BLE Scanner Node         //
    /*==========================*/
    function BLEScanner(config) {

        RED.nodes.createNode(this, config);
        const node = this;
        node.searchFor = config.searchFor;
        node.output = config.output;

        // Event listener to check BLE power state.
        const STATE = (state) => {
            if (noble.state == "poweredOn") {
                node.log("The Bluetooth adapter state is poweredOn.");
            } else {
                node.error("The Bluetooth adapter state is:", state);
            }
        };
        noble.on('stateChange', STATE);

        // Event listener to check if scanning stopped.
        const STOPSCAN = () => {
            node.status({ fill: "red", shape: "dot", text: "Scan stopped." });
        };
        noble.on('scanStop', STOPSCAN);
        // Event listener to check if scanning started.
        const STARTSCAN = () => {
            node.status({ fill: "green", shape: "ring", text: "Scanning." });
        };
        noble.on('scanStart', STARTSCAN);

        // Set event listener for discovered peripherals.
        const PERIPHERAL = (peripheral) => {

            const ADVERTISEMENT = peripheral.advertisement;
            const MAC = peripheral.address;
            vars.peripheral = peripheral;
            
            // Add found objects to arrays.
            if (!macArray.includes(MAC)) {
                macArray.push(MAC);
            }
            if (!peripheralArray.includes(peripheral)) {
                peripheralArray.push(peripheral);
            }

            // We output based on users node config.
            const OUTPUT = () => {
                switch (node.output) {
                    // We either output MAC address.
                    case 'MAC':
                        return node.send({ payload: MAC });
                    // or the whole peripheral object.
                    case 'Peripheral':
                        return node.send({ payload: JSON.parse(peripheral) });
                    // or the advertised data. 
                    case 'Data':
                        return node.send({ payload: ADVERTISEMENT.serviceData[0].data });
                };
            };

            // If no search parameter given, output all.
            if (!node.searchFor) {
                OUTPUT();
            // If given, match the names.    
            } else if (ADVERTISEMENT.localName) {
                const LOCALNAME = ADVERTISEMENT.localName.toLowerCase();
                const GIVENNAME = node.searchFor.toLowerCase();
                if (LOCALNAME == GIVENNAME) {
                    OUTPUT();
                }
            }
        };
        noble.on('discover', PERIPHERAL);

        // Set event listener for scanner input.
        const INPUT_SCAN = (msg, send, done) => {
            let inputMessage = msg.topic.toLowerCase()
            if (noble.state === 'poweredOn' && inputMessage === "start") {
                noble.startScanning([], true);
                done();
            } else if (inputMessage === "start") {
                send(new Error("BLE is powered on."));
                done();
            } else if (inputMessage === "stop") {
                noble.stopScanning();
                done();
            } else {
                send(new Error("Unknown input."));
            }
        };
        node.on('input', INPUT_SCAN);

        // Set event listener on close
        const CLOSE = () => {
            noble.stopScanning();
            noble.removeAllListeners('discover');
            noble.removeAllListeners('stateChange');
            noble.removeAllListeners('input');
            noble.removeAllListeners('scanStart');
            noble.removeAllListeners('scanStop');
            noble.removeAllListeners('close');
        };
        node.on('close', CLOSE);
    }
    // Register the node.
    RED.nodes.registerType("BLE Scanner", BLEScanner);

    /*==========================*/
    // BLE Connect Node         
    /*==========================*/

    function BLEConnect(config) {

        RED.nodes.createNode(this, config);
        let node = this;
        node.config = config;
        node.subscribe = config.subscribe;
        node.micro = config.micro;

        const BUFFERCHECKER = Buffer.from([0, 0]);
        const NOTIFYSETTER = Buffer.from([1, 0]);
        const decimalSetter = [0.01, 0.1];

        // This approach is not good, will update later on.
        // It would be better to have separate node textbox for each.
        // Get services and characteristics node subscribe input.
        try {
            var inputObject = JSON.parse(node.subscribe);
        }
        catch (e) {
            node.error("Unparseable JSON Response.", e);
        }
        let inputKeys = Object.keys(inputObject);
        if (inputKeys[0] === "services" && inputKeys[1] === "characteristics") {
            var serviceValues = Object.values(inputObject)[0];
            var characteristicValues = Object.values(inputObject)[1];
        } else {
            node.send(new Error(`Key names should be "services" and "characteristics".`));
        }
        // END of the not good approach.

        // Initialize environmental data.
        let environmentalData = {};
        environmentalData.payload = {};

        // Initialize parameters for arduino IMU.
        let arduinoIMU = {
            payload: {
                "accX": 0,
                "accY": 0,
                "accZ": 0,
                "gyroX": 0,
                "gyroY": 0,
                "gyroZ": 0,
                "magX": 0,
                "magY": 0,
                "magZ": 0
            }
        };
        // Initialize parameters for adafruit IMU.
        let adafruitIMU = {
            payload: {
              "ada_accX": 0,
              "ada_accY": 0,
              "ada_accZ": 0,
              "ada_gyroX": 0,
              "ada_gyroY": 0,
              "ada_gyroZ": 0,
              "ada_magX": 0,
              "ada_magY": 0,
              "ada_magZ": 0
            }
          };
        // Initialize parameters for quaternions.
        let quaternionData = { 
            payload: { 
                "qX": 0, 
                "qY": 0, 
                "qZ": 0, 
                "qW": 0 } 
            };

        // Initializes counter used for outputting modality at once.
        let [index, characteristicNumber] = [0, 0];
        let [counterTemp, counterHum, counterPres] = [0, 0, 0];
        let [counterArduinoAcc, counterArduinoGyro, counterArduinoMag] = [0, 0, 0];
        let [counterAdafruitAcc, counterAdafruitGyro, counterAdafruitMag] = [0, 0, 0];
        
        // Set listener for BLE Connect input.
        // Input is either MAC or disconnect.
        const INPUT = async (msg, send, done) => {

            let input = msg.topic.toLowerCase()
            vars.input = input;

            // Check if peripheral objects present.
            if (vars.peripheral === undefined) {
                // Log error if nothing discovered yet.
                send(new Error("Peripheral object is null."));
            // Check if given MAC is discovered.    
            } else if (vars.macArray.includes(input)) {
                // Get its array index.
                index = macArray.indexOf(input);
                // Stop scanning before initiating connection.
                try {
                    await noble.stopScanningAsync();
                } catch (err) {
                    done(err);
                };
                // Connecting to a corresponding peripheral.
                try {
                    // For some reason connect emits scanStart!
                    await peripheralArray[index].connectAsync()
                    console.log("Connecting to a peripheral...");
                    node.status({ fill: "green", shape: "ring", text: "Connected." });
                    noble.emit('scanStop');
                } catch (err) {
                    done(err);
                };
                // Discover all services and characteristics.
                try {
                    var all = await peripheralArray[index].discoverSomeServicesAndCharacteristicsAsync(serviceValues, characteristicValues);
                    console.log("Discovering services and characteristics...");
                    send(all)
                } catch (err) {
                    done(err)
                }

                // How many characteristics discovered.
                characteristicNumber = Object.keys(all.characteristics).length;
                node.log('Characteristics count: ' + characteristicNumber);
                // Set the notify bit so we can connect.
                for (const [key, character] of Object.entries(all.characteristics)) {
                    // Check the notify bit, if not set, set it. //
                    if (character.properties.includes("notify")) {
                        const DESCRIPTORS = await character.discoverDescriptorsAsync();
                        for (const [key, descriptor] of Object.entries(DESCRIPTORS)) {
                            let descriptorData = await descriptor.readValueAsync();
                            if (descriptorData[0] === BUFFERCHECKER[0] || descriptorData[1] === BUFFERCHECKER[1]) {
                                node.log(`The ${character.name} ${character.uuid} notify bit is disabled.`);
                                node.log("Enabling notification bit...");
                                descriptor.writeValueAsync(NOTIFYSETTER);
                                if (character.name !== null) {
                                    node.log(`Notification for ${character.name} characteristic is enabled.`);
                                } else {
                                    node.log(`Notification for custom characteristic is enabled.`);
                                };
                            } else {
                                node.log(`The ${character.name} ${character.uuid} notify bit is already enabled.`);
                                return;
                            }
                        }
                    } else {
                        node.log(`Notification is not allowed for ${character.name} characteristic.`)
                    }
                };
                // Get data and group based on data type.
                for (const [key, character] of Object.entries(all.characteristics)) {
                    character.on('data', (data) => {
                        // Do for environmental data.
                        if (character.uuid === '2a6d' && data !== undefined) {
                            data = data.readUInt32LE() * decimalSetter[1];
                            environmentalData.payload[character.name] = parseFloat(data.toFixed(2));
                            counterPres++;
                        } else if (character.uuid === '2a6e' && data !== undefined) {
                            data = data.readUInt16LE() * decimalSetter[0];
                            environmentalData.payload[character.name] = parseFloat(data.toFixed(2));
                            counterTemp++;
                        } else if (character.uuid === '2a6f' && data !== undefined) {
                            data = data.readUInt16LE() * decimalSetter[0];
                            environmentalData.payload[character.name] = parseFloat(data.toFixed(2));
                            counterHum++;
                        // Do for IMU data.
                        } else if (character.uuid === '5543e32e51ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = SPLIT_TO_CHUNKS(data.toJSON().data, 3);
                            let dataGroupedFloat = BUFFER_TO_FLOAT(dataGrouped);
                            arduinoIMU.payload['accX'] = dataGroupedFloat[0];
                            arduinoIMU.payload['accY'] = dataGroupedFloat[1];
                            arduinoIMU.payload['accZ'] = dataGroupedFloat[2];
                            counterArduinoAcc++;
                        } else if (character.uuid === '5543e32e51ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitIMU.payload['ada_accX'] = parseFloat(dataArray[0]);
                            adafruitIMU.payload['ada_accY'] = parseFloat(dataArray[1]);
                            adafruitIMU.payload['ada_accZ'] = parseFloat(dataArray[2]);
                            counterAdafruitAcc++;
                        } else if (character.uuid === '5543e55451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = SPLIT_TO_CHUNKS(data.toJSON().data, 3);
                            let dataGroupedFloat = BUFFER_TO_FLOAT(dataGrouped);
                            arduinoIMU.payload['gyroX'] = dataGroupedFloat[0];
                            arduinoIMU.payload['gyroY'] = dataGroupedFloat[1];
                            arduinoIMU.payload['gyroZ'] = dataGroupedFloat[2];
                            counterArduinoGyro++;
                        } else if (character.uuid === '5543e55451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitIMU.payload['ada_gyroX'] = parseFloat(dataArray[0]);
                            adafruitIMU.payload['ada_gyroY'] = parseFloat(dataArray[1]);
                            adafruitIMU.payload['ada_gyroZ'] = parseFloat(dataArray[2]);
                            counterAdafruitGyro++;
                        } else if (character.uuid === '5543e64451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = SPLIT_TO_CHUNKS(data.toJSON().data, 3);
                            let dataGroupedFloat = BUFFER_TO_FLOAT(dataGrouped);
                            arduinoIMU.payload['magX'] = dataGroupedFloat[0];
                            arduinoIMU.payload['magY'] = dataGroupedFloat[1];
                            arduinoIMU.payload['magZ'] = dataGroupedFloat[2];
                            counterArduinoMag++;
                        } else if (character.uuid === '5543e64451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitIMU.payload['ada_magX'] = parseFloat(dataArray[0]);
                            adafruitIMU.payload['ada_magY'] = parseFloat(dataArray[1]);
                            adafruitIMU.payload['ada_magZ'] = parseFloat(dataArray[2]);
                            counterAdafruitMag++;
                        // Do for quaternion data.
                        } else if (character.uuid === 'a2278362b3fe11ecb9090242ac120002' && data !== undefined) {
                            let dataGrouped = SPLIT_TO_CHUNKS(data.toJSON().data, 4);
                            let dataGroupedFloat = BUFFER_TO_FLOAT(dataGrouped);
                            quaternionData.payload['qX'] = dataGroupedFloat[0];
                            quaternionData.payload['qY'] = dataGroupedFloat[1];
                            quaternionData.payload['qZ'] = dataGroupedFloat[2];
                            quaternionData.payload['qW'] = dataGroupedFloat[3];
                            send(quaternionData);
                            done();
                        }
                        
                        // Send data to Node-RED after we read all given characteristics.
                        if ((counterHum + counterPres + counterTemp) % 3 == 0 && (counterHum + counterPres + counterTemp) !== 0) {
                            send(environmentalData);
                            done();
                        };
                        if ((counterArduinoAcc + counterArduinoGyro + counterArduinoMag) % 3 == 0 && (counterArduinoAcc + counterArduinoGyro + counterArduinoMag) !== 0) {
                            send(arduinoIMU);
                            done();
                        };
                        if ((counterAdafruitAcc + counterAdafruitGyro + counterAdafruitMag) % 3 == 0 && (counterAdafruitAcc + counterAdafruitGyro + counterAdafruitMag) !== 0) {
                            send(adafruitIMU);
                            done();
                        };
                    });
                }
                done();
            }
            if (input === "disconnect") {
                try {
                    await peripheralArray[index].disconnectAsync();
                    node.status({ fill: "red", shape: "dot", text: "Disconnected." });
                    done();
                } catch (err) {
                    done(err)
                };
            } else {
                send(new Error("Unknown input."));
            }
        };
        node.on('input', INPUT);
        
        // Functions convert received buffer to float.
        const SPLIT_TO_CHUNKS = (array, parts) => {
            let result = [];
            for (let i = parts; i > 0; i--) {
              result.push(array.splice(0, Math.ceil(array.length / i)));
            }
            return result;
          };
        const BUFFER_TO_FLOAT = (data) => {
            let result = [];
            for (let i = 0; i < data.length; i++) {
                let buffer = Buffer.from(data[i]).readFloatLE(0);
                result.push(buffer);
            }
            return result;
        };
        
    }
    RED.nodes.registerType("BLE Connect", BLEConnect);
}
