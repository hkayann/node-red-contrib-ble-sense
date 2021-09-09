module.exports = function(RED) {

    /*==========================*/
    // BLE Scanner Node         //
    /*==========================*/

    "use strict";

    require('events').defaultMaxListeners = 50;
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
    // BLE Connect Node         //
    /*==========================*/

    function BLEConnect(config) {

        RED.nodes.createNode(this,config);
        let node = this;
        node.config = config;
        node.subscribe = config.subscribe;
        
        const bufferChecker = Buffer.from([0, 0]);
        const notifySetter = Buffer.from([1, 0]); 
        const decimalSetter = [0.01];
        
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

        let dataObject = {};
        let index;
        let counterTemp = 0;
        let counterHum = 0;
        let counterPres = 0;
        let total = 0;
        let characteristicNumber = 0;
        
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
                const ALL = await peripheralArray[index].discoverSomeServicesAndCharacteristicsAsync(serviceValues, characteristicValues).catch(e => send(e));
                characteristicNumber = Object.keys(ALL.characteristics).length;
                for (const [key, character] of Object.entries(ALL.characteristics)) {
                    character.on('data', (data) => { 
                        if (character.uuid === '2a6d') {
                            dataObject[character.name] = data.readUInt32LE();
                            counterPres++;
                        } else if (character.uuid === '2a6e') {
                            data = data.readUInt16LE() * decimalSetter[0];
                            dataObject[character.name] = data.toFixed(2);
                            counterTemp++;
                        } else if (character.uuid === '2a6f') {
                            data = data.readUInt16LE() * decimalSetter[0];
                            dataObject[character.name] = data.toFixed(2);
                            counterHum++;
                        } else {
                            data = data.readValue();
                            node.send(data);
                        }
                        total = counterHum + counterPres + counterTemp;
                        if ( total % characteristicNumber == 0){
                            send(dataObject);
                        }
                      });
                    if (character.properties.includes("notify")) {
                        character.discoverDescriptors ( (error, descriptors) => {
                            if (error) {
                                node.log(error);
                            } else {
                                for (const [key, descriptor] of Object.entries(descriptors)) {
                                    descriptor.readValue( (error, data) => {
                                        if(error){
                                            node.log(error);
                                        } else if (data[0] === bufferChecker[0] || data[1] === bufferChecker [1]) {
                                            node.log(`The ${character.name} ${character.uuid} notify bit is disabled.`);
                                            node.log("Enabling notification bit...");
                                            descriptor.writeValue(notifySetter, (error) => {
                                                if (error) {
                                                    node.log(error);
                                                } else {
                                                    node.log (`Notification for ${character.name} characteristic is enabled.`);
                                                }
                                            }); 
                                        } else {
                                            node.log(`The ${character.name} ${character.uuid} notify bit is already enabled.`);
                                            return;
                                        }
                                    });
                                }
                            }
                        });
                    } else {
                        send(`Notification is not allowed for ${character.name} characteristic.`)
                    }
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

        node.on('input', INPUT);

        const CLOSE = () => {
            noble.removeAllListeners('input');
            noble.removeAllListeners('data');
            node.status({});
        };
        noble.on('close', CLOSE);
    }
    RED.nodes.registerType("BLE Connect", BLEConnect);

}
