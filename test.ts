import * as tun from '.';

const tunnel = tun.create({ ip: '13.13.13.13' });

console.log(tunnel);

tunnel.on('ready', () => console.log('ready'));

tunnel.on('data', packet => {
  console.log('GOT MESSAGE', packet.length, 'bytes');
});
