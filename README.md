# tun-ts

Cross-platform TUN device. Currently supports MacOS and Linux. Please send a PR to help with Windows!

## Installation

```
$ npm install tun-ts
```

## Usage

```
import * as tun from 'tun-ts';

// create a TUN device without an IP
const tunnel = tun.create();
// create a TUN device with an IP
const tunnel = tun.create({
  ip: '10.0.1.10',
  netmask: '255.255.255.255' // netmask is optional
});

console.log(tunnel);
/*
{
  fd: 20,
  ip: '10.0.1.10,
  name: 'tun0',
  netmask: '255.255.255.255'
}
*/

// data is a Buffer
tunnel.on('data', data => {
  // forward to another socket
  someSocket.write(data);
  // echo back
  tunnel.write(data);
});

tunnel.on('ready', () => console.log('Ready!'));
tunnel.on('error', err => console.log(err.stack));
```

## Usage with Docker

In order to use this module inside a docker container, you must do a few things:

- Run the container with `--privileged` or `--cap-add=NET_ADMIN`
- In alpine, install `linux-headers` (`apk add --update linux headers`)
- Create the `/dev/net/tun` special file (`mkdir -p /dev/net && mknod /dev/net/tun c 10 200`)
