#!/usr/bin/node 

const express = require('express');
const keypress = require('keypress');

const port = 8080;

const pressureStates = [
  'FILL',
  'CLOSE',
  'OVER',
  'STOP',
];

const pressureStateValuesBar = [
  180.6999969,
  191.6999969,
  202.6999969,
  213.6999969,
];

const baseData = {
    pressureBar: 0,
    pressureLimitBar: 200,
    pressureState: 'FILL',
    ignitionState: 'ON',
    overrideCountdownDurationMs: 0,
    runTimeMs: 0,
    timeUntilPurgeMs: 900000,
    batteryV: 10.683399916,
};

let startTimeMs = 0;
let pressureState = 0;

function getData() {
  let data = Object.assign({}, baseData);
  data.runTimeMs = Date.now() - startTimeMs;
  data.pressureState = pressureStates[pressureState];
  data.pressureBar = pressureStateValuesBar[pressureState];

  return data;
}

function printPressureState() {
  console.log(`Pressure state set to ${pressureStates[pressureState]} (${pressureStateValuesBar[pressureState]} bar)`);
}

function processInput(ch, key) {
  switch (key.name) {
  case 'c':
    if (key.ctrl) {
        process.exit();
    }

    break;
  case 'x':
    process.exit();

    break;
  case 'up':
    if (pressureState < pressureStates.length - 1) {
        pressureState++;
        printPressureState();
    }

    break;
  case 'down':
    if (pressureState > 0) {
        pressureState--;
        printPressureState();
    }

    break;
  }
}

function main() {
  startTimeMs = Date.now();

  console.log(`Use up / down arrows to change pressure, 'x' to exit.`);

  let stdin = process.stdin;
  keypress(stdin);
  stdin.setRawMode(true);
  stdin.resume();
  stdin.on('keypress', processInput);

  let app = express();
  app.get('/api/getData', (req, res) => {
    let data = getData();
    res.send(data);
    console.log(`Sending data:\n${JSON.stringify(data, null, 2)}`);
  });

  app.listen(port, () => {
    console.log(`Simulator listening on port ${port}`);
  });
}

main();
