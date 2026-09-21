import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { build } from '../esp32/web/node_modules/esbuild/lib/main.js';

const source = new URL('../esp32/web/index.html', import.meta.url);
const html = await readFile(source, 'utf8');
const menu = JSON.parse(await readFile(new URL('../menu.json', import.meta.url), 'utf8'));
const entries = key => menu.main_menus.find(item => item.key === key).submenus;
const oscillator = entries('OSC');
const definitions = [oscillator.slice(0, 6).concat(oscillator[6].submenus), entries('VCF'), entries('LFO'), [],
  entries('ENV1').slice(0, 4).concat(entries('ENV2').slice(0, 4))];
const script = html.match(/<script type="module">([\s\S]*?)<\/script>/);
if (!script) throw new Error('Missing web application script');
const result = await build({
  stdin: { contents: script[1].replace('__SYNTH_DEFINITIONS__', JSON.stringify(definitions)), resolveDir: fileURLToPath(new URL('../esp32/web/', import.meta.url)) },
  bundle: true, minify: true, format: 'iife', target: 'es2020', write: false, legalComments: 'inline'
});
const page = html.replace(script[0], () => `<script>${result.outputFiles[0].text}</script>`);
if (page.includes(')DCOHTML"')) throw new Error('Raw string delimiter collision');
await writeFile(new URL('../esp32/src/remote_page.h', import.meta.url),
  `#pragma once\n\nstatic const char kRemotePage[] PROGMEM = R"DCOHTML(${page})DCOHTML";\n`);
console.log(`Offline remote page: ${Buffer.byteLength(page)} bytes`);