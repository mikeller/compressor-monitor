'use strict';

const URL = '/api/getData';
const intervalMs = 5000;

var pressureGauge = new RadialGauge({
    renderTo: 'pressure_chart',
    maxValue: 250,
    majorTicks: [0, 50, 100, 150, 200, 250],
    minorTicks: 5,
    units: "bar",
    title: "Pressure",
    highlights: getPressureHighlights(200),
});

let timeRemainingGauge = new RadialGauge(Object.assign({
    renderTo: 'time_remaining_chart',
    minValue: -10,
    minorTicks: 5,
    units: "min",
    title: "Time Remaining",
}, getTimeRemainingOptions(1800000, 120000)));

let batteryGauge = new RadialGauge({
    renderTo: 'battery_chart',
    maxValue: 20,
    majorTicks: [0, 5, 10, 15, 20],
    minorTicks: 5,
    units: "V",
    title: "Supply Voltage",
    highlights: [
        {
            from: 0,
            to: 9,
            color: 'red',
        }, {
            from: 9,
            to: 10.5,
            color: 'yellow',
        }
    ],
});

let overrideGauge = new RadialGauge({
    renderTo: 'override_chart',
    maxValue: 60,
    majorTicks: [0, 10, 20, 30, 40, 50, 60],
    minorTicks: 10,
    units: "sec",
    title: "Override",
    highlights: [
        {
            from: 0,
            to: 10,
            color: 'red',
        }, {
            from: 10,
            to: 60,
            color: 'yellow',
        }
    ],
});

function getPressureHighlights(pressureLimitBar) {
    return [
        {
            from: pressureLimitBar,
            to: 250,
            color: 'red',
        }, {
            from: pressureLimitBar - 10,
            to: pressureLimitBar,
            color: 'yellow',
        }
    ];
}

function getTimeRemainingOptions(maxTimeRemainingMs, warnTimeMs) {
    let majorTicks = [];
    for (let i = -10; i <= maxTimeRemainingMs / 60000; i+=5) {
        majorTicks.push(i);
    }

    return {
        maxValue: maxTimeRemainingMs / 60000,
        majorTicks: majorTicks,
        highlights: [
            {
                from: -10,
                to: 0,
                color: 'red',
            }, {
                from: 0,
                to: warnTimeMs / 60000,
                color: 'yellow',
            }
        ],
    };
}

async function fetchAndUpdate() {
    let result = await fetch(URL);
    let data = await result.json();

    updateDisplay(data);
}

let oldPressureLimitBar = 0;
let oldPurgeIntervalMs = 0;
let oldWarnTimeMs = 0;

function updateDisplay(data) {
    if (data) {
        let pressureOptions = {};
        let pressureLimitBar = data.settings.pressureLimitBar;
        if (oldPressureLimitBar !== pressureLimitBar) {
            pressureOptions.highlights = getPressureHighlights(pressureLimitBar);
            oldPressureLimitBar = pressureLimitBar;
        }
        pressureOptions.value = data.pressureBar;
        pressureGauge.update(pressureOptions);

        let timeRemainingOptions = {};
        let purgeIntervalMs = data.settings.purgeIntervalMs;
        let warnTimeMs = data.settings.warnTimeMs;
        if (oldPurgeIntervalMs !== purgeIntervalMs || oldWarnTimeMs !== warnTimeMs) {
            timeRemainingOptions = getTimeRemainingOptions(purgeIntervalMs, warnTimeMs);
            oldPurgeIntervalMs = purgeIntervalMs;
            oldWarnTimeMs = warnTimeMs;
        }
        let timeRemainingMin = data.timeUntilPurgeMs / 60000;
        if (data.timeUntilFullEstimateMs >= 0 && data.timeUntilFullEstimateMs / 60000 < timeRemainingMin) {
            timeRemainingMin = data.timeUntilFullEstimateMs / 60000;
        }
        timeRemainingOptions.value = timeRemainingMin;
        timeRemainingGauge.update(timeRemainingOptions);

        let overrideDurationS = data.overrideCountdownDurationMs / 1000;
        if (overrideDurationS) {
            document.getElementById("override_chart").style.visibility = "visible";
        } else {
            document.getElementById("override_chart").style.visibility = "hidden";
        }
        overrideGauge.update({ value: 60 - overrideDurationS, });

        batteryGauge.update({ value: data.batteryV, });

        document.getElementById("pressure_status_text").innerText = data.pressureState;
        document.getElementById("ignition_status_text").innerText = data.ignitionState;
        let runTimeH = data.runTimeMs / 1000 / 60 / 60;
        document.getElementById("run_time_text").innerText = `${runTimeH.toFixed(2)} h`;

        document.getElementById("alerts_text").innerText = data.alerts.join("\n");
    } else {
        pressureGauge.update({ value: -888.88 });

        timeRemainingGauge.update({ value: -888.88 });

        overrideGauge.update({ value: -888.88 });

        batteryGauge.update({ value: -888.88 });

        document.getElementById("pressure_status_text").innerText = `NO DATA`;
        document.getElementById("ignition_status_text").innerText = `NO DATA`;
        document.getElementById("run_time_text").innerText = `NO DATA`;
    }
}

async function checkRegisterServiceWorker() {
    await navigator.serviceWorker.register('serviceworker.js');
}

function connectToServiceWorker(registration) {
    registration.active.postMessage({
        type: 'CONNECT',
        intervalMs: intervalMs,
    });
}

async function main() {
    pressureGauge.draw();
    timeRemainingGauge.draw();
    batteryGauge.draw();
    overrideGauge.draw();

    let useServiceWorker = 'serviceWorker' in navigator;
    if (useServiceWorker) {
        let permission = await Notification.requestPermission();
        if (permission !== "granted") {
            useServiceWorker = false;

            console.log(`Notification permission denied.`);
        }
    }

    if (useServiceWorker) {
        await checkRegisterServiceWorker();

        let registration = await navigator.serviceWorker.ready;

        // Including this, but it's probably not triggered often enough to make a difference
        registration.periodicSync.register('trigger-check-task', {
            minInterval: 30 * 1000,
        });

        navigator.serviceWorker.onmessage = function (event) {
            if (event.data && event.data.type === 'DATA') {
                updateDisplay(event.data.payload);
            }
        }

        connectToServiceWorker(registration);

        document.addEventListener('visibilitychange', event => {
            if (document.visibilityState === 'visible') {
                updateDisplay();

                connectToServiceWorker(registration);
            }
        });
    } else {
        setInterval(fetchAndUpdate, intervalMs);
    }

    document.getElementById("notifications_status_text").innerText = useServiceWorker ? `ON` : `OFF`;
}

function confirmResetPurgeInterval()
{
    let result = confirm("OK to reset the purge interval?");
    if (result) {
        fetch("/api/resetPurgeInterval");
    }

    return false;
}

main();
