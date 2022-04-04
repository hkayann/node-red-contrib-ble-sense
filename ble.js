module.exports = function (RED) {

    /*==========================*/
    // BLE Scanner Node         //
    /*==========================*/
    "use strict";
    //const EventEmitter = require('events');
    require('events').defaultMaxListeners = 50;
    //EventEmitter.defaultMaxListeners = 50;
    const noble = require('@abandonware/noble');

    var vars = {};
    var macArray = [];
    var peripheralArray = [];
    vars.macArray = macArray;
    function BLEScanner(config) {

        RED.nodes.createNode(this, config);
        let node = this;
        node.searchFor = config.searchFor;
        node.output = config.output;

        noble.removeAllListeners('discover');
        noble.removeAllListeners('stateChange');
        noble.removeAllListeners('scanStop');
        noble.removeAllListeners('input');
        noble.removeAllListeners('scanStart');
        noble.removeAllListeners('close');

        // const STOPSCAN = () => {
        //     node.status( { fill: "red", shape: "dot", text: "Scan stopped." } );
        // };
        // noble.on('scanStop', STOPSCAN);

        // const STARTSCAN = () => {
        //     node.status( { fill: "green", shape: "ring", text: "Scanning." } )
        // };
        // noble.on('scanStart', STARTSCAN);

        const STATE = (state) => {
            if (noble.state === "poweredOn") {
                noble.startScanning([], true);
            } else {
                node.error(" The Bluetooth adapter state is:", state);
            }
        };
        // Set event listeners for scanner
        function STARTSCAN() {
            console.log("scanStart", node, node.status);
            node.status({ fill: "green", shape: "ring", text: "Scanning." })
        };
        noble.on('scanStart', function () {
            STARTSCAN();
        });
        function STOPSCAN() {
            console.log("scanStop", node, node.status);
            node.status({ fill: "red", shape: "dot", text: "Scan stopped." });
        };
        noble.on('scanStop', function () {
            STOPSCAN();
        });
        // END
        // Set event listener for peripheral
        const PERIPHERAL = (peripheral) => {
            const ADVERTISEMENT = peripheral.advertisement;
            const MAC = peripheral.address;
            vars.peripheral = peripheral;
            if (macArray.includes(MAC) === false) {
                macArray.push(MAC);
            }
            if (peripheralArray.includes(peripheral) === false) {
                peripheralArray.push(peripheral);
            }
            const OUTPUT = () => {
                switch (node.output) {
                    case 'MAC':
                        return node.send({ payload: MAC });
                    case 'Peripheral':
                        return node.send({ payload: JSON.parse(peripheral) });
                    case 'Data':
                        if (ADVERTISEMENT.serviceData[0]) {
                            if (ADVERTISEMENT.serviceData[0].data) {
                                return node.send({ payload: ADVERTISEMENT.serviceData[0].data });
                            }
                        }
                        break;
                };
            };
            if (node.searchFor === "") {
                OUTPUT();
            } else if (ADVERTISEMENT.localName) {
                const LOCALNAME = ADVERTISEMENT.localName.toLowerCase();
                const GIVENNAME = node.searchFor.toLowerCase();
                if (LOCALNAME == GIVENNAME) {
                    OUTPUT();
                }
            }
        };
        noble.on('discover', PERIPHERAL);
        // END
        // Set event listener for scanner input
        const INPUT = (msg, send, done) => {
            let input = msg.topic.toLowerCase()
            if (noble.state === 'poweredOn' && input === "start") {
                noble.startScanning([], true);
                done();
            } else if (input === "start") {
                noble.on('stateChange', STATE);
                done();
            } else if (input === "stop") {
                noble.stopScanning();
                done();
            } else {
                send(new Error("Unknown input."));
            }
        };
        node.on('input', INPUT);
        // END
        // Set event listener on close
        const CLOSE = () => {
            noble.stopScanning();
            noble.removeAllListeners('discover');
            noble.removeAllListeners('stateChange');
            noble.removeAllListeners('scanStop');
            noble.removeAllListeners('input');
            noble.removeAllListeners('scanStart');
            node.status({});
        };
        noble.on('close', CLOSE);
        // END
    }
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

        const bufferChecker = Buffer.from([0, 0]);
        const notifySetter = Buffer.from([1, 0]);
        const decimalSetter = [0.01, 0.1];
        // Get and check services and characteristics input
        try {
            var inputObject = JSON.parse(node.subscribe);
        }
        catch (e) {
            node.error("Unparseable JSON Response.", e);
        }
        var inputKeys = Object.keys(inputObject);
        if (inputKeys[0] === "services" && inputKeys[1] === "characteristics") {
            var serviceValues = Object.values(inputObject)[0];
            var characteristicValues = Object.values(inputObject)[1];
        } else {
            node.send(new Error(`Key names should be "services" and "characteristics".`));
        }
        // END
        // Create initial parameters for data
        let environmentalData = {};
        environmentalData.payload = {};

        let arduinoData = {};
        arduinoData.payload = {
            "accX": 0,
            "accY": 0,
            "accZ": 0,
            "gyroX": 0,
            "gyroY": 0,
            "gyroZ": 0,
            "magX": 0,
            "magY": 0,
            "magZ": 0
        };

        let adafruitData = {};
        adafruitData.payload = {
            "ada_accX": 0,
            "ada_accY": 0,
            "ada_accZ": 0,
            "ada_gyroX": 0,
            "ada_gyroY": 0,
            "ada_gyroZ": 0,
            "ada_magX": 0,
            "ada_magY": 0,
            "ada_magZ": 0
        };
        
        let quaternionData = {};
        quaternionData.payload = {
            "qX": 0,
            "qY": 0,
            "qZ": 0,
            "qW" :0
        };

        let index;
        let counterTemp = 0;
        let counterHum = 0;
        let counterPres = 0;
        let counterArduinoAcc = 0;
        let counterArduinoGyro = 0;
        let counterArduinoMag = 0;
        let counterAdafruitAcc = 0;
        let counterAdafruitGyro = 0;
        let counterAdafruitMag = 0;
        let characteristicNumber = 0;
        // END
        // Set listener for BLE Connect input
        const INPUT = async (msg, send, done) => {

            let input = msg.topic.toLowerCase()
            vars.input = input;

            if (vars.peripheral === undefined) {
                send(new Error("Peripheral object is null."));
            } else if (vars.macArray.includes(input)) {
                index = macArray.indexOf(input);
                try {
                    let e = await noble.stopScanningAsync()
                    send(e)
                } catch (err) {
                    done(err)
                };
                try {
                    let e = await peripheralArray[index].connectAsync()
                    node.status({ fill: "green", shape: "ring", text: "Connected." });
                    send(e)
                } catch (err) {
                    done(err)
                };
                // discover all services and characteristics
                try {
                    var ALL = await peripheralArray[index].discoverSomeServicesAndCharacteristicsAsync(serviceValues, characteristicValues);
                    send(ALL)
                } catch (err) {
                    done(err)
                }
                // how many characteristics discovered
                characteristicNumber = Object.keys(ALL.characteristics).length;
                node.log(ALL.characteristics);
                node.log('Characteristics count: ' + characteristicNumber);
                for (const [key, character] of Object.entries(ALL.characteristics)) {
                    // Check the notify bit, if not set, set it. //
                    if (character.properties.includes("notify")) {
                        const descriptors = await character.discoverDescriptorsAsync();
                        for (const [key, descriptor] of Object.entries(descriptors)) {
                            node.log(descriptor);
                            let descriptorData = await descriptor.readValueAsync();
                            if (descriptorData[0] === bufferChecker[0] || descriptorData[1] === bufferChecker[1]) {
                                node.log(`The ${character.name} ${character.uuid} notify bit is disabled.`);
                                node.log("Enabling notification bit...");
                                descriptor.writeValueAsync(notifySetter);
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

                for (const [key, character] of Object.entries(ALL.characteristics)) {
                    character.on('data', (data) => {
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
                        } else if (character.uuid === '5543e32e51ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            arduinoData.payload['accX'] = dataGroupedFloat[0];
                            arduinoData.payload['accY'] = dataGroupedFloat[1];
                            arduinoData.payload['accZ'] = dataGroupedFloat[2];
                            counterArduinoAcc++;
                        } else if (character.uuid === '5543e32e51ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitData.payload['ada_accX'] = parseFloat(dataArray[0]);
                            adafruitData.payload['ada_accY'] = parseFloat(dataArray[1]);
                            adafruitData.payload['ada_accZ'] = parseFloat(dataArray[2]);
                            counterAdafruitAcc++;
                        } else if (character.uuid === '5543e55451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            arduinoData.payload['gyroX'] = dataGroupedFloat[0];
                            arduinoData.payload['gyroY'] = dataGroupedFloat[1];
                            arduinoData.payload['gyroZ'] = dataGroupedFloat[2];
                            counterArduinoGyro++;
                        } else if (character.uuid === '5543e55451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitData.payload['ada_gyroX'] = parseFloat(dataArray[0]);
                            adafruitData.payload['ada_gyroY'] = parseFloat(dataArray[1]);
                            adafruitData.payload['ada_gyroZ'] = parseFloat(dataArray[2]);
                            counterAdafruitGyro++;
                        } else if (character.uuid === '5543e64451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Arduino') {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            arduinoData.payload['magX'] = dataGroupedFloat[0];
                            arduinoData.payload['magY'] = dataGroupedFloat[1];
                            arduinoData.payload['magZ'] = dataGroupedFloat[2];
                            counterArduinoMag++;
                        } else if (character.uuid === '5543e64451ca11ecbf630242ac130002' && data !== undefined && node.micro == 'Adafruit') {
                            let dataString = Buffer.from(data).toString();
                            let dataArray = dataString.split(',');
                            adafruitData.payload['ada_magX'] = parseFloat(dataArray[0]);
                            adafruitData.payload['ada_magY'] = parseFloat(dataArray[1]);
                            adafruitData.payload['ada_magZ'] = parseFloat(dataArray[2]);
                            counterAdafruitMag++;
                        } else if (character.uuid === 'a2278362b3fe11ecb9090242ac120002' && data !== undefined) {
                            let dataGrouped = splitToChunks(data.toJSON().data, 4);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
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
                            send(arduinoData);
                            done();
                        };
                        if ((counterAdafruitAcc + counterAdafruitGyro + counterAdafruitMag) % 3 == 0 && (counterAdafruitAcc + counterAdafruitGyro + counterAdafruitMag) !== 0) {
                            send(adafruitData);
                            done();
                        };
                    });
                    // Character data event listener END //
                }
                done();
            } else if (input === "disconnect") {
                try {
                    let e = await peripheralArray[index].disconnectAsync()
                    node.status({ fill: "red", shape: "dot", text: "Disconnected." });
                    send(e)
                    done();
                } catch (err) {
                    done(err)
                };
            } else {
                send(new Error("Unknown input."));
            }
        };
        node.on('input', INPUT);
        // END
        // Set listener on close
        const CLOSE = () => {
            noble.removeAllListeners('input');
            noble.removeAllListeners('data');
            node.status({});
        };
        noble.on('close', CLOSE);
        // END
        // Functions convert received buffer to float.
        function splitToChunks(array, parts) {
            let result = [];
            for (let i = parts; i > 0; i--) {
                result.push(array.splice(0, Math.ceil(array.length / i)));
            }
            return result;
        }
        function bufferToFloat(data) {
            let result = [];
            for (let i = 0; i < data.length; i++) {
                let buffer = Buffer.from(data[i]).readFloatLE(0);
                result.push(buffer);
            }
            return result;
        }
    }
    RED.nodes.registerType("BLE Connect", BLEConnect);
}
