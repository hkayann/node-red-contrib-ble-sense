module.exports = function(RED) {

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
 
        const STOPSCAN = () => {
            node.status( { fill: "red", shape: "dot", text: "Scan stopped." } );
        };
        noble.on('scanStop', STOPSCAN);

        const STARTSCAN = () => {
            node.status( { fill: "green", shape: "ring", text: "Scanning." } )
        };
        noble.on('scanStart', STARTSCAN);

        const STATE = (state) => {
            if (noble.state === "poweredOn") {
                noble.startScanning([], true);
            } else{
                node.error(" The Bluetooth adapter state is:", state);
            }
        };

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
                switch (node.output){
                    case 'MAC':
                        return node.send({payload:MAC});
                    case 'Peripheral':
                        return node.send({payload:JSON.parse(peripheral)});
                    case 'Data':
                        if(ADVERTISEMENT.serviceData[0]){
                            if (ADVERTISEMENT.serviceData[0].data) {
                                return node.send({payload:ADVERTISEMENT.serviceData[0].data});
                            }
                        }
                        break; 
                };
            };
            if (node.searchFor === ""){
                OUTPUT();
            } else if (ADVERTISEMENT.localName){
                const LOCALNAME = ADVERTISEMENT.localName.toLowerCase();
                const GIVENNAME = node.searchFor.toLowerCase();
                if (LOCALNAME === GIVENNAME){
                    OUTPUT();
                }
            }
        };
        noble.on('discover', PERIPHERAL);

        const INPUT = (msg, send, done) => {
            let input = msg.topic.toLowerCase()
            if (noble.state === 'poweredOn' && input === "start") {
                noble.startScanning([], true); 
                done();
            } else if ( input === "start"){
                noble.on('stateChange', STATE);
                done();
            } else if ( input === "stop"){
                noble.stopScanning();
                done();
            } else {
                send(new Error("Unknown input."));
            }
        };
        node.on('input', INPUT);

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
    }
    RED.nodes.registerType("BLE Scanner", BLEScanner);

    /*==========================*/
    // BLE Connect Node         
    /*==========================*/

    function BLEConnect(config) {

        RED.nodes.createNode(this,config);
        let node = this;
        node.config = config;
        node.subscribe = config.subscribe;
        
        const bufferChecker = Buffer.from([0, 0]);
        const notifySetter = Buffer.from([1, 0]); 
        const decimalSetter = [0.01, 0.1];
        
        try {
            var inputObject = JSON.parse(node.subscribe); 
        }
        catch(e) {
            node.error("Unparseable JSON Response.", e);
        } 

        var inputKeys = Object.keys(inputObject);
        
        if (inputKeys[0] === "services" && inputKeys[1] === "characteristics") {
            var serviceValues = Object.values(inputObject)[0];
            var characteristicValues = Object.values(inputObject)[1];
        } else {
            node.send(new Error(`Key names should be "services" and "characteristics".`));
        }

        let environmentalData = {};
        environmentalData.payload = {};
        var accData = {};
        var gyroData = {};
        var magData = {};
        accData.payload = {
            "accX": 0,
            "accY": 0,
            "accZ": 0
        };
        gyroData.payload = {
            "gyroX": 0,
            "gyroY": 0,
            "gyroZ": 0
        };
        magData.payload = {
            "magX": 0,
            "magY": 0,
            "magZ": 0
        };
        let index;
        let counterTemp = 0;
        let counterHum = 0;
        let counterPres = 0;
        let counterAccX = 0;
        let counterAccY = 0;
        let counterAccZ = 0;
        let characteristicNumber = 0;
        
        // Start const INPUT
        const INPUT = async (msg, send, done) => {
    
            let input = msg.topic.toLowerCase()
            vars.input = input;

            if (vars.peripheral === undefined) {
                send(new Error("Peripheral object is null."));
            } else if (vars.macArray.includes(input)) {
                index = macArray.indexOf(input);
                await noble.stopScanningAsync().catch(e => send(e));
                await peripheralArray[index].connectAsync().catch(e => send(e)); 
                node.status( { fill: "green", shape: "ring", text: "Connected." } );
                // discover all services and characteristics
                const ALL = await peripheralArray[index].discoverSomeServicesAndCharacteristicsAsync(serviceValues, characteristicValues).catch(e => send(e));
                // how many characteristics discovered
                characteristicNumber = Object.keys(ALL.characteristics).length;
                node.log(ALL.characteristics);
                node.log('Characteristics count: ' + characteristicNumber);
                for (const [key, character] of Object.entries(ALL.characteristics)) {
                    // Check the notify bit, if not set, set it. //

                    if (character.properties.includes("notify")) {
                        const descriptors = await character.discoverDescriptorsAsync().catch(e => send(e));
                        for (const [key, descriptor] of Object.entries(descriptors)) {
                            node.log(descriptor);
                            let descriptorData = await descriptor.readValueAsync().catch(e => send(e));
                            if (descriptorData[0] === bufferChecker[0] || descriptorData[1] === bufferChecker [1]) {
                                node.log(`The ${character.name} ${character.uuid} notify bit is disabled.`);
                                node.log("Enabling notification bit...");
                                descriptor.writeValueAsync(notifySetter).catch(e => send(e));
                                node.log (`Notification for ${character.name} characteristic is enabled.`);
                            } else {
                                node.log(`The ${character.name} ${character.uuid} notify bit is already enabled.`);
                                return;
                            }
                        }
                    } else {
                        node.log(`Notification is not allowed for ${character.name} characteristic.`)
                    }
                }

                for (const [key, character] of Object.entries(ALL.characteristics)) {
                    character.on('data', (data) => {
                        if (character.uuid === '2a6d' && data !== undefined) {
                            data = data.readUInt32LE() * decimalSetter[1];
                            environmentalData.payload[character.name] = data.toFixed(2);
                            counterPres++;
                        } else if (character.uuid === '2a6e' && data !== undefined) {
                            data = data.readUInt16LE() * decimalSetter[0];
                            environmentalData.payload[character.name] = data.toFixed(2);
                            counterTemp++;
                        } else if (character.uuid === '2a6f' && data !== undefined) {
                            data = data.readUInt16LE() * decimalSetter[0];
                            environmentalData.payload[character.name] = data.toFixed(2);
                            counterHum++;
                        } else if (character.uuid === '5543e32e51ca11ecbf630242ac130002' && data !== undefined ) {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            accData.payload['accX'] = dataGroupedFloat[0];
                            accData.payload['accY'] = dataGroupedFloat[1];
                            accData.payload['accZ'] = dataGroupedFloat[2];
                            send(accData);
                            done();
                        } else if (character.uuid === '5543e55451ca11ecbf630242ac130002' && data !== undefined) {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            gyroData.payload['gyroX'] = dataGroupedFloat[0];
                            gyroData.payload['gyroY'] = dataGroupedFloat[1];
                            gyroData.payload['gyroZ'] = dataGroupedFloat[2];
                            send(gyroData);
                            done();
                        } else if (character.uuid === '5543e64451ca11ecbf630242ac130002' && data !== undefined) {
                            let dataGrouped = splitToChunks(data.toJSON().data, 3);
                            let dataGroupedFloat = bufferToFloat(dataGrouped);
                            magData.payload['magX'] = dataGroupedFloat[0];
                            magData.payload['magY'] = dataGroupedFloat[1];
                            magData.payload['magZ'] = dataGroupedFloat[2];
                            send(magData);
                            done();
                        }
                        // Sends Temp., Hum., and Pres. data together.
                        if ( (counterHum + counterPres + counterTemp) % 3 == 0 && (counterHum + counterPres + counterTemp) !== 0){
                            send(environmentalData);
                        }
                      });
                    // Character data event listener END //
                }

                done();
            } else if ( input === "disconnect"){
                await peripheralArray[index].disconnectAsync().catch(e => node.send(e));
                node.status({ fill: "red", shape: "dot", text: "Disconnected." });
                done();
            } else {
                send(new Error("Unknown input."));
            }
        };
        // End of const Input //

        node.on('input', INPUT);

        const CLOSE = () => {
            noble.removeAllListeners('input');
            noble.removeAllListeners('data');
            node.status({});
        };
        noble.on('close', CLOSE);

        /*=================================*/
        // Functions convert received buffer to float.
        /*=================================*/
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
