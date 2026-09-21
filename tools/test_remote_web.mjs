import { chromium } from '../esp32/web/node_modules/playwright/index.mjs';
import { readFile, mkdir } from 'node:fs/promises';
import assert from 'node:assert/strict';

const header = await readFile(new URL('../esp32/src/remote_page.h', import.meta.url), 'utf8');
const html = header.slice(header.indexOf('R"DCOHTML(') + 10, header.lastIndexOf(')DCOHTML"'));
const menu = JSON.parse(await readFile(new URL('../menu.json', import.meta.url), 'utf8'));
const entries = key => menu.main_menus.find(item => item.key === key).submenus;
const oscillator = entries('OSC');
const groups = [oscillator.slice(0, 6).concat(oscillator[6].submenus), entries('VCF'), entries('LFO'), [],
  entries('ENV1').slice(0, 4).concat(entries('ENV2').slice(0, 4))];
const browser = await chromium.launch({ headless: true });
const output = new URL('../build/display-preview/', import.meta.url);
await mkdir(output, { recursive: true });
try {
  for (const viewport of [{ width: 1440, height: 1000 }, { width: 390, height: 844 }]) {
    const page = await browser.newPage({ viewport });
    const errors = [], commands = [];
    page.on('pageerror', error => errors.push(error.message));
    let fail = false, connected = true;
    const state = {
      token: 'test', connected: true, slot: 7, playing: false, count: 16, head: -1, bpm: 120, busy: false,
      parametersReady: true, command: { id: 0, status: 'idle' },
      steps: Array.from({ length: 32 }, () => ({ state: 1, degree: 0, transpose: 0 })),
      parameters: groups.map(group => [...group.map(item => item.default), ...Array(24 - group.length).fill(0)])
    };
    await page.route('http://dco.test/**', async route => {
      const url = new URL(route.request().url());
      if (url.pathname === '/') return route.fulfill({ contentType: 'text/html', body: html });
      if (url.pathname === '/api/state') return route.fulfill({ json: { ...state, connected } });
      if (url.pathname === '/api/patterns') return route.fulfill({ json: { patterns: [{ slot: 7, name: 'Test synthese' }] } });
      const command = route.request().postDataJSON(); commands.push(command);
      if (fail) return route.fulfill({ status: 409, json: { error: 'Commande refusee' } });
      if (command.action === 'param') state.parameters[command.group][command.parameter] = command.value;
      state.command = { id: commands.length, status: 'ok' };
      return route.fulfill({ status: 202, json: { id: commands.length } });
    });
    await page.goto('http://dco.test/');
    await page.locator('#connection[data-online=true]').waitFor();
    await page.locator('#synthTab').click();
    await page.locator('#param-1-2').fill('72');
    await page.locator('#param-1-2').press('Tab');
    await page.getByText('Son modifi', { exact: false }).waitFor();
    assert.equal(state.parameters[1][2], 72);
    await page.locator('#param-2-5').selectOption('3');
    await page.waitForFunction(() => !document.querySelector('#param-2-5').disabled);
    assert.equal(state.parameters[2][5], 3);
    for (let parameter = 0; parameter < 8; ++parameter) {
      const value = parameter % 4 === 2 ? 35 : 650;
      const control = page.locator(`#param-4-${parameter}`);
      await control.fill(String(value));
      await control.press('Tab');
      await page.waitForFunction(() => !document.querySelector('#param-4-0').disabled);
      assert.equal(state.parameters[4][parameter], value);
    }
    const envelope = page.locator('canvas[data-wave-group="4"]').first();
    assert.match(await envelope.getAttribute('aria-label'), /Sustain 35 %/);
    const beforeCurve = await envelope.evaluate(canvas => canvas.toDataURL());
    state.parameters[4][2] = 80;
    await page.locator('#synthTab').focus();
    await page.waitForFunction(() => document.querySelector('#param-4-2').value === '80');
    assert.notEqual(await envelope.evaluate(canvas => canvas.toDataURL()), beforeCurve);
    await page.screenshot({ path: new URL(`web-synth-${viewport.width}.png`, output).pathname, fullPage: true });
    const colored = await page.locator('canvas').first().evaluate(canvas => {
      const data = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
      return data.some((value, index) => index % 4 === 3 && value > 0);
    });
    assert(colored);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
    await page.locator('#matrixTab').click();
    assert.equal(await page.locator('.route').count(), 8);
    await page.locator('#param-3-21').selectOption('3');
    await page.waitForFunction(() => !document.querySelector('#param-3-22').disabled);
    await page.locator('#param-3-22').selectOption('6');
    await page.waitForFunction(() => !document.querySelector('#param-3-23').disabled);
    await page.locator('#param-3-23').fill('-35');
    await page.locator('#param-3-23').press('Tab');
    await page.waitForFunction(() => !document.querySelector('#param-3-23').disabled);
    assert.deepEqual(state.parameters[3].slice(21), [3, 6, -35]);
    await page.getByRole('checkbox', { name: 'Activer routage 8' }).uncheck();
    await page.waitForFunction(() => !document.querySelector('#param-3-21').disabled);
    assert.equal(state.parameters[3][21], 0);
    await page.getByRole('checkbox', { name: 'Activer routage 8' }).check();
    await page.waitForFunction(() => !document.querySelector('#param-3-21').disabled);
    assert.equal(state.parameters[3][21], 3);
    await page.screenshot({ path: new URL(`web-matrix-${viewport.width}.png`, output).pathname, fullPage: true });
    const overflow = await page.evaluate(() => document.documentElement.scrollWidth > innerWidth);
    assert.equal(overflow, false, `Horizontal overflow at ${viewport.width}`);
    fail = true;
    await page.locator('#param-3-23').fill('80');
    await page.locator('#param-3-23').press('Tab');
    await page.getByText('Commande refusee', { exact: true }).waitFor();
    assert.equal(await page.locator('#param-3-23').inputValue(), '-35');
    connected = false;
    await page.locator('#connection[data-online=false]').waitFor();
    assert(await page.locator('#param-3-23').isDisabled());
    assert.deepEqual(errors, []);
    await page.close();
    console.log(`Web ${viewport.width}px: synth controls, LFO2, route 8, bypass, error recovery, offline, nonblank canvases and layout OK`);
  }
} finally { await browser.close(); }