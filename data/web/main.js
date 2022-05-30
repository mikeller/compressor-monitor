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
    highlights: getPressureHighlights(),
});

let purgeGauge = new RadialGauge({
    renderTo: 'purge_chart',
    minValue: -10,
    maxValue: 15,
    majorTicks: [-10, 5, 0, 5, 10, 15],
    minorTicks: 5,
    units: "min",
    title: "Purge Due In",
    highlights: [
        {
            from: -10,
            to: 0,
            color: 'red',
        }, {
            from: 0,
            to: 1,
            color: 'yellow',
        }
    ],
});

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

function getPressureHighlights (pressureLimitBar) {
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

function fetchAndUpdate() {
    fetch(URL)
        .then(result => result.json())
        .then(data => updateDisplay(data))
        .catch(err => {
            throw err;
        });
}

let oldPressureLimitBar = 0;

function updateDisplay(data) {
    let pressureOptions = {
        value: data.pressureBar,
    };
    if (oldPressureLimitBar !== data.pressureLimitBar) {
        pressureOptions.highlights = getPressureHighlights(data.pressureLimitBar);
        oldPressureLimitBar = data.pressureLimitBar;
    }
    pressureGauge.update(pressureOptions);

    purgeGauge.update({ value: data.timeUntilPurgeMs / 60000, });

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
    let runTimeH = Math.trunc(data.runTimeMs / 1000 / 60 / 60);
    document.getElementById("run_time_text").innerText = `${runTimeH.toFixed(2)} h`;
}

function main() {
    pressureGauge.draw();
    purgeGauge.draw();
    batteryGauge.draw();
    overrideGauge.draw();

    let useServiceWorker = 'serviceWorker' in navigator;
    if (useServiceWorker) {
        Notification.requestPermission().then(function (permission) {
            // If the user accepts, let's create a notification
            if (permission !== "granted") {
                useServiceWorker = false;
                console.log(`Notification permission denied.`);
            }
        });
    }

    if (useServiceWorker) {
        navigator
            .serviceWorker
            .register(
                // path to the service worker file
                'serviceworker.js'
            )
            .catch(error => {
                console.error(`Problem registering service worker: ${error}`);
            });

        navigator.serviceWorker.ready.then(registration => {
            registration.active.postMessage({
                type: 'CONNECT',
                intervalMs: intervalMs,
            });

            document.addEventListener('visibilitychange', event => {
                if (document.visibilityState === 'visible') {
                    registration.active.postMessage({
                        type: 'ACTIVATE',
                    });
                }
            });

            navigator.serviceWorker.onmessage = function (event) {
                if (event.data && event.data.type === 'DATA') {
                    updateDisplay(event.data.payload);
                }
            };
        });
    } else {
        setInterval(fetchAndUpdate, intervalMs);
    }

    document.getElementById("notifications_status_text").innerText = useServiceWorker ? `ON` : `OFF`;
}

main();
