'use strict';

const url = '/api/getData';

let fillPressure = 200;

function getPressureHighlights () {
    return [
        {
            from: fillPressure,
            to: 250,
            color: 'red',
        }, {
            from: fillPressure - 10,
            to: fillPressure,
            color: 'yellow',
        }
    ];
}

let pressureGauge = new RadialGauge({
    renderTo: 'pressure_chart',
    maxValue: 250,
    majorTicks: [0, 50, 100, 150, 200, 250],
    minorTicks: 5,
    units: "bar",
    title: "Pressure",
    highlights: getPressureHighlights(),
});
pressureGauge.draw();

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
purgeGauge.draw();

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
batteryGauge.draw();

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
overrideGauge.draw();

setInterval(function() {
    fetch(url)
        .then(result => result.json())
        .then((data) => {
            let pressureOptions = {
                value: data.pressureBar,
            };
            if (fillPressure !== data.pressureLimitBar) {
                fillPressure = data.pressureLimitBar;
                pressureOptions.highlights = getPressureHighlights();
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
            let runTimeMin = Math.trunc(data.runTimeMs / 60000);
            document.getElementById("run_time_text").innerText = `${Math.trunc(runTimeMin / 60)}:${String(runTimeMin % 60).padStart(2, '0')}`;
        })
    .catch(err => { throw err });
}, 1000);
