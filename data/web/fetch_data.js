use strict;

const url = '/api/getData';

google.charts.load('current', {'packages':['gauge']});
google.charts.setOnLoadCallback(drawChart);

function drawChart() {
  let pressureData = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    ['Pressure [Bar]', 0],
  ]);

  let pressureOptions = {
    max: 250,
    width: 400, height: 400,
    redFrom: 200, redTo: 250,
    yellowFrom: 190, yellowTo: 200,
    minorTicks: 10
  };

  let pressureChart = new google.visualization.Gauge(document.getElementById('pressure_chart_div'));

  pressureChart.draw(pressureData, pressureOptions);

  let overrideData = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    ['Override [s]', 61],
  ]);

  let overrideOptions = {
    max: 61,
    width: 200, height: 200,
    redFrom: 0, redTo: 10,
    yellowFrom: 10, yellowTo: 60,
    minorTicks: 1
  };

  let overrideChart = new google.visualization.Gauge(document.getElementById('override_chart_div'));

  overrideChart.draw(overrideData, overrideOptions);

  let purgeData = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    ['Purge Due [min]', 0],
  ]);

  let purgeOptions = {
    min: -10, max: 15,
    width: 200, height: 200,
    redFrom: -10, redTo: 0,
    yellowFrom: 0, yellowTo: 1,
    minorTicks: 1
  };

  let purgeChart = new google.visualization.Gauge(document.getElementById('purge_chart_div'));

  purgeChart.draw(purgeData, purgeOptions);

  let batteryData = google.visualization.arrayToDataTable([
    ['Label', 'Value'],
    ['Voltage', 0],
  ]);

  let batteryOptions = {
    max: 20,
    width: 200, height: 200,
    redFrom: 0, redTo: 9,
    yellowFrom: 9, yellowTo: 10.5,
    minorTicks: 1
  };

  let batteryChart = new google.visualization.Gauge(document.getElementById('battery_chart_div'));

  batteryChart.draw(batteryData, batteryOptions);

  setInterval(function() {
    fetch(url)
      .then(result => result.json())
      .then((data) => {
        pressureData.setValue(0, 1, data.pressureBar);
        if (pressureOptions.redFrom !== data.pressureLimitBar) {
          pressureOptions.redFrom = data.pressureLimitBar;
          pressureOptions.yellowTo = data.pressureLimitBar;
          pressureOptions.yellowFrom = data.pressureLimitBar - 10;
        }
        pressureChart.draw(pressureData, pressureOptions);

        purgeData.setValue(0, 1, data.timeUntilPurgeMs / 60000);
        purgeChart.draw(purgeData, purgeOptions);

        overrideData.setValue(0, 1, data.overrideCountdownDurationMs ? (60 - data.overrideCountdownDurationMs / 1000) : 61);
        overrideChart.draw(overrideData, overrideOptions);

        batteryData.setValue(0, 1, data.batteryV);
        batteryChart.draw(batteryData, batteryOptions);
      })
      .catch(err => { throw err });
  }, 1000);
}
