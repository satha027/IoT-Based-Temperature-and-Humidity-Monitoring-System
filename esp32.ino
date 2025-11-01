/*
 * ESP32 DHT11 Temp/Humidity Monitor
 * * Features:
 * 1. Hosts a web server with a full HTML/CSS/JS interface.
 * 2. Provides a /data JSON endpoint for AJAX updates.
 * 3. Displays readings on a 1.3" I2C OLED display (128x64).
 * 4. Reads from a DHT11 sensor.
 */

// --- Wi-Fi and Web Server Libraries ---
#include <WiFi.h>
#include <WebServer.h>

// --- JSON Library ---
#include <ArduinoJson.h>

// --- DHT Sensor Libraries ---
#include <DHT.h>
#include <DHT_U.h>

// --- OLED Display Libraries ---
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Configuration ---

// 1. Wi-Fi Credentials
const char* ssid = "YOUR_SSID";         // <-- REPLACE with your Wi-Fi SSID
const char* password = "YOUR_PASSWORD"; // <-- REPLACE with your Wi-Fi password

// 2. Sensor Configuration
#define DHT_PIN 4       // Pin connected to DHT11 data
#define DHT_TYPE DHT11  // We are using a DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// 3. OLED Display Configuration
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // I2C address (common for 128x64)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Global Variables ---
WebServer server(80); // Create web server object on port 80

// Global variables to store sensor readings
float temperature = 0.0;
float humidity = 0.0;

// Timer for non-blocking sensor reads
unsigned long lastReadTime = 0;
const long readInterval = 2000; // Read sensor every 2 seconds

// --- Web Page HTML (as a raw literal) ---
// This is the complete HTML/CSS/JS file from the previous answer
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Temp & Humidity Monitor</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #f0f8ff; /* Light blue theme */
            color: #333;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        header { width: 100%; text-align: center; margin-bottom: 30px; }
        header h1 { color: #0056b3; font-size: 2.5rem; }
        .cards-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 30px;
            width: 100%;
            max-width: 900px;
        }
        .card {
            background-color: #ffffff;
            border-radius: 12px;
            box-shadow: 0 6px 12px rgba(0, 0, 0, 0.08);
            padding: 25px;
            width: 300px;
            text-align: center;
            transition: transform 0.2s;
        }
        .card:hover { transform: translateY(-5px); }
        .card h2 { font-size: 1.75rem; color: #003366; margin-bottom: 20px; }
        .card .reading { font-size: 3.5rem; font-weight: 600; line-height: 1; }
        .card .unit { font-size: 1.5rem; color: #555; margin-left: 5px; }
        .temp-cool { color: #0077b6; }
        .temp-norm { color: #2b9348; }
        .temp-hot { color: #d00000; }
        .hum-value { color: #00a0a0; }
        @media (max-width: 680px) {
            header h1 { font-size: 2rem; }
            .cards-container { flex-direction: column; align-items: center; gap: 20px; }
            .card { width: 90%; }
            .card .reading { font-size: 3rem; }
        }
    </style>
</head>
<body>
    <header>
        <h1>ESP32 Temperature & Humidity Monitor</h1>
    </header>
    <main class="cards-container">
        <div class="card temperature">
            <h2>🌡️ Temperature</h2>
            <p class="reading">
                <span id="temp-value" class="temp-norm">--</span>
                <span class="unit">&deg;C</span>
            </p>
        </div>
        <div class="card humidity">
            <h2>💧 Humidity</h2>
            <p class="reading">
                <span id="hum-value" class="hum-value">--</span>
                <span class="unit">%RH</span>
            </p>
        </div>
    </main>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            function fetchData() {
                fetch('/data')
                    .then(response => response.json())
                    .then(data => {
                        const tempElement = document.getElementById('temp-value');
                        const humElement = document.getElementById('hum-value');
                        
                        const temp = parseFloat(data.temperature).toFixed(1);
                        const hum = parseFloat(data.humidity).toFixed(1);

                        tempElement.textContent = temp;
                        humElement.textContent = hum;

                        tempElement.classList.remove('temp-cool', 'temp-norm', 'temp-hot');
                        if (temp > 30) {
                            tempElement.classList.add('temp-hot');
                        } else if (temp < 18) {
                            tempElement.classList.add('temp-cool');
                        } else {
                            tempElement.classList.add('temp-norm');
                        }
                    })
                    .catch(error => {
                        console.error('Error fetching data:', error);
                        document.getElementById('temp-value').textContent = 'Err';
                        document.getElementById('hum-value').textContent = 'Err';
                    });
            }
            fetchData(); // Initial fetch
            setInterval(fetchData, 5000); // Fetch every 5 seconds
        });
    </script>
</body>
</html>
)rawliteral";


// --- Setup Function ---
void setup() {
    Serial.begin(115200);
    
    // --- Initialize OLED ---
    Wire.begin(); // Start I2C (SDA = 21, SCL = 22)
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Connecting to WiFi...");
    display.display();

    // --- Initialize WiFi ---
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        display.print(".");
        display.display();
    }
    Serial.println("\nWiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // --- Show IP on OLED ---
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connected!");
    display.println("IP Address:");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);

    // --- Initialize DHT Sensor ---
    dht.begin();

    // --- Define Web Server Routes ---
    // 1. Root route: serves the HTML page
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", index_html);
    });

    // 2. /data route: serves the JSON data
    server.on("/data", HTTP_GET, []() {
        // Create a JSON document
        StaticJsonDocument<128> doc;
        
        // Add values
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        
        // Serialize JSON
        String jsonOutput;
        serializeJson(doc, jsonOutput);
        
        // Send response
        server.send(200, "application/json", jsonOutput);
    });

    // 3. Not Found route
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not Found");
    });

    // --- Start Server ---
    server.begin();
    Serial.println("Web server started.");

    // Perform an initial sensor read
    readSensorData();
    updateOledDisplay();
}

// --- Main Loop ---
void loop() {
    // Handle web server client requests
    server.handleClient();

    // Non-blocking sensor read
    if (millis() - lastReadTime > readInterval) {
        readSensorData();
        updateOledDisplay();
        lastReadTime = millis();
    }
}

// --- Helper Functions ---

void readSensorData() {
    float h = dht.readHumidity();
    float t = dht.readTemperature(); // Read temperature as Celsius

    // Check if any reads failed and exit early (to keep old values)
    if (isnan(h) || isnan(t)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        // Keep the old values
    } else {
        // Update global variables
        temperature = t;
        humidity = h;
        Serial.printf("Temperature: %.1f C, Humidity: %.1f %%\n", temperature, humidity);
    }
}

void updateOledDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    // Title
    display.setCursor(0, 0);
    display.println("ESP32 Monitor");
    display.println("--------------");
    
    // Temperature
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.printf("T: %.1f C\n", temperature);
    
    // Humidity
    display.setCursor(0, 44);
    display.printf("H: %.1f %%\n", humidity);
    
    // Show on display
    display.display();
}
