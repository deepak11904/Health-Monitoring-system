#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <WebServer.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

// WiFi credentials - use these consistently
#define WIFI_SSID "vivo1909"
#define WIFI_PASSWORD "1234567890"

#define BOT_TOKEN "7687254840:AAEIU-Va4qItVXrh90uz16rFQrkNfr8SYHw"
#define CHAT_ID "5440952657"

#define PULSE_SENSOR_PIN 34  
#define NUM_READINGS 5  
#define THRESHOLD 500

// WiFi connection timeout in milliseconds
#define WIFI_TIMEOUT 20000

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WebServer server(80);

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
bool mlx90614_found = false;
uint32_t lastUpdate = 0;

int pulseReadings[NUM_READINGS] = {0};
int readIndex = 0, total = 0, filteredPulse = 0;
bool pulseDetected = false;

// Improved WiFi connection function with proper error handling
bool connectToWiFi(const char* ssid, const char* password, int timeout = WIFI_TIMEOUT) {
  Serial.println("\n🔌 Connecting to WiFi...");
  
  WiFi.mode(WIFI_STA);  // Set WiFi to Station mode
  WiFi.begin(ssid, password);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && 
         millis() - startAttemptTime < timeout) {
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to WiFi!");
    Serial.println("📎 Troubleshooting tips:");
    Serial.println("   - Check SSID and password");
    Serial.println("   - Make sure router is on");
    Serial.println("   - Try moving closer to router");
    Serial.println("   - WiFi status code: " + String(WiFi.status()));
    return false;
  }
}

void updatePulseSensor() {
    int pulseValue = analogRead(PULSE_SENSOR_PIN);
    if (pulseValue > THRESHOLD) {
        total -= pulseReadings[readIndex];  
        pulseReadings[readIndex] = pulseValue;
        total += pulseReadings[readIndex];  
        readIndex = (readIndex + 1) % NUM_READINGS;  
        filteredPulse = total / NUM_READINGS;  
        pulseDetected = true;
    } else {
        pulseDetected = false;
    }
}

int medianFilter() {
    int sorted[NUM_READINGS];
    memcpy(sorted, pulseReadings, NUM_READINGS * sizeof(int));
    for (int i = 0; i < NUM_READINGS - 1; i++) {
        for (int j = i + 1; j < NUM_READINGS; j++) {
            if (sorted[i] > sorted[j]) {
                int temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    return sorted[NUM_READINGS / 2];
}

void sendTelegramMessage() {
    float temp = mlx90614_found ? mlx.readObjectTempC() : -1.0;
    int bpm = pulseDetected ? map(medianFilter(), 500, 3000, 60, 100) : 0;

    String message = "📊 *Health Monitoring Report* \n\n";
    
    message += "👤 *Patient:* Deepak Pandey\n";
    message += "🆔 *ID:* PAT-2025-0042\n";
    message += "📋 *Details:* Male,19 years, 75kg\n";
    message += "🏥 *Condition:* Post-operative monitoring\n\n";
    
    message += "📈 *Current Vital Signs:*\n";
    
    if (mlx90614_found) {
        String tempStatus = "Normal";
        if (temp > 38.0) tempStatus = "⚠️ HIGH";
        else if (temp > 37.5) tempStatus = "⚠️ Elevated";
        
        message += "🌡 *Body Temp:* " + String(temp, 1) + " °C (" + tempStatus + ")\n";
    } else {
        message += "⚠️ *Temp Sensor Not Detected*\n";
    }
    
    if (pulseDetected) {
        String bpmStatus = "Normal";
        if (bpm > 100) bpmStatus = "⚠️ Tachycardia";
        else if (bpm < 60) bpmStatus = "⚠️ Bradycardia";
        
        message += "💓 *Heart Rate:* " + String(bpm) + " BPM (" + bpmStatus + ")\n";
    } else {
        message += "💓 *Heart Rate:* No pulse detected\n";
    }
    
    message += "🩸 *Blood Pressure:* 120/80 mmHg (Normal)\n";
    message += "🫁 *SpO₂:* 98%\n\n";
    
    message += "⏱ *Last Updated:* " + String(__TIME__) + "\n";
    message += "📱 *Device IP:* " + WiFi.localIP().toString();
    
    bot.sendMessage(CHAT_ID, message, "Markdown");
}

void sendWelcomeMessage() {
    String welcomeText = "👋 *Welcome to the Health Monitoring Bot!* \n"
                         "Click below to check your real-time health status.";

    String keyboard = "[[{\"text\":\"Check Health\",\"callback_data\":\"/health\"}]]";
    bot.sendMessageWithInlineKeyboard(CHAT_ID, welcomeText, "Markdown", keyboard);
}

// API endpoint for sending health report
void handleSendReport() {
    sendTelegramMessage();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Health report sent to Telegram!\"}");
}

// Web Server Handlers
void handleRoot() {
    float temp = mlx90614_found ? mlx.readObjectTempC() : -1.0;
    int bpm = pulseDetected ? map(medianFilter(), 500, 3000, 60, 100) : 0;

    String html = "<html><head><meta http-equiv='refresh' content='5'>";
    html += "<meta charset='UTF-8'>"; // Add UTF-8 charset
    html += "<title>Medical Health Dashboard</title>";
    html += "<style>";
    html += "body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 0; background-color: #f5f7fa; color: #333; }";
    html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
    html += ".header { background-color: #2c3e50; color: white; padding: 15px 20px; border-radius: 8px 8px 0 0; display: flex; justify-content: space-between; align-items: center; }";
    html += ".header h1 { margin: 0; font-size: 24px; }";
    html += ".header .status { font-size: 14px; }";
    html += ".dashboard { display: flex; flex-wrap: wrap; gap: 20px; margin-top: 20px; }";
    html += ".patient-card { background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); overflow: hidden; flex: 1; min-width: 300px; }";
    html += ".patient-info { display: flex; padding: 20px; border-bottom: 1px solid #eee; }";
    html += ".patient-image { width: 100px; height: 100px; border-radius: 50%; background-color: #ddd; margin-right: 20px; background-image: url('/api/placeholder/100/100'); background-size: cover; }";
    html += ".patient-details h2 { margin: 0 0 10px 0; color: #2c3e50; }";
    html += ".patient-details p { margin: 5px 0; color: #7f8c8d; }";
    html += ".vitals { padding: 20px; }";
    html += ".vitals h3 { margin: 0 0 15px 0; color: #2c3e50; }";
    html += ".vital-signs { display: flex; flex-wrap: wrap; gap: 15px; }";
    html += ".vital-box { flex: 1; min-width: 120px; background: #f8f9fa; border-radius: 6px; padding: 15px; text-align: center; }";
    html += ".vital-box.warning { background: #fff3cd; }";
    html += ".vital-box.danger { background: #f8d7da; }";
    html += ".vital-box.good { background: #d4edda; }";
    html += ".vital-box h4 { margin: 0 0 10px 0; font-size: 14px; color: #6c757d; }";
    html += ".vital-box .value { font-size: 24px; font-weight: bold; margin-bottom: 5px; }";
    html += ".vital-box .icon { font-size: 20px; margin-bottom: 10px; }";
    html += ".footer { margin-top: 20px; text-align: center; color: #7f8c8d; font-size: 12px; }";
    html += ".chart-container { padding: 20px; background: white; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); margin-top: 20px; }";
    html += ".chart-container h3 { margin: 0 0 15px 0; color: #2c3e50; }";
    html += ".chart { height: 200px; background: #f8f9fa; border-radius: 6px; display: flex; align-items: center; justify-content: center; color: #6c757d; }";
    html += ".bpm-meter { position: relative; width: 100%; height: 30px; background: #f8f9fa; border-radius: 15px; overflow: hidden; margin-top: 10px; }";
    html += ".bpm-value { height: 100%; background: linear-gradient(to right, #4caf50, #ffc107, #f44336); width: 0%; transition: width 0.5s ease; }";
    html += ".temp-meter { position: relative; width: 100%; height: 30px; background: #f8f9fa; border-radius: 15px; overflow: hidden; margin-top: 10px; }";
    html += ".temp-value { height: 100%; background: linear-gradient(to right, #2196f3, #f44336); width: 0%; transition: width 0.5s ease; }";
    
    // Add popup styles
    html += ".popup { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background-color: rgba(0,0,0,0.5); z-index: 1000; justify-content: center; align-items: center; }";
    html += ".popup-content { background-color: white; padding: 20px; border-radius: 8px; box-shadow: 0 4px 15px rgba(0,0,0,0.2); text-align: center; max-width: 400px; width: 90%; }";
    html += ".popup-content h3 { margin-top: 0; color: #2c3e50; }";
    html += ".popup-content .success-icon { font-size: 48px; color: #4caf50; margin: 10px 0; }";
    html += ".popup-content .error-icon { font-size: 48px; color: #f44336; margin: 10px 0; }";
    html += ".popup-content .button { background: #2c3e50; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; font-size: 14px; margin-top: 10px; }";
    
    html += "</style>";
    
    // Add JavaScript for AJAX request and popup handling
    html += "<script>";
    html += "function sendHealthReport() {";
    html += "  const popup = document.getElementById('popup');";
    html += "  const popupMessage = document.getElementById('popup-message');";
    html += "  const popupIcon = document.getElementById('popup-icon');";
    html += "  const popupTitle = document.getElementById('popup-title');";
    
    html += "  fetch('/api/sendReport')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      popupTitle.textContent = 'Success!';";
    html += "      popupMessage.textContent = data.message;";
    html += "      popupIcon.textContent = '✅';";
    html += "      popupIcon.className = 'success-icon';";
    html += "      popup.style.display = 'flex';";
    html += "    })";
    html += "    .catch(error => {";
    html += "      popupTitle.textContent = 'Error';";
    html += "      popupMessage.textContent = 'Failed to send report. Please try again.';";
    html += "      popupIcon.textContent = '❌';";
    html += "      popupIcon.className = 'error-icon';";
    html += "      popup.style.display = 'flex';";
    html += "    });";
    html += "  return false;"; // Prevent default link behavior
    html += "}";
    
    html += "function closePopup() {";
    html += "  document.getElementById('popup').style.display = 'none';";
    html += "}";
    html += "</script>";
    
    html += "</head><body>";
    
    // Popup dialog
    html += "<div id='popup' class='popup' onclick='closePopup()'>";
    html += "  <div class='popup-content' onclick='event.stopPropagation()'>";
    html += "    <h3 id='popup-title'>Success!</h3>";
    html += "    <div id='popup-icon' class='success-icon'>✅</div>";
    html += "    <p id='popup-message'>Health report sent to Telegram!</p>";
    html += "    <button class='button' onclick='closePopup()'>Close</button>";
    html += "  </div>";
    html += "</div>";
    
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>Medical Health Dashboard</h1>";
    html += "<div class='status'>📡 Connected | Last Updated: " + String(__TIME__) + "</div>";
    html += "</div>";
    
    html += "<div class='dashboard'>";
    html += "<div class='patient-card'>";
    html += "<div class='patient-info'>";
    html += "<div class='patient-image'></div>";
    html += "<div class='patient-details'>";
    html += "<h2>Deepak Pandey</h2>";
    html += "<p><strong>ID:</strong> PAT-2025-0042</p>";
    html += "<p><strong>Age:</strong> 22 | <strong>Sex:</strong> Male</p>";
    html += "<p><strong>Weight:</strong> 65 kg | <strong>Height:</strong> 178 cm</p>";
    html += "<p><strong>Condition:</strong> Post-operative monitoring</p>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='vitals'>";
    html += "<h3>Current Vital Signs</h3>";
    html += "<div class='vital-signs'>";
    
    // Temperature vital box with dynamic class based on value
    String tempClass = "vital-box";
    if (mlx90614_found) {
        if (temp > 38.0) {
            tempClass = "vital-box danger";
        } else if (temp > 37.5) {
            tempClass = "vital-box warning";
        } else {
            tempClass = "vital-box good";
        }
        
        html += "<div class='" + tempClass + "'>";
        html += "<h4>TEMPERATURE</h4>";
        html += "<div class='icon'>🌡️</div>";
        html += "<div class='value'>" + String(temp, 1) + " °C</div>";
        int tempWidth = constrain(map(temp*10, 340, 400, 0, 100), 0, 100);
        html += "<div class='temp-meter'><div class='temp-value' style='width: " + String(tempWidth) + "%;'></div></div>";
        html += "</div>";
    } else {
        html += "<div class='vital-box warning'>";
        html += "<h4>TEMPERATURE</h4>";
        html += "<div class='icon'>⚠️</div>";
        html += "<div class='value'>N/A</div>";
        html += "<div>Sensor not found</div>";
        html += "</div>";
    }
    
    // Heart rate vital box with dynamic class based on value
    String bpmClass = "vital-box";
    if (pulseDetected) {
        if (bpm > 100) {
            bpmClass = "vital-box danger";
        } else if (bpm < 60) {
            bpmClass = "vital-box warning";
        } else {
            bpmClass = "vital-box good";
        }
        
        html += "<div class='" + bpmClass + "'>";
        html += "<h4>HEART RATE</h4>";
        html += "<div class='icon'>💓</div>";
        html += "<div class='value'>" + String(bpm) + " BPM</div>";
        int bpmWidth = constrain(map(bpm, 40, 140, 0, 100), 0, 100);
        html += "<div class='bpm-meter'><div class='bpm-value' style='width: " + String(bpmWidth) + "%;'></div></div>";
        html += "</div>";
    } else {
        html += "<div class='vital-box warning'>";
        html += "<h4>HEART RATE</h4>";
        html += "<div class='icon'>💤</div>";
        html += "<div class='value'>0 BPM</div>";
        html += "<div>No pulse detected</div>";
        html += "</div>";
    }
    
    // Blood Pressure (Placeholder)
    html += "<div class='vital-box'>";
    html += "<h4>BLOOD PRESSURE</h4>";
    html += "<div class='icon'>🩸</div>";
    html += "<div class='value'>120/80</div>";
    html += "<div>Last measured: 30m ago</div>";
    html += "</div>";
    
    // SpO2 (Placeholder)
    html += "<div class='vital-box good'>";
    html += "<h4>SpO₂</h4>";
    html += "<div class='icon'>🫁</div>";
    html += "<div class='value'>98%</div>";
    html += "<div>Normal range</div>";
    html += "</div>";
    
    html += "</div>"; // vital-signs
    html += "</div>"; // vitals
    html += "</div>"; // patient-card
    
    // System Status Card
    html += "<div class='patient-card'>";
    html += "<div class='vitals'>";
    html += "<h3>System Status</h3>";
    html += "<div class='vital-signs'>";
    
    html += "<div class='vital-box good'>";
    html += "<h4>WIFI STATUS</h4>";
    html += "<div class='icon'>📡</div>";
    html += "<div class='value'>Connected</div>";
    html += "<div>" + WiFi.localIP().toString() + "</div>";
    html += "</div>";
    
    String tempSensorClass = mlx90614_found ? "vital-box good" : "vital-box warning";
    html += "<div class='" + tempSensorClass + "'>";
    html += "<h4>TEMP SENSOR</h4>";
    html += "<div class='icon'>" + String(mlx90614_found ? "✅" : "⚠️") + "</div>";
    html += "<div class='value'>" + String(mlx90614_found ? "Online" : "Offline") + "</div>";
    html += "<div>" + String(mlx90614_found ? "MLX90614 Connected" : "Check Connection") + "</div>";
    html += "</div>";
    
    String pulseSensorClass = pulseDetected ? "vital-box good" : "vital-box warning";
    html += "<div class='" + pulseSensorClass + "'>";
    html += "<h4>PULSE SENSOR</h4>";
    html += "<div class='icon'>" + String(pulseDetected ? "✅" : "⚠️") + "</div>";
    html += "<div class='value'>" + String(pulseDetected ? "Active" : "Inactive") + "</div>";
    html += "<div>" + String(pulseDetected ? "Reading Data" : "No Signal") + "</div>";
    html += "</div>";
    
    // Memory status
    html += "<div class='vital-box good'>";
    html += "<h4>MEMORY</h4>";
    html += "<div class='icon'>🧠</div>";
    html += "<div class='value'>" + String(ESP.getFreeHeap() / 1024) + " KB</div>";
    html += "<div>Free Memory</div>";
    html += "</div>";
    
    html += "</div>"; // vital-signs
    
    // Quick Actions - Updated to use JavaScript function instead of direct link
    html += "<h3 style='margin-top: 20px;'>Quick Actions</h3>";
    html += "<div style='display: flex; gap: 10px;'>";
    html += "<a href='#' onclick='return sendHealthReport()' style='flex: 1; text-decoration: none;'><div style='background: #2c3e50; color: white; text-align: center; padding: 12px; border-radius: 6px;'>Send Report to Telegram</div></a>";
    html += "<a href='/' style='flex: 1; text-decoration: none;'><div style='background: #7f8c8d; color: white; text-align: center; padding: 12px; border-radius: 6px;'>Refresh Dashboard</div></a>";
    html += "</div>";
    
    html += "</div>"; // vitals
    html += "</div>"; // patient-card
    html += "</div>"; // dashboard
    
    // Chart placeholder
    html += "<div class='chart-container'>";
    html += "<h3>Vital Signs History (Last 24 Hours)</h3>";
    html += "<div class='chart'>Real-time charts would appear here in a full implementation</div>";
    html += "</div>";
    
    html += "<div class='footer'>";
    html += "Medical Health Monitoring System v1.0 | Auto-refresh: 5 seconds | &copy; 2025";
    html += "</div>";
    
    html += "</div>"; // container
    html += "</body></html>";

    server.send(200, "text/html", html);
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Give serial monitor time to start
    
    Serial.println("\n\n=== Health Monitoring System Starting ===");
    
    // Try to connect to WiFi with proper error handling
    bool wifiConnected = connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
    
    if (!wifiConnected) {
        Serial.println("⚠️ Continuing with limited functionality");
        // You could add a restart option here if desired:
        // ESP.restart();
    } else {
        // Only set up secured client and send welcome message if WiFi is connected
        secured_client.setInsecure();
        sendWelcomeMessage();
    }

    delay(2000); // Brief delay before initializing sensors
    
    Wire.begin(21, 22);
    if (mlx.begin()) {
        Serial.println("✅ MLX90614 Temperature Sensor Initialized");
        mlx90614_found = true;
    } else {
        Serial.println("❌ MLX90614 NOT FOUND! Check wiring connections to I2C pins 21 & 22");
    }
    
    pinMode(PULSE_SENSOR_PIN, INPUT);
    analogReadResolution(12);
    Serial.println("✅ Pulse Sensor Initialized (Pin 34)");

    // Only start the web server if WiFi connected
    if (wifiConnected) {
        server.on("/", handleRoot);
        server.on("/api/sendReport", handleSendReport);
        server.begin();
        Serial.println("🌍 Web server started at " + WiFi.localIP().toString());
    }
    
    Serial.println("=== Setup Complete ===\n");
}

void loop() {
    updatePulseSensor();
    
    // Only handle server clients if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    } else {
        // Try to reconnect to WiFi if disconnected
        static unsigned long lastWiFiCheck = 0;
        if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
            lastWiFiCheck = millis();
            connectToWiFi(WIFI_SSID, WIFI_PASSWORD, 5000); // Short timeout for reconnection attempts
        }
    }

    // Declare temp and bpm at the beginning of loop()
    float temp = -1.0;
    int bpm = 0;

    if (millis() - lastUpdate > 5000) {  // Update every 5 seconds
        lastUpdate = millis();
        
        Serial.println("========== Health Monitoring System Status ==========");

        // WiFi status report
        Serial.print("📡 WiFi Status: ");
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected (" + WiFi.localIP().toString() + ")");
        } else {
            Serial.println("Disconnected (status code: " + String(WiFi.status()) + ")");
        }

        if (mlx90614_found) {
            temp = mlx.readObjectTempC();
            Serial.print("🌡️ Body Temperature: "); 
            Serial.print(temp); 
            Serial.println(" °C");
        } else {
            Serial.println("⚠️ MLX90614 Temperature Sensor Not Found!");
        }

        if (pulseDetected) {
            bpm = map(medianFilter(), 500, 3000, 60, 100);
            Serial.print("💓 Heart Rate: "); 
            Serial.print(bpm); 
            Serial.println(" BPM");
        } else {
            Serial.println("💤 No Pulse Detected (0 BPM)");
        }

        Serial.println("=====================================================\n");
    }

    // Only check for Telegram messages if WiFi is connected
    if (WiFi.status() == WL_CONNECTED) {
        int newMessages = bot.getUpdates(bot.last_message_received + 1);
        for (int i = 0; i < newMessages; i++) {
            String message_text = bot.messages[i].text;
            if (message_text == "/health") {
                sendTelegramMessage();
            }
        }
    }
}