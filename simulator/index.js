#!/usr/bin/node 

const express = require('express');
const app = express();
const port = 8080;

const data1 = {
    pressureBar: 180.6999969,
    pressureLimitBar: 200,
    pressureState: "FILL",
    ignitionState: "OFF",
    overrideCountdownDurationMs: 0,
    runTimeMs: 0,
    timeUntilPurgeMs: 900000,
    batteryV: 3.683399916,
};

const data2 = {
    pressureBar: 203.8000031,
    pressureLimitBar: 160,
    pressureState: "OVER",
    ignitionState: "ON",
    overrideCountdownDurationMs: 0,
    runTimeMs: 17072,
    timeUntilPurgeMs: 882928,
    batteryV: 3.615780115,
};

app.get('/api/getData', (req, res) => {
  res.send(data2);
})

app.listen(port, () => {
  console.log(`Simulator listening on port ${port}`);
})
