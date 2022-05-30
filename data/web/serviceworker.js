'use strict';

const URL = '/api/getData';

self.addEventListener("fetch", event => {
    // default behaviour: send the request
    event.respondWith(fetch(event.request));
});

self.addEventListener('install', function(event) {
    event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
    event.waitUntil(clients.claim());
});

function showNotification(data) {
    let text;
    if (data.pressureBar > data.pressureLimitBar) {
        text = `Over pressure: ${data.pressureBar} Bar`;
    } else {
        text = `Pressure: ${data.pressureBar} Bar`;
    }

    self.registration.showNotification(`Compressor`, {
        body: text,
        tag: `compressor-alert`,
        vibrate: [200, 100, 200],
        renotify: true,
        silent: false,
    });
}

function closeNotifications() {
    registration.getNotifications({ tag: `compressor-alert` }).then(notifications => {
        notifications.forEach(notification => notification.close());
    });
}

function checkShowNotification(data, intervalS) {
    let now = Date.now();
    if (now > lastNotificationMs + intervalS * 1000) {
        closeNotifications();

        showNotification(data);

        lastNotificationMs = now;
    }
}

let listeners = [];
let lastNotificationMs = 0;

function fetchData() {
	return fetch(URL)
        .then(result => result.json())
        .then(data => {
            listeners.forEach(listener => {
                listener.postMessage({
                    type: `DATA`,
                    payload: data
                });
            });

            if (data.pressureBar > data.pressureLimitBar) {
                checkShowNotification(data, 10);
            } else if (data.pressureBar > (data.pressureLimitBar - 10)) {
                checkShowNotification(data, 20);
            } else {
                closeNotifications();
            }
        })
    .catch(error => {
        console.error(`Problem fetching data: ${error}`);
    });
}

let fetcherTask;
let intervalMs = 5000;

function triggerFetcherTask() {
    fetchData();

    if (fetcherTask) {
        clearInterval(fetcherTask);
    }

    fetcherTask = setInterval(fetchData, intervalMs);
}


function main() {
    self.addEventListener("message", event => {
        if (event.data) {
            if (event.data.type === 'CONNECT') {
                listeners.push(event.source);
                intervalMs = event.data.intervalMs || intervalMs;

                triggerFetcherTask();

                console.log(`Service worker connected.`);
            } else if (event.data.type === 'ACTIVATE') {
                triggerFetcherTask();

                console.log(`Service worker activated.`);
            }
        }
    });
}

main();
