import fs from 'fs';
import mutexify from 'mutexify';
import { exec } from 'child_process';
import { EventEmitter } from 'events';

const tun = require('../build/Release/tun');

export interface Tunnel {
  readonly fd: number;
  readonly ip?: string;
  readonly name: string;
  readonly netmask?: string;

  write(data: Buffer, callback: (error: Error | null | undefined, bytesWritten?: number) => void): void;
  write(data: Buffer): Promise<number>;

  addListener(event: 'data', listener: (data: Buffer) => void): this;
  on(event: 'data', listener: (data: Buffer) => void): this;
  once(event: 'data', listener: (data: Buffer) => void): this;
  prependListener(event: 'data', listener: (data: Buffer) => void): this;
  prependOnceListener(event: 'data', listener: (data: Buffer) => void): this;
  removeListener(event: 'data', listener: (data: Buffer) => void): this;
  off(event: 'data', listener: (data: Buffer) => void): this;
  removeAllListeners(event: 'data'): this;
  emit(event: 'data', data: Buffer): boolean;

  addListener(event: 'error', listener: (err: Error) => void): this;
  on(event: 'error', listener: (err: Error) => void): this;
  once(event: 'error', listener: (err: Error) => void): this;
  prependListener(event: 'error', listener: (err: Error) => void): this;
  prependOnceListener(event: 'error', listener: (err: Error) => void): this;
  removeListener(event: 'error', listener: (err: Error) => void): this;
  off(event: 'error', listener: (err: Error) => void): this;
  removeAllListeners(event: 'error'): this;
  emit(event: 'error', err: Error): boolean;

  addListener(event: 'ready', listener: () => void): this;
  on(event: 'ready', listener: () => void): this;
  once(event: 'ready', listener: () => void): this;
  prependListener(event: 'ready', listener: () => void): this;
  prependOnceListener(event: 'ready', listener: () => void): this;
  removeListener(event: 'ready', listener: () => void): this;
  off(event: 'ready', listener: () => void): this;
  removeAllListeners(event: 'ready'): this;
  emit(event: 'ready'): boolean;

  removeAllListeners(): this;
  setMaxListeners(n: number): this;
  getMaxListeners(): number;
  listeners(event: string | symbol): Function[];
  rawListeners(event: string | symbol): Function[];
  eventNames(): Array<string | symbol>;
  listenerCount(type: string | symbol): number;
}

function isIpV4(value: string): boolean {
  return /^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$/.test(value);
}

export function create(opts: { ip?: string; netmask?: string } = {}): Tunnel {
  if (opts.ip) {
    opts.netmask = opts.netmask || '255.255.255.255';

    if (!isIpV4(opts.ip)) {
      throw new Error(`ip must be an IPv4 address: ${opts.ip}`);
    }
    if (!isIpV4(opts.netmask)) {
      throw new Error(`netmask must be an IPv4 address: ${opts.netmask}`);
    }
  }

  const emitter = new EventEmitter();

  const { fd, name } = tun.tun(() => {
    const lock = mutexify();
    const recv = Buffer.alloc(8192);

    lock(release => {
      fs.read(fd, recv, 0, recv.length, null, (err, readBytes) => {
        if (!err) {
          tunnel.emit('data', recv.slice(4, readBytes));
        }
        release();
      });
    });
  });

  const tunnel = Object.assign(emitter, {
    get fd() {
      return fd;
    },
    get ip() {
      return opts.ip;
    },
    get name() {
      return name;
    },
    get netmask() {
      return opts.netmask;
    },
    write(data: Buffer, callback?: (error: Error | null | undefined, bytesWritten?: number) => void) {
      const packet = Buffer.concat([Buffer.from([0, 0, 0, 2]), data]);

      if (callback) {
        return fs.write(fd, packet, 0, packet.length, null, callback);
      }

      return new Promise<number>((resolve, reject) => {
        fs.write(fd, packet, 0, packet.length, null, (err, bytesWritten) => {
          err ? reject(err) : resolve(bytesWritten);
        });
      });
    }
  }) as Tunnel;

  if (opts.ip) {
    if (fs.existsSync('/sbin/ifconfig')) {
      exec(`/sbin/ifconfig ${name} inet ${opts.ip} ${opts.ip} netmask ${opts.netmask}`, err => {
        err ? tunnel.emit('error', err) : tunnel.emit('ready');
      });
    } else if (fs.existsSync('/sbin/ip')) {
      // ip link set dev tun0 mtu 1500 ??
      exec(`/sbin/ip addr add ${opts.ip}/${opts.netmask} dev ${name}`, err => {
        err ? tunnel.emit('error', err) : tunnel.emit('ready');
      });
    } else {
      throw new Error('Cannot find either /sbin/ip or /sbin/ifconfig. Cannot use the ip option on this platform. =-(');
    }
  } else {
    tunnel.emit('ready');
  }

  return tunnel;
}
