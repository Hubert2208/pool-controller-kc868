// Calibration Wizard — state machine, stability detection, API handlers
// Included by main.cpp

enum CalState { CAL_IDLE, CAL_PH_WAIT_7, CAL_PH_LOCKED_7, CAL_PH_WAIT_4, CAL_PH_LOCKED_4, CAL_ORP_WAIT, CAL_ORP_LOCKED };
static CalState calState = CAL_IDLE;
static float calPH7Voltage = 0.0f;
static float calPH4Voltage = 0.0f;
static float calORPRequiredMV = 220.0f;
static float calVoltageWindow[10];
static int calWindowIdx = 0;
static int calWindowCount = 0;

static void calResetWindow() { calWindowIdx = 0; calWindowCount = 0; }

static void calPushVoltage(float v) {
    calVoltageWindow[calWindowIdx] = v;
    calWindowIdx = (calWindowIdx + 1) % 10;
    if (calWindowCount < 10) calWindowCount++;
}

static float calPeakToPeakMV() {
    if (calWindowCount < 8) return 999.0f;
    float minV = calVoltageWindow[0], maxV = calVoltageWindow[0];
    for (int i = 1; i < calWindowCount; i++) {
        if (calVoltageWindow[i] < minV) minV = calVoltageWindow[i];
        if (calVoltageWindow[i] > maxV) maxV = calVoltageWindow[i];
    }
    return (maxV - minV) / 4.096f * 1000.0f;
}

static bool calIsStable() { return calPeakToPeakMV() < 2.0f; }

static String calDaysAgo(unsigned long ts) {
    if (ts == 0) return "never";
    time_t now = time(nullptr);
    if (now < 100000) return "unknown";
    int days = ((unsigned long)now - ts) / 86400;
    if (days == 0) return "today";
    char buf[24]; snprintf(buf, sizeof(buf), "%d days ago", days);
    return String(buf);
}

// ── Calibration API Handlers ──

void handleCalStatus() {
    StaticJsonDocument<256> doc;
    doc["state"] = (int)calState;
    PHSensor* ph = sensorManager ? sensorManager->getPHSensor() : nullptr;
    ORPSensor* orp = sensorManager ? sensorManager->getORPSensor() : nullptr;
    float rawV = 0.0f, rawMV = 0.0f;
    if (calState == CAL_PH_WAIT_7 || calState == CAL_PH_WAIT_4) {
        if (ph && ph->isConnected()) { rawV = ph->readRawVoltage(); calPushVoltage(rawV); }
    } else if (calState == CAL_ORP_WAIT) {
        if (orp && orp->isConnected()) { rawV = orp->readRawVoltage(); rawMV = orp->readRawMV(); calPushVoltage(rawV); }
    }
    doc["voltageV"] = rawV;
    doc["voltageMV"] = (calState == CAL_ORP_WAIT) ? rawMV : ((rawV / 4.096f) * 1000.0f);
    doc["driftMV"] = calPeakToPeakMV();
    doc["stable"] = calIsStable();
    if (calState >= CAL_PH_LOCKED_7) doc["lockedV"] = calPH7Voltage;
    if (calState == CAL_PH_LOCKED_4) { doc["slope"] = calibrationData.phSlope; doc["intercept"] = calibrationData.phIntercept; }
    if (calState >= CAL_ORP_WAIT) doc["orpRefMV"] = calORPRequiredMV;
    if (calState == CAL_ORP_LOCKED) doc["offset"] = calibrationData.orpOffset;
    String json; serializeJson(doc, json); webServer.send(200, "application/json", json);
}

void handleCalStartPH() {
    PHSensor* ph = sensorManager ? sensorManager->getPHSensor() : nullptr;
    if (!ph || !ph->isConnected()) { webServer.send(400, "application/json", "{\"error\":\"pH sensor not connected\"}"); return; }
    calState = CAL_PH_WAIT_7; calResetWindow();
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCalLockPH7() {
    PHSensor* ph = sensorManager ? sensorManager->getPHSensor() : nullptr;
    if (!ph) { webServer.send(400, "application/json", "{\"error\":\"no sensor\"}"); return; }
    calPH7Voltage = ph->readRawVoltage();
    calState = CAL_PH_LOCKED_7; calResetWindow();
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCalStartPH4() {
    calState = CAL_PH_WAIT_4; calResetWindow();
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCalLockPH4() {
    PHSensor* ph = sensorManager ? sensorManager->getPHSensor() : nullptr;
    if (!ph) { webServer.send(400, "application/json", "{\"error\":\"no sensor\"}"); return; }
    calPH4Voltage = ph->readRawVoltage();
    ph->setCalibration(calPH7Voltage, calPH4Voltage);
    ph->saveCalibration(calibrationData);
    calibrationData.save();
    calState = CAL_PH_LOCKED_4; calResetWindow();
    StaticJsonDocument<128> doc;
    doc["ok"] = true; doc["slope"] = calibrationData.phSlope; doc["intercept"] = calibrationData.phIntercept;
    String json; serializeJson(doc, json); webServer.send(200, "application/json", json);
}

void handleCalStartORP() {
    ORPSensor* orp = sensorManager ? sensorManager->getORPSensor() : nullptr;
    if (!orp || !orp->isConnected()) { webServer.send(400, "application/json", "{\"error\":\"ORP sensor not connected\"}"); return; }
    calState = CAL_ORP_WAIT; calResetWindow();
    webServer.send(200, "application/json", "{\"ok\":true}");
}

void handleCalLockORP() {
    ORPSensor* orp = sensorManager ? sensorManager->getORPSensor() : nullptr;
    if (!orp) { webServer.send(400, "application/json", "{\"error\":\"no sensor\"}"); return; }
    if (webServer.hasArg("ref")) { float ref = webServer.arg("ref").toFloat(); if (ref >= 100 && ref <= 900) calORPRequiredMV = ref; }
    orp->setCalibration(calORPRequiredMV);
    orp->saveCalibration(calibrationData);
    calibrationData.save();
    calState = CAL_ORP_LOCKED; calResetWindow();
    StaticJsonDocument<64> doc;
    doc["ok"] = true; doc["offset"] = calibrationData.orpOffset;
    String json; serializeJson(doc, json); webServer.send(200, "application/json", json);
}

void handleCalReset() {
    bool cancelOnly = webServer.hasArg("cancel");
    calState = CAL_IDLE; calResetWindow();
    if (!cancelOnly) {
        calibrationData = CalibrationData(); calibrationData.save();
        PHSensor* ph = sensorManager ? sensorManager->getPHSensor() : nullptr;
        ORPSensor* orp = sensorManager ? sensorManager->getORPSensor() : nullptr;
        if (ph) ph->loadCalibration(calibrationData);
        if (orp) orp->loadCalibration(calibrationData);
    }
    webServer.send(200, "application/json", "{\"ok\":true}");
}
