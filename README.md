# node-red-contrib-ble-sense
Node-RED central module for Bluetooth Low Energy (BLE) devices. Speficially designed to connect microcontrollers such as [Arduino Nano 33 BLE Sense](https://store-usa.arduino.cc/products/arduino-nano-33-ble-sense) but should be working well with other central devices.

Example peripheral codes are provided in **examples** folder.

You **MUST USE SAME UUIDs** that are given in example scripts to convert received data.

`hci0` is accepted as default adapter. Multiple adapter support will be added later.

# Permissions

To run without `sudo` run the following:

```
sudo setcap cap_net_raw+eip $(eval readlink -f `which node`)
```

This grants the `node` binary `cap_net_raw` privileges, so it can start/stop BLE advertising.


# Installation

```
npm install node-red-contrib-ble-sense
```

# Prerequisite

Requires [@abandonware/noble: 1.9.2-20.](https://www.npmjs.com/package/@abandonware/noble).

# Quick Start

The package contains two nodes: BLE Scanner, BLE Connect.

**BLE Scanner** node allows you to scan BLE devices. A message with **start** as a topic starts scanning. The node can output the following:
- Whole peripheral as an object.
- The MAC address of the peripheral.
- The advertisement data as a buffer array.

Hence **BLE Scanner** node can be used as an observer. This node also allows you to configure and search for a specific peripheral via given name.
To stop scanning a message with **stop** topic should be given.

**BLE Connect** node provides direct connection to the peripheral. This node must not be used alone. The scanning should be in progress to establish a connection.
It takes JSON Object as an input. The service and characteristics UUIDs should be provided. If not, it will try to subscribe all advertised characteristics.

The example input is shown below:
```
{
    "services": [
        "181a"
    ],
    "characteristics": [
        "2a6e",
        "2a6f",
        "2a6d"
    ]
}
```

More details are provided in Node-RED node information panel.

# Examples

An example flow that provides subscription to the all characteristics is given below.

<img src="images/exampleFlow2.png"></img>

# License

Licensed under the MIT [License](LICENSE).

# Cite

Please cite the following work if you use this package academically.

@article{KAYAN2021100437,<br />
title = {AnoML-IoT: An end to end re-configurable multi-protocol anomaly detection pipeline for Internet of Things},<br />
journal = {Internet of Things},<br />
pages = {100437},<br />
year = {2021},<br />
issn = {2542-6605},<br />
doi = {https://doi.org/10.1016/j.iot.2021.100437},<br />
url = {https://www.sciencedirect.com/science/article/pii/S2542660521000810},<br />
author = {Hakan Kayan and Yasar Majib and Wael Alsafery and Mahmoud Barhamgi and Charith Perera}<br />
}

# Latest Update Notes
- Fixed the random connection drops.
- Fixed the wrong node status for the scanner node.
- Dependencies are updated.
- Code is less spaghetti.

# Known Bugs

- Bluetooth adapter state event is not properly emitted during initialization.
