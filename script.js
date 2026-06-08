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

// Konfigurasi Firebase
// API key dan database URL disamakan dengan program ESP32.
const firebaseConfig = {
    apiKey: 'AIzaSyBdbq2LHD1n6smpjI67h2Um48ysPfVqUCo',
    databaseURL: 'https://kontrolpanel-f4a91-default-rtdb.firebaseio.com/',
};

const app = firebase.initializeApp(firebaseConfig);
const auth = firebase.auth(app);
const database = firebase.database(app);

// Path Firebase RTDB yang digunakan ESP32
const paths = {
    monitoring: '/monitoring',
    suhu: '/monitoring/suhu',
    statusKipas: '/monitoring/status_kipas',
    outputPid: '/monitoring/output_pid',
    setpoint: '/kontrol/Setpoint',
    mode: '/kontrol/mode',
    relay1Command: '/kontrol/relay1_command',
    relay2Command: '/kontrol/relay2_command',
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

signInAndStart();

async function signInAndStart() {
    try {
        await auth.signInAnonymously();
        startFirebaseListeners();
    } catch (error) {
        console.warn('Login anonymous Firebase gagal, mencoba akses database tanpa auth.', error);
        startFirebaseListeners();
    }
}

function startFirebaseListeners() {
    // Listener koneksi Firebase
    database.ref('.info/connected').on('value', (snapshot) => {
        updateConnectionStatus(snapshot.val() === true);
    });

    // Listener data monitoring dari ESP32
    database.ref(paths.monitoring).on('value', (snapshot) => {
        const data = snapshot.val();
        if (!data) return;

        updateTemperature(data.suhu);
        updatePidOutput(data.output_pid);

        if (typeof data.relay1 === 'string' || typeof data.relay2 === 'string') {
            if (typeof data.relay1 === 'string') {
                updateRelayUI(1, data.relay1.toUpperCase() === 'ON');
            }

            if (typeof data.relay2 === 'string') {
                updateRelayUI(2, data.relay2.toUpperCase() === 'ON');
            }
        } else if (typeof data.status_kipas === 'string') {
            const isOn = data.status_kipas.toUpperCase() === 'ON';
            updateRelayUI(1, isOn);
            updateRelayUI(2, isOn);
        }
    }, handleFirebaseError);

    // Listener setpoint dari Firebase agar input tetap sinkron dengan ESP32
    database.ref(paths.setpoint).on('value', (snapshot) => {
        const setpoint = parseFloat(snapshot.val());
        if (!isNaN(setpoint) && document.activeElement !== elSetpointInput) {
            elSetpointInput.value = setpoint.toFixed(1);
        }
    }, handleFirebaseError);

    // Listener mode operasi dari Firebase.
    database.ref(paths.mode).on('value', (snapshot) => {
        const mode = String(snapshot.val() || 'auto').toLowerCase();
        const isManual = mode === 'manual';

        radioModeManual.checked = isManual;
        radioModeAuto.checked = !isManual;
        enableManualControl(isManual);
    }, handleFirebaseError);
}

function updateConnectionStatus(isOnline) {
    if (isOnline) {
        elConnectionStatus.classList.remove('offline');
        elConnectionStatus.classList.add('online');
        elStatusText.textContent = 'Firebase Online';
    } else {
        elConnectionStatus.classList.remove('online');
        elConnectionStatus.classList.add('offline');
        elStatusText.textContent = 'Firebase Offline';
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

function updateRelayUI(relayNum, isOn) {
    const statusEl = relayNum === 1 ? elRelay1Status : elRelay2Status;
    const toggleEl = relayNum === 1 ? elRelay1Toggle : elRelay2Toggle;

    if (document.activeElement !== toggleEl) {
        toggleEl.checked = isOn;
    }

    statusEl.textContent = isOn ? 'ON' : 'OFF';

    if (isOn) {
        statusEl.classList.add('on');
    } else {
        statusEl.classList.remove('on');
    }
}

function revertRelayToggle(relayNum) {
    const toggleEl = relayNum === 1 ? elRelay1Toggle : elRelay2Toggle;
    const isOn = !toggleEl.checked;
    toggleEl.checked = isOn;
    updateRelayUI(relayNum, isOn);
}

function enableManualControl(isManual) {
    elRelay1Toggle.disabled = !isManual;
    elRelay2Toggle.disabled = !isManual;
}

function handleFirebaseError(error) {
    console.error('Firebase Error:', error);
    updateConnectionStatus(false);
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
elBtnSaveSetpoint.addEventListener('click', async () => {
    const newVal = parseFloat(elSetpointInput.value);
    if (isNaN(newVal)) return;

    try {
        await database.ref(paths.setpoint).set(newVal);
        showSaveFeedback();
    } catch (error) {
        handleFirebaseError(error);
        alert('Gagal menyimpan setpoint ke Firebase.');
    }
});

// Perubahan Mode Operasi
radioModeAuto.addEventListener('change', async () => {
    if (!radioModeAuto.checked) return;

    enableManualControl(false);
    try {
        await database.ref(paths.mode).set('auto');
    } catch (error) {
        handleFirebaseError(error);
    }
});

radioModeManual.addEventListener('change', async () => {
    if (!radioModeManual.checked) return;

    enableManualControl(true);
    try {
        await database.ref(paths.mode).set('manual');
    } catch (error) {
        handleFirebaseError(error);
    }
});

// Toggle manual disimpan sebagai command di Firebase dan dibaca ESP32.
elRelay1Toggle.addEventListener('change', async (event) => {
    if (!radioModeManual.checked) {
        event.preventDefault();
        alert('Silakan ubah ke mode Manual terlebih dahulu untuk mengontrol kipas secara manual!');
        return;
    }

    const cmd = event.target.checked ? 'ON' : 'OFF';
    updateRelayUI(1, event.target.checked);

    try {
        await database.ref(paths.relay1Command).set(cmd);
    } catch (error) {
        handleFirebaseError(error);
        revertRelayToggle(1);
    }
});

elRelay2Toggle.addEventListener('change', async (event) => {
    if (!radioModeManual.checked) {
        event.preventDefault();
        alert('Silakan ubah ke mode Manual terlebih dahulu untuk mengontrol kipas secara manual!');
        return;
    }

    const cmd = event.target.checked ? 'ON' : 'OFF';
    updateRelayUI(2, event.target.checked);

    try {
        await database.ref(paths.relay2Command).set(cmd);
    } catch (error) {
        handleFirebaseError(error);
        revertRelayToggle(2);
    }
});
