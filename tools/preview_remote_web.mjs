import { createServer, request } from 'node:http';
import { readFile } from 'node:fs/promises';

const port = Number(process.env.PORT || 4173);
const upstream = process.env.DCO_URL || 'http://192.168.4.1';
createServer(async (incoming, outgoing) => {
  if (incoming.url.startsWith('/api/')) {
    const proxy = request(new URL(incoming.url, upstream), {
      method: incoming.method, headers: { ...incoming.headers, host: new URL(upstream).host }, timeout: 2500
    }, response => {
      outgoing.writeHead(response.statusCode, response.headers); response.pipe(outgoing);
    });
    proxy.on('timeout', () => proxy.destroy());
    proxy.on('error', () => {
      if (!outgoing.headersSent) outgoing.writeHead(503, { 'Content-Type': 'application/json' });
      outgoing.end(JSON.stringify({ error: 'DCO-ONE hors ligne. Rejoindre son reseau Wi-Fi.' }));
    });
    incoming.pipe(proxy);
    return;
  }
  try {
    const header = await readFile(new URL('../esp32/src/remote_page.h', import.meta.url), 'utf8');
    const start = header.indexOf('R"DCOHTML(') + 10;
    const page = header.slice(start, header.lastIndexOf(')DCOHTML"'));
    outgoing.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
    outgoing.end(page);
  } catch {
    outgoing.writeHead(503); outgoing.end('Build the remote web page first.');
  }
}).listen(port, '127.0.0.1', () => console.log(`DCO-ONE preview: http://127.0.0.1:${port}`));