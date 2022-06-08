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

async function closeNotifications() {
    let notifications = await registration.getNotifications({ tag: `compressor-alert` });
    notifications.forEach(notification => notification.close());
}

async function checkShowNotification(data, intervalS) {
    let now = Date.now();
    if (now > lastNotificationMs + intervalS * 1000) {
        await closeNotifications();

        showNotification(data);

        lastNotificationMs = now;
    }
}

let listeners = [];
let lastNotificationMs = 0;

async function fetchData() {
    let result = await fetch(URL);
    let data = await result.json();

    listeners.forEach(listener => {
        listener.postMessage({
            type: `DATA`,
            payload: data
        });
    });

    if (data.pressureBar > data.pressureLimitBar) {
        await checkShowNotification(data, 10);
    } else if (data.pressureBar > (data.pressureLimitBar - 10)) {
        await checkShowNotification(data, 20);
    } else {
        await closeNotifications();
    }
}

let fetcherTask;
let intervalMs = 5000;

async function triggerFetcherTask() {
    await fetchData();

    if (fetcherTask) {
        clearInterval(fetcherTask);
    }

    fetcherTask = setInterval(fetchData, intervalMs);
}

function main() {
    self.addEventListener("message", event => {
        if (event.data && event.data.type === 'CONNECT') {
            if (!listeners.includes(event.source)) {
                listeners.push(event.source);
            }
            intervalMs = event.data.intervalMs || intervalMs;

            event.waitUntil(triggerFetcherTask());

            console.log(`Service worker connected.`);
        }
    });

    self.addEventListener('periodicsync', event => {
        if (event.tag == 'trigger-check-task') {
            event.waitUntil(triggerFetcherTask());
        }
    });
}

main();
