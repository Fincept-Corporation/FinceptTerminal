// Browser polyfill for node:net module
export const isIP = (input: string): number => {
  // Simple IP validation for browser environment
  const ipv4Regex = /^(\d{1,3}\.){3}\d{1,3}$/;
  const ipv6Regex = /^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$/;

  if (ipv4Regex.test(input)) return 4;
  if (ipv6Regex.test(input)) return 6;
  return 0;
};

export const isIPv4 = (input: string): boolean => isIP(input) === 4;
export const isIPv6 = (input: string): boolean => isIP(input) === 6;

// Stub for net.connect - not functional in browser
export const connect = (...args: any[]): any => {
  console.warn('net.connect is not supported in browser environment');
  return null;
};

// Stub for Socket class - not functional in browser
export class Socket {
  constructor(...args: any[]) {
    console.warn('net.Socket is not supported in browser environment');
  }
}

export default { isIP, isIPv4, isIPv6, connect, Socket };
