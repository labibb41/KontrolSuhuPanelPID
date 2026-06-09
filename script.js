// Splash Screen Logic
window.addEventListener('load', () => {
    setTimeout(() => {
        const splash = document.getElementById('splashScreen');
        if (splash) {
            splash.classList.add('hidden');
            setTimeout(() => {
                splash.remove();
            }, 800);
        }
    }, 2000);
});

// Konfigurasi MQTT EMQX Cloud
// Sesuaikan jika endpoint WebSocket broker kamu berbeda.
const mqttUrl = 'wss://s6ddf312.ala.asia-southeast1.emqxsl.com:8084/mqtt';
const mqttOptions = {
    clientId: `web_client_${Math.random().toString(16).slice(2, 10)}`,
    username: 'ESP32_ZAQI',
    password: 'Zaqi123',
    clean: true,
    connectTimeout: 4000,
    reconnectPeriod: 2000,
    keepalive: 30,
    protocolVersion: 4,
};

const topics = {
    monitoring: 'monitoring/data',
    setpoint: 'kontrol/Setpoint',
    mode: 'kontrol/mode',
    relay1Command: 'kontrol/relay1_command',
    relay2Command: 'kontrol/relay2_command',
};

// Referensi Elemen DOM
const elStatusText = document.getElementById('statusText');
const elConnectionStatus = document.getElementById('connectionStatus');
const elCurrentTemp = document.getElementById('currentTemp');
const elPidOutput = document.getElementById('pidOutput');
const elPidMeterBar = document.getElementById('pidMeterBar');
const elSetpointInput = document.getElementById('setpointInput');
const elBtnSaveSetpoint = document.getElementById('btnSaveSetpoint');
const elRelay1Status = document.getElementById('relay1StatusText');
const elRelay1Toggle = document.getElementById('relay1Toggle');
const elRelay2Status = document.getElementById('relay2StatusText');
const elRelay2Toggle = document.getElementById('relay2Toggle');
const radioModeAuto = document.getElementById('modeAuto');
const radioModeManual = document.getElementById('modeManual');

let mqttClient = null;
let lastMode = 'auto';

connectMqtt();

function connectMqtt() {
    mqttClient = mqtt.connect(mqttUrl, mqttOptions);

    mqttClient.on('connect', () => {
        updateConnectionStatus(true);
        mqttClient.subscribe([topics.monitoring, topics.setpoint, topics.mode, topics.relay1Command, topics.relay2Command]);
    });

    mqttClient.on('reconnect', () => {
        updateConnectionStatus(false);
    });

    mqttClient.on('close', () => {
        updateConnectionStatus(false);
    });

    mqttClient.on('offline', () => {
        updateConnectionStatus(false);
    });

    mqttClient.on('error', (error) => {
        console.error('MQTT Error:', error);
        updateConnectionStatus(false);
    });

    mqttClient.on('message', handleMqttMessage);
}

function handleMqttMessage(topic, payloadBuffer) {
    const payload = payloadBuffer.toString();

    if (topic === topics.monitoring) {
        try {
            const data = JSON.parse(payload);
            if (data.suhu !== undefined) {
                updateTemperature(data.suhu);
            }

            if (data.output_pid !== undefined) {
                updatePidOutput(data.output_pid);
            }

            if (data.setpoint !== undefined) {
                syncSetpointInput(data.setpoint);
            }

            if (data.mode !== undefined) {
                syncMode(String(data.mode).toLowerCase());
            }

            if (data.relay1 !== undefined) {
                updateRelayUI(1, String(data.relay1).toUpperCase() === 'ON');
            }

            if (data.relay2 !== undefined) {
                updateRelayUI(2, String(data.relay2).toUpperCase() === 'ON');
            } else if (data.status_kipas !== undefined) {
                const isOn = String(data.status_kipas).toUpperCase() === 'ON';
                updateRelayUI(1, isOn);
                updateRelayUI(2, isOn);
            }
        } catch (error) {
            console.error('Gagal parse payload monitoring:', error);
        }
        return;
    }

    if (topic === topics.setpoint) {
        syncSetpointInput(payload);
        return;
    }

    if (topic === topics.mode) {
        syncMode(payload.toLowerCase());
        return;
    }

    if (topic === topics.relay1Command) {
        updateRelayUI(1, payload.toUpperCase() === 'ON');
        return;
    }

    if (topic === topics.relay2Command) {
        updateRelayUI(2, payload.toUpperCase() === 'ON');
    }
}

function publish(topic, payload, retain = true) {
    if (!mqttClient || !mqttClient.connected) return false;
    mqttClient.publish(topic, String(payload), { retain });
    return true;
}

function updateConnectionStatus(isOnline) {
    if (isOnline) {
        elConnectionStatus.classList.remove('offline');
        elConnectionStatus.classList.add('online');
        elStatusText.textContent = 'MQTT Online';
    } else {
        elConnectionStatus.classList.remove('online');
        elConnectionStatus.classList.add('offline');
        elStatusText.textContent = 'MQTT Offline';
    }
}

function updateTemperature(value) {
    const suhu = parseFloat(value);
    if (!isNaN(suhu)) {
        elCurrentTemp.textContent = suhu.toFixed(1);
    }
}

function updatePidOutput(value) {
    const output = parseFloat(value);
    if (isNaN(output)) return;

    elPidOutput.textContent = output.toFixed(0);

    const percent = Math.max(0, Math.min(100, (output / 5000) * 100));
    elPidMeterBar.style.width = `${percent}%`;
}

function syncSetpointInput(value) {
    const setpoint = parseFloat(value);
    if (!isNaN(setpoint) && document.activeElement !== elSetpointInput) {
        elSetpointInput.value = setpoint.toFixed(1);
    }
}

function syncMode(mode) {
    lastMode = mode === 'manual' ? 'manual' : 'auto';
    const isManual = lastMode === 'manual';

    radioModeManual.checked = isManual;
    radioModeAuto.checked = !isManual;
    enableManualControl(isManual);
}

function updateRelayUI(relayNum, isOn) {
    const statusEl = relayNum === 1 ? elRelay1Status : elRelay2Status;
    const toggleEl = relayNum === 1 ? elRelay1Toggle : elRelay2Toggle;

    if (document.activeElement !== toggleEl) {
        toggleEl.checked = isOn;
    }

    statusEl.textContent = isOn ? 'ON' : 'OFF';
    statusEl.classList.toggle('on', isOn);
}

function revertRelayToggle(relayNum) {
    const toggleEl = relayNum === 1 ? elRelay1Toggle : elRelay2Toggle;
    updateRelayUI(relayNum, !toggleEl.checked);
}

function enableManualControl(isManual) {
    elRelay1Toggle.disabled = !isManual;
    elRelay2Toggle.disabled = !isManual;
}

function showSaveFeedback() {
    const originalText = elBtnSaveSetpoint.innerHTML;
    elBtnSaveSetpoint.innerHTML = '<i class="fa-solid fa-check"></i> Tersimpan';
    elBtnSaveSetpoint.style.background = '#4caf50';

    setTimeout(() => {
        elBtnSaveSetpoint.innerHTML = originalText;
        elBtnSaveSetpoint.style.background = '';
    }, 2000);
}

// Tombol Simpan Setpoint
elBtnSaveSetpoint.addEventListener('click', () => {
    const newVal = parseFloat(elSetpointInput.value);
    if (isNaN(newVal)) return;

    if (publish(topics.setpoint, newVal, true)) {
        showSaveFeedback();
    } else {
        alert('MQTT belum tersambung.');
    }
});

// Perubahan Mode Operasi
radioModeAuto.addEventListener('change', () => {
    if (!radioModeAuto.checked) return;
    enableManualControl(false);

    if (!publish(topics.mode, 'auto', true)) {
        alert('MQTT belum tersambung.');
    }
});

radioModeManual.addEventListener('change', () => {
    if (!radioModeManual.checked) return;
    enableManualControl(true);

    if (!publish(topics.mode, 'manual', true)) {
        alert('MQTT belum tersambung.');
    }
});

// Toggle manual relay
elRelay1Toggle.addEventListener('change', (event) => {
    if (lastMode !== 'manual') {
        event.preventDefault();
        alert('Silakan ubah ke mode Manual terlebih dahulu untuk mengontrol kipas secara manual!');
        return;
    }

    const isOn = event.target.checked;
    updateRelayUI(1, isOn);

    if (!publish(topics.relay1Command, isOn ? 'ON' : 'OFF', true)) {
        revertRelayToggle(1);
        alert('MQTT belum tersambung.');
    }
});

elRelay2Toggle.addEventListener('change', (event) => {
    if (lastMode !== 'manual') {
        event.preventDefault();
        alert('Silakan ubah ke mode Manual terlebih dahulu untuk mengontrol kipas secara manual!');
        return;
    }

    const isOn = event.target.checked;
    updateRelayUI(2, isOn);

    if (!publish(topics.relay2Command, isOn ? 'ON' : 'OFF', true)) {
        revertRelayToggle(2);
        alert('MQTT belum tersambung.');
    }
});
