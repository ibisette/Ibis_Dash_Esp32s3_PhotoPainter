// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║                    🦤 IBIS DASH - USER READY VERSION 🦤                      ║
// ║                                                                              ║
// ║  VERSION 4.0 - USB COMPOSITE DEVICE (PC-FRIENDLY)                            ║
// ║  • NEW: Board identifies as "Ibis Dash" in Device Manager!                   ║
// ║  • NEW: USB Composite Device (CDC + HID) prevents PC power management        ║
// ║  • PCs treat HID devices as essential - no more USB disconnects!             ║
// ║  • NEVER sleeps on USB - foolproof triple verification                       ║
// ║  • Ignores false low battery readings on USB                                 ║
// ║                                                                              ║
// ║  This version has NO personal credentials - ready for end users!             ║
// ║  All configuration is stored in NVS (flash) and set via Ibis Setup app.      ║
// ║                                                                              ║
// ║  FIRST TIME SETUP:                                                           ║
// ║  1. Upload this sketch to your ESP32-S3-PhotoPainter                         ║
// ║  2. You'll see the setup screen with ibis logos                              ║
// ║  3. Press KEY button on back to enter pairing mode (30 min)                  ║
// ║  4. Open Ibis Setup app, connect, and configure WiFi + Strava                ║
// ║  5. Save - the board becomes a Strava dashboard!                             ║
// ║                                                                              ║
// ║  USB IDENTITY (how PCs see this board):                                      ║
// ║  • Product Name: "Ibis Dash"                                                 ║
// ║  • Manufacturer: "Ibis"                                                      ║
// ║  • Shows as composite device: USBSerial Port + HID                           ║
// ║  • PCs will NOT aggressively power-manage this device                        ║
// ║                                                                              ║
// ║  FEATURES:                                                                   ║
// ║  + Setup screen with pixel art ibis logos when unconfigured                  ║
// ║  + Wakes from sleep when USB connected (for easy editing!)                   ║
// ║  + NEVER sleeps on USB - foolproof triple verification                       ║
// ║  + Configurable refresh intervals (hourly/12h/daily/weekly)                  ║
// ║  + Configurable tracking period (weekly/monthly/yearly)                      ║
// ║  + Configurable sport type (Run/Ride/Swim/Hike/Walk)                         ║
// ║  + Custom title/username support                                             ║
// ║  + Weekly stats: Monday to Sunday                                            ║
// ║  + Wipe command to clear all user data                                       ║
// ║                                                                              ║
// ║  ARDUINO IDE SETTINGS:                                                       ║
// ║  >> Board: ESP32S3 Dev Module                                                ║
// ║  >> Flash Mode: DIO (NOT OPI!)                                               ║
// ║  >> USB CDC On Boot: "Disabled"                                              ║
// ║  >> USB Mode: "USB-OTG (TinyUSB)"                                            ║
// ║                                                                              ║
// ║  Hardware: Waveshare ESP32-S3-PhotoPainter                                   ║
// ║  ESP32 Board Version: 2.0.17                                                 ║
// ╚══════════════════════════════════════════════════════════════════════════════╝

// =============================================================================
// SECTION 1: LIBRARY INCLUDES
// =============================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <GxEPD2_7C.h>

// Custom fonts
#include <Fonts/fonnts_com_Maison_Neue_Bold9pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Bold18pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Bold24pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light9pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light15pt7b.h>
#include <Fonts/fonnts_com_Maison_Neue_Light18pt7b.h>

#include <Wire.h>
#include "XPowersLib.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_task_wdt.h"
#include "esp_int_wdt.h"
#include <nvs_flash.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <vector>
#include <Preferences.h>

// USB Composite Device - Makes board identify as "Ibis Dash" to PCs
// CDC (serial) + HID (prevents aggressive USB power management)
#include "USB.h"
#include "USBHID.h"
#include "USBCDC.h"

// In USB-OTG (TinyUSB) mode with "USB CDC On Boot: Disabled",
// the core does NOT auto-create a USBSerial object.
// We declare our own USBCDC instance here. All serial communication
// in this sketch uses USBSerial so it goes through the USB COM port.
USBCDC USBSerial;

// Include the ibis logo data (place ibis_logos.h in same folder as this .ino)
#include "ibis_logos.h"


// =============================================================================
// SECTION 2: HARDWARE PIN DEFINITIONS
// =============================================================================

#define PMU_SDA 47
#define PMU_SCL 48
#define PMU_IRQ 21

static const int EPD_BUSY = 13;
static const int EPD_RST  = 12;
static const int EPD_DC   = 8;
static const int EPD_CS   = 9;
static const int EPD_SCK  = 10;
static const int EPD_MOSI = 11;
static const int EPD_MISO = -1;

static const int ACT_LED_PIN = 42;
static const int BOOT_BUTTON_PIN = 0;
static const int USER_BUTTON_PIN = 4;  // KEY button
static const int PWR_BUTTON_PIN = 5;


// =============================================================================
// SECTION 3: CONFIGURATION CONSTANTS
// =============================================================================

static const float LOW_BATTERY_THRESHOLD = 15.0;
static const int W = 800;
static const int H = 480;
static const int WATCHDOG_TIMEOUT_SECONDS = 120;
static const int CRASH_LOOP_THRESHOLD = 3;
static const int FACTORY_RESET_HOLD_MS = 5000;

// Pairing mode duration
#define PAIRING_MODE_DURATION_MS (30 * 60 * 1000UL)  // 30 minutes

// Default sleep durations (can be overridden by user settings)
#define SLEEP_DURATION_UNCONFIGURED_US (7 * 24 * 60 * 60 * 1000000ULL)  // 1 week
#define DISPLAY_REFRESH_UNCONFIGURED_MS (5 * 60 * 1000UL)              // 5 minutes

#define uS_TO_S_FACTOR 1000000ULL

// Tracking period options
#define TRACK_YEARLY   0
#define TRACK_MONTHLY  1
#define TRACK_WEEKLY   2

// Sport type options
#define SPORT_RUN   "Run"
#define SPORT_RIDE  "Ride"
#define SPORT_SWIM  "Swim"
#define SPORT_HIKE  "Hike"
#define SPORT_WALK  "Walk"


// =============================================================================
// SECTION 4: RTC MEMORY VARIABLES (survive deep sleep)
// =============================================================================

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR bool wasManualWake = false;
RTC_DATA_ATTR time_t lastStravaFetchEpoch = 0;
RTC_DATA_ATTR time_t lastNtpSyncEpoch = 0;  // Track when we last synced NTP
RTC_DATA_ATTR int dashYear = 0;
RTC_DATA_ATTR int dashMonth = 0;
RTC_DATA_ATTR int dashWeek = 0;
RTC_DATA_ATTR float kmDone = 0;
RTC_DATA_ATTR float timeHours = 0;
RTC_DATA_ATTR int activitiesCount = 0;
RTC_DATA_ATTR int rapidBootCount = 0;

// Token caching - survives deep sleep
RTC_DATA_ATTR char cachedAccessToken[256] = {0};
RTC_DATA_ATTR time_t tokenExpiresAt = 0;


// =============================================================================
// SECTION 5: GLOBAL VARIABLES
// =============================================================================

XPowersAXP2101 PMU;
Preferences preferences;
String serialInputBuffer = "";

GxEPD2_7C<GxEPD2_730c_GDEP073E01, GxEPD2_730c_GDEP073E01::HEIGHT> display(
  GxEPD2_730c_GDEP073E01(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ===== USB COMPOSITE DEVICE =====
// Minimal HID report descriptor - presents as a "vendor defined" HID device
// This is intentionally a no-op: it never sends reports, it just exists
// so Windows treats the whole USB composite as a HID device and refuses
// to power-manage it aggressively (Windows never suspends keyboards/mice/HID)
static const uint8_t ibisHidReportDescriptor[] = {
  0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
  0x09, 0x01,        // Usage (Vendor Usage 1)
  0xA1, 0x01,        // Collection (Application)
  0x09, 0x01,        //   Usage (Vendor Usage 1)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x00,  //   Logical Maximum (255)
  0x75, 0x08,        //   Report Size (8)
  0x95, 0x01,        //   Report Count (1)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  0xC0               // End Collection
};

// HID device instance
USBHID ibishid;
bool usbCompositeStarted = false;

// ===== USER CONFIGURATION (loaded from NVS, set via Ibis Setup app) =====
// These start BLANK - user must configure via app
String WIFI_SSID = "";
String WIFI_PASS = "";
String CLIENT_ID = "";
String CLIENT_SECRET = "";
String REFRESH_TOKEN = "";
String USER_NAME = "";
String SPORT_TYPE = "Run";
String CUSTOM_TITLE = "";        // Custom title for header (optional)
float YEARLY_GOAL = 1000.0;
int REFRESH_HOURS = 12;          // Refresh interval in hours
int TRACK_PERIOD = TRACK_YEARLY; // 0=yearly, 1=monthly, 2=weekly

// Runtime state
String accessToken = "";
String lastTitle = "";
String lastLine = "";
String lastPolyline = "";
float lastDistKm = 0;        // Last activity distance in km
int lastMovingSecs = 0;       // Last activity moving time in seconds
String lastDateStr = "";      // Last activity date e.g. "2 January"
bool isUsbConnected = false;
bool wasUsbConnected = false;
float batteryPercentage = 0.0;
float batteryVoltage = 0.0;
bool lowBatteryMode = false;
bool isCharging = false;
String lastUpdateTime = "";
unsigned long lastDisplayRefresh = 0;
unsigned long lastStravaCheck = 0;
uint16_t C_HEADER = GxEPD_RED;
uint16_t C_TEXT_DIM = GxEPD_RED;
bool inSafeMode = false;

// Setup state - determined by checking if WiFi credentials exist
bool isConfigured = false;

// Sleep control flag - set by serial commands to trigger sleep after command completes
bool sleepRequested = false;


// =============================================================================
// SECTION 6: POLYLINE DECODING
// =============================================================================

struct Point { 
  float lat; 
  float lon; 
};

std::vector<Point> decodePolyline(String encoded) {
  std::vector<Point> points;
  int index = 0, len = encoded.length();
  int lat = 0, lng = 0;

  while (index < len) {
    int b, shift = 0, result = 0;
    do { 
      b = encoded[index++] - 63; 
      result |= (b & 0x1f) << shift; 
      shift += 5; 
    } while (b >= 0x20);
    int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lat += dlat;

    shift = 0; 
    result = 0;
    do { 
      b = encoded[index++] - 63; 
      result |= (b & 0x1f) << shift; 
      shift += 5; 
    } while (b >= 0x20);
    int dlng = ((result & 1) ? ~(result >> 1) : (result >> 1));
    lng += dlng;

    points.push_back({lat * 1e-5f, lng * 1e-5f});
  }
  return points;
}


// =============================================================================
// SECTION 7: CONFIGURATION MANAGEMENT
// =============================================================================

void loadConfiguration() {
  USBSerial.println("=== Loading Configuration from NVS ===");
  
  preferences.begin("config", true);  // Read-only
  
  // Load WiFi credentials
  WIFI_SSID = preferences.getString("ssid", "");
  WIFI_PASS = preferences.getString("password", "");
  
  // Load Strava API credentials
  CLIENT_ID = preferences.getString("clientID", "");
  CLIENT_SECRET = preferences.getString("clientSecret", "");
  REFRESH_TOKEN = preferences.getString("refreshToken", "");
  
  // Load user settings
  USER_NAME = preferences.getString("name", "");
  SPORT_TYPE = preferences.getString("sport", "Run");
  CUSTOM_TITLE = preferences.getString("title", "");
  YEARLY_GOAL = preferences.getFloat("goal", 1000.0);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  TRACK_PERIOD = preferences.getInt("trackPeriod", TRACK_YEARLY);
  
  preferences.end();
  
  // Determine if board is configured (has WiFi credentials)
  isConfigured = (WIFI_SSID.length() > 0);
  
  USBSerial.println("[OK] Configuration loaded:");
  USBSerial.print("  Configured: "); USBSerial.println(isConfigured ? "YES" : "NO");
  USBSerial.print("  WiFi SSID: "); USBSerial.println(WIFI_SSID.length() > 0 ? WIFI_SSID : "(not set)");
  USBSerial.print("  User Name: "); USBSerial.println(USER_NAME.length() > 0 ? USER_NAME : "(not set)");
  USBSerial.print("  Sport Type: "); USBSerial.println(SPORT_TYPE);
  USBSerial.print("  Goal: "); USBSerial.print(YEARLY_GOAL); USBSerial.println(" km");
  USBSerial.print("  Refresh Hours: "); USBSerial.println(REFRESH_HOURS);
  USBSerial.print("  Track Period: "); 
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: USBSerial.println("Weekly"); break;
    case TRACK_MONTHLY: USBSerial.println("Monthly"); break;
    default: USBSerial.println("Yearly"); break;
  }
  USBSerial.println();
}

bool hasStravaCredentials() {
  return (CLIENT_ID.length() > 0 && CLIENT_SECRET.length() > 0 && REFRESH_TOKEN.length() > 0);
}


// =============================================================================
// SECTION 8: WATCHDOG & STABILITY FUNCTIONS
// =============================================================================

void initWatchdog() {
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(NULL);
  USBSerial.print("[OK] Watchdog enabled: ");
  USBSerial.print(WATCHDOG_TIMEOUT_SECONDS);
  USBSerial.println("s timeout");
}

void feedWatchdog() {
  esp_task_wdt_reset();
}

bool checkCrashLoop() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    rapidBootCount++;
    if (rapidBootCount >= 5) {
      USBSerial.println("WARNING: CRASH LOOP DETECTED!");
      return true;
    }
  } else {
    rapidBootCount = 0;
  }
  return false;
}

void resetCrashCounter() {
  rapidBootCount = 0;
}

bool checkFactoryReset() {
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    USBSerial.println("BOOT button held - checking for factory reset...");
    
    for (int i = 0; i < 5; i++) {
      digitalWrite(ACT_LED_PIN, HIGH);
      delay(100);
      digitalWrite(ACT_LED_PIN, LOW);
      delay(100);
      feedWatchdog();
    }
    
    unsigned long startTime = millis();
    
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
      if (millis() - startTime >= FACTORY_RESET_HOLD_MS) {
        USBSerial.println("FACTORY RESET TRIGGERED!");
        
        for (int i = 0; i < 10; i++) {
          digitalWrite(ACT_LED_PIN, HIGH);
          delay(50);
          digitalWrite(ACT_LED_PIN, LOW);
          delay(50);
        }
        
        // Erase all configuration
        preferences.begin("config", false);
        preferences.clear();
        preferences.end();
        
        nvs_flash_erase();
        nvs_flash_init();
        
        bootCount = 0;
        rapidBootCount = 0;
        lastStravaFetchEpoch = 0;
        kmDone = 0;
        timeHours = 0;
        activitiesCount = 0;
        
        USBSerial.println("[OK] Factory reset complete - restarting...");
        delay(500);
        ESP.restart();
        return true;
      }
      
      digitalWrite(ACT_LED_PIN, (millis() / 200) % 2);
      delay(10);
      feedWatchdog();
    }
  }
  return false;
}

void enterSafeMode() {
  USBSerial.println("*** SAFE MODE ***");
  inSafeMode = true;
  
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  display.setRotation(2);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0, 0, W, 100, GxEPD_RED);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(200, 65);
    display.print("SAFE MODE");
    
    display.setFont(&fonnts_com_Maison_Neue_Light18pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, 200);
    display.print("Hold BOOT button 5 seconds to reset");
  } while (display.nextPage());
  
  while (true) {
    feedWatchdog();
    checkFactoryReset();
    delay(100);
    digitalWrite(ACT_LED_PIN, (millis() / 1000) % 2);
  }
}


// =============================================================================
// SECTION 9: PMU FUNCTIONS
// =============================================================================

void pmu_irq_init() {
  pinMode(PMU_IRQ, INPUT_PULLUP);
  
  // Verify PMU IRQ pin state
  int irqState = digitalRead(PMU_IRQ);
  USBSerial.print("PMU IRQ pin (GPIO 21) state: ");
  USBSerial.println(irqState ? "HIGH" : "LOW");
  
  // Configure PMU to generate IRQ on USB events
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    PMU.clearIrqStatus();
    
    // Enable USB insertion/removal interrupts
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_INSERT_IRQ);
    PMU.enableIRQ(XPOWERS_AXP2101_VBUS_REMOVE_IRQ);
    
    USBSerial.println("  ✓ PMU USB interrupts enabled");
  } else {
    USBSerial.println("  ⚠ PMU interrupt setup failed");
  }
}

void pmu_configure_awake() {
  feedWatchdog();
  
  PMU.disableSleep();
  PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
  PMU.disableVbusVoltageMeasure();
  
  PMU.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
  PMU.setPowerKeyPressOnTime(XPOWERS_POWERON_128MS);
  PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
  PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);
  PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
  PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
  
  PMU.setDC1Voltage(3300);
  PMU.setALDO1Voltage(3300); PMU.enableALDO1();
  PMU.setALDO2Voltage(3300); PMU.enableALDO2();
  PMU.setALDO3Voltage(3300); PMU.enableALDO3();
  PMU.setALDO4Voltage(3300); PMU.enableALDO4();
  
  PMU.enableBattVoltageMeasure();
  PMU.enableBattDetection();
  PMU.clearIrqStatus();
  
  feedWatchdog();
}

void pmu_prepare_for_esp32_sleep() {
  feedWatchdog();
  
  USBSerial.println("Preparing PMU for deep sleep...");
  
  // Clear any pending interrupts
  PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  PMU.clearIrqStatus();
  
  // CRITICAL: Enable USB insertion interrupt to wake from sleep
  PMU.enableIRQ(XPOWERS_AXP2101_VBUS_INSERT_IRQ);  // Wake on USB plug-in
  PMU.enableIRQ(XPOWERS_AXP2101_VBUS_REMOVE_IRQ);  // Detect USB removal
  
  // Keep essential power rails on during sleep
  PMU.disableSleep();
  PMU.enableALDO3();  // Keep display rail alive
  PMU.enableALDO4();  // Keep other peripherals alive
  
  USBSerial.println("[OK] PMU ready - will wake on USB connection");
}

void printBatteryStatus() {
  feedWatchdog();
  
  // Re-read PMU to ensure fresh values
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(100);
  
  isUsbConnected = PMU.isVbusIn();
  batteryVoltage = PMU.getBattVoltage() / 1000.0;
  
  // Check charging status - isCharging returns true when actively charging
  // When battery is full and USB connected, isCharging will be false
  isCharging = PMU.isCharging();
  
  // Battery percentage based on voltage
  // Charge target is 4.1V, so 4.05V+ = 100% when on USB
  // On battery: 4.2V = 100%, 3.3V = 0%
  if (isUsbConnected) {
    // When USB connected, battery reads slightly lower due to load
    if (batteryVoltage >= 4.05) batteryPercentage = 100;
    else if (batteryVoltage >= 3.95) batteryPercentage = 90;
    else if (batteryVoltage >= 3.85) batteryPercentage = 75;
    else if (batteryVoltage >= 3.75) batteryPercentage = 55;
    else if (batteryVoltage >= 3.65) batteryPercentage = 35;
    else if (batteryVoltage >= 3.55) batteryPercentage = 20;
    else if (batteryVoltage >= 3.45) batteryPercentage = 10;
    else batteryPercentage = 5;
    
    // CRITICAL FIX: If battery shows <20% on USB, PMU reading is unreliable
    // This happens on some PCs/users - IGNORE IT
    if (batteryPercentage < 20) {
      USBSerial.print(" [⚠️  PMU UNRELIABLE - ignoring false low reading] ");
      batteryPercentage = 50;  // Force reasonable charge level
    }
  } else {
    // On battery only
    if (batteryVoltage >= 4.10) batteryPercentage = 100;
    else if (batteryVoltage >= 4.00) batteryPercentage = 90;
    else if (batteryVoltage >= 3.90) batteryPercentage = 75;
    else if (batteryVoltage >= 3.80) batteryPercentage = 55;
    else if (batteryVoltage >= 3.70) batteryPercentage = 40;
    else if (batteryVoltage >= 3.60) batteryPercentage = 25;
    else if (batteryVoltage >= 3.50) batteryPercentage = 15;
    else batteryPercentage = 5;
  }
  
  // CRITICAL FIX: NEVER enable low battery mode when USB connected
  // USB = charging = always use RED colors (normal mode)
  if (isUsbConnected || USBSerial) {
    lowBatteryMode = false;  // Force normal mode when any USB connection exists
  } else {
    lowBatteryMode = (batteryPercentage < LOW_BATTERY_THRESHOLD);
  }
  
  // Print status
  USBSerial.print("Battery: ");
  USBSerial.print(batteryPercentage);
  USBSerial.print("% (");
  USBSerial.print(batteryVoltage, 2);
  USBSerial.print("V)");
  
  if (isUsbConnected) {
    if (isCharging) {
      USBSerial.print(" [CHARGING]");
    } else if (batteryPercentage >= 95) {
      USBSerial.print(" [CHARGED]");
    } else {
      USBSerial.print(" [USB]");
    }
  }
  USBSerial.println();
}


// =============================================================================
// SECTION 10: USB COMPOSITE DEVICE INITIALIZATION
// =============================================================================

// Custom HID device class - just provides the descriptor, never sends data
class IbisDummyHID : public USBHIDDevice {
public:
  IbisDummyHID() {}
  
  uint16_t _onGetDescriptor(uint8_t* buffer) {
    memcpy(buffer, ibisHidReportDescriptor, sizeof(ibisHidReportDescriptor));
    return sizeof(ibisHidReportDescriptor);
  }
  
  // We never send reports - this device just exists for USB identity
  void _onOutput(uint8_t report_id, const uint8_t* buffer, uint16_t len) {
    // Intentionally empty - we don't process HID output
  }
};

IbisDummyHID ibisDummyDevice;

void initUSBComposite() {
  // Set custom USB device identity - this is what PCs see in Device Manager
  USB.productName("Ibis Dash");
  USB.manufacturerName("Ibis");
  
  // Use Espressif's VID (0x303A) - safe for hobbyist/small projects
  // Custom PID to distinguish from generic ESP32 devices
  USB.VID(0x303A);
  USB.PID(0x8001);  // Custom PID within Espressif's range
  
  // Initialize the dummy HID device - this makes it a composite device
  // Windows will see: CDC (serial port) + HID (vendor device)
  // The HID interface prevents Windows from power-managing the device
  ibishid.addDevice(&ibisDummyDevice, sizeof(ibisHidReportDescriptor));
  ibishid.begin();
  
  // Initialize CDC serial BEFORE starting the USB stack
  // The PC will try to read descriptors immediately on USB.begin(),
  // so all interfaces (CDC + HID) must be ready first.
  // If CDC isn't initialized, the descriptor response is malformed
  // and Windows reports "Device Descriptor Request Failed".
  USBSerial.begin();
  USBSerial.setTxTimeoutMs(0);  // Don't block if no PC is listening (battery mode)
  
  // Start the USB stack - this makes the device visible to the PC
  // All interfaces are now registered and ready for enumeration
  USB.begin();
  
  usbCompositeStarted = true;
}


// =============================================================================
// SECTION 11: DISPLAY LOW-LEVEL FUNCTIONS
// =============================================================================

void epd_wait_busy() {
  unsigned long startTime = millis();
  unsigned long lastYield = millis();
  
  while (digitalRead(EPD_BUSY) == LOW) {
    delay(10);
    if (millis() - lastYield >= 100) {
      feedWatchdog();
      yield();
      lastYield = millis();
    }
    if (millis() - startTime > 60000) {
      USBSerial.println("Display timeout!");
      return;
    }
  }
  yield();
  feedWatchdog();
}

void epd_hardware_reset() {
  digitalWrite(EPD_RST, HIGH);
  delay(50);
  digitalWrite(EPD_RST, LOW);
  delay(20);
  digitalWrite(EPD_RST, HIGH);
  delay(50);
  epd_wait_busy();
}

void epd_deep_init() {
  feedWatchdog();
  epd_hardware_reset();
  delay(80);
  epd_wait_busy();
}

void blinkLED(int times) {
  for(int i = 0; i < times; i++) {
    digitalWrite(ACT_LED_PIN, HIGH);
    delay(100);
    digitalWrite(ACT_LED_PIN, LOW);
    delay(100);
  }
}


// =============================================================================
// SECTION 12: DRAW IBIS LOGO FUNCTION
// =============================================================================

void drawIbisLogo(int x, int y, const uint8_t* logoData, uint16_t w, uint16_t h) {
  // Draw a 4bpp logo from PROGMEM
  // Colors: 0=BLACK, 1=WHITE, 4=RED
  
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col += 2) {
      uint16_t byteIndex = (row * w + col) / 2;
      uint8_t packedByte = pgm_read_byte(&logoData[byteIndex]);
      
      uint8_t pixel1 = (packedByte >> 4) & 0x0F;
      uint8_t pixel2 = packedByte & 0x0F;
      
      uint16_t color1, color2;
      
      // Map to e-ink colors
      switch(pixel1) {
        case 0: color1 = GxEPD_BLACK; break;
        case 4: color1 = GxEPD_RED; break;
        default: color1 = GxEPD_WHITE; break;
      }
      switch(pixel2) {
        case 0: color2 = GxEPD_BLACK; break;
        case 4: color2 = GxEPD_RED; break;
        default: color2 = GxEPD_WHITE; break;
      }
      
      // Only draw non-white pixels for transparency effect
      if (color1 != GxEPD_WHITE) {
        display.drawPixel(x + col, y + row, color1);
      }
      if (col + 1 < w && color2 != GxEPD_WHITE) {
        display.drawPixel(x + col + 1, y + row, color2);
      }
    }
  }
}


// =============================================================================
// SECTION 13: SETUP SCREEN (shown when not configured)
// =============================================================================

void drawSetupScreen() {
  USBSerial.println("=== Drawing Setup Screen ===");
  feedWatchdog();
  
  // Power stabilization before display refresh
  USBSerial.println("Stabilizing power...");
  delay(2000);
  feedWatchdog();
  
  // Re-read PMU to ensure stable readings
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(500);
  feedWatchdog();
  
  esp_task_wdt_delete(NULL);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Header bar - "IBIS SETUP"
    display.fillRect(0, 0, W, 80, GxEPD_RED);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    
    int16_t tx, ty;
    uint16_t tw, th;
    String headerText = "IBIS SETUP";
    display.getTextBounds(headerText, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, 55);
    display.print(headerText);
    
    // Draw both ibis logos side by side (150x150)
    // Positioned a bit lower for better balance
    int logoY = 100;
    int logoSpacing = 60;
    int logo1X = (W / 2) - IBIS_LOGO_CENTERED_WIDTH - (logoSpacing / 2);
    int logo2X = (W / 2) + (logoSpacing / 2);
    
    drawIbisLogo(logo1X, logoY, ibis_logo_centered, IBIS_LOGO_CENTERED_WIDTH, IBIS_LOGO_CENTERED_HEIGHT);
    drawIbisLogo(logo2X, logoY, ibis_logo_cycle, IBIS_LOGO_CYCLE_WIDTH, IBIS_LOGO_CYCLE_HEIGHT);
    
    // Instructions - below logos with good spacing
    // Positioned lower on screen
    int textStartY = logoY + IBIS_LOGO_CENTERED_HEIGHT + 70;
    int lineSpacing = 45;
    
    // Line 1 - BOLD
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    String line1 = "Finish setup on computer";
    display.getTextBounds(line1, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY);
    display.print(line1);
    
    // Line 2 - not bold
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    
    String line2 = "Connect board with USB-C";
    display.getTextBounds(line2, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY + lineSpacing);
    display.print(line2);
    
    // Line 3 - not bold
    String line3 = "Run ibis.exe and follow the instructions";
    display.getTextBounds(line3, 0, 0, &tx, &ty, &tw, &th);
    display.setCursor((W - tw) / 2, textStartY + lineSpacing * 2);
    display.print(line3);
    
  } while (display.nextPage());
  
  // Wait for display to finish
  epd_wait_busy();
  
  // Post-refresh stabilization - reinit PMU
  delay(500);
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  pmu_configure_awake();
  delay(500);
  
  esp_task_wdt_add(NULL);
  feedWatchdog();
  
  USBSerial.println("Setup screen complete!");
}


// =============================================================================
// SECTION 14: WIFI & TIME FUNCTIONS
// =============================================================================

void connectWiFi() {
  if (WIFI_SSID.length() == 0) {
    USBSerial.println("No WiFi credentials - skipping connection");
    return;
  }
  
  USBSerial.print("Connecting to WiFi: ");
  USBSerial.println(WIFI_SSID);
  
  // Power stabilization
  for (int i = 0; i < 3; i++) {
    delay(1000);
    feedWatchdog();
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    handleSerialCommands();
    delay(500);
    USBSerial.print(".");
    attempts++;
    feedWatchdog();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    USBSerial.println(" Connected!");
    USBSerial.print("IP: ");
    USBSerial.println(WiFi.localIP());
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
  } else {
    USBSerial.println(" FAILED!");
  }
}

bool testWiFiConnection() {
  // Test if WiFi credentials are valid by attempting to connect
  if (WIFI_SSID.length() == 0) {
    return false;
  }
  
  USBSerial.println("Testing WiFi connection...");
  
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    USBSerial.print(".");
    attempts++;
    feedWatchdog();
  }
  
  bool success = (WiFi.status() == WL_CONNECTED);
  
  if (success) {
    USBSerial.println(" WiFi OK!");
  } else {
    USBSerial.println(" WiFi FAILED!");
  }
  
  // Disconnect after test
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  return success;
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void initTime() {
  // Skip NTP sync if we synced recently (saves WiFi time)
  time_t now;
  time(&now);
  if (lastNtpSyncEpoch > 0 && (now - lastNtpSyncEpoch) < 43200) {
    USBSerial.println("NTP sync skipped (synced within 12h)");
    return;
  }
  
  USBSerial.println("Syncing time with NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
    feedWatchdog();
  }
  
  if (getLocalTime(&timeinfo)) {
    USBSerial.print("Time synced: ");
    USBSerial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    time(&lastNtpSyncEpoch);
  }
}


// =============================================================================
// SECTION 15: STRAVA API FUNCTIONS
// =============================================================================

bool refreshAccessToken() {
  if (!hasStravaCredentials()) {
    USBSerial.println("No Strava credentials - skipping token refresh");
    return false;
  }
  
  // Check if cached token is still valid (with 5 min buffer)
  time_t now;
  time(&now);
  if (cachedAccessToken[0] != '\0' && tokenExpiresAt > 0 && now < (tokenExpiresAt - 300)) {
    USBSerial.println("Using cached access token");
    accessToken = String(cachedAccessToken);
    return true;
  }
  
  USBSerial.println("Refreshing Strava access token...");
  feedWatchdog();
  
  HTTPClient http;
  String url = "https://www.strava.com/oauth/token";
  url += "?client_id=" + CLIENT_ID;
  url += "&client_secret=" + CLIENT_SECRET;
  url += "&refresh_token=" + REFRESH_TOKEN;
  url += "&grant_type=refresh_token";
  
  http.begin(url);
  int code = http.POST("");
  
  if (code == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    accessToken = doc["access_token"].as<String>();
    int expiresIn = doc["expires_in"] | 21600;
    
    // Cache the token
    strncpy(cachedAccessToken, accessToken.c_str(), sizeof(cachedAccessToken) - 1);
    cachedAccessToken[sizeof(cachedAccessToken) - 1] = '\0';
    time(&now);
    tokenExpiresAt = now + expiresIn;
    
    USBSerial.println("[OK] Token refreshed and cached");
    http.end();
    return true;
  } else {
    USBSerial.print("[FAIL] Token refresh: ");
    USBSerial.println(code);
    http.end();
    return false;
  }
}

// Get timestamps for tracking period
void getTrackingPeriodTimestamps(long &afterTS, long &beforeTS) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    USBSerial.println("Failed to get time!");
    afterTS = 0;
    beforeTS = 0;
    return;
  }
  
  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon;
  int day = timeinfo.tm_mday;
  int wday = timeinfo.tm_wday;  // 0 = Sunday
  
  struct tm start = {0}, end = {0};
  
  switch (TRACK_PERIOD) {
    case TRACK_WEEKLY: {
      // Monday to Sunday (wday: Mon=1, Sun=0)
      // Calculate days since Monday
      int daysSinceMonday = (wday == 0) ? 6 : wday - 1;
      
      // Start of week (Monday 00:00)
      start.tm_year = year - 1900;
      start.tm_mon = month;
      start.tm_mday = day - daysSinceMonday;
      start.tm_hour = 0;
      start.tm_min = 0;
      start.tm_sec = 0;
      
      // End of week (next Monday 00:00)
      end = start;
      end.tm_mday += 7;
      
      dashWeek = (timeinfo.tm_yday / 7) + 1;
      dashMonth = month + 1;
      dashYear = year;
      break;
    }
    
    case TRACK_MONTHLY: {
      // Start of month
      start.tm_year = year - 1900;
      start.tm_mon = month;
      start.tm_mday = 1;
      
      // Start of next month
      end.tm_year = year - 1900;
      end.tm_mon = month + 1;
      end.tm_mday = 1;
      
      dashMonth = month + 1;
      dashYear = year;
      break;
    }
    
    default:  // TRACK_YEARLY
    case TRACK_YEARLY: {
      // Start of year
      start.tm_year = year - 1900;
      start.tm_mon = 0;
      start.tm_mday = 1;
      
      // Start of next year
      end.tm_year = year - 1900 + 1;
      end.tm_mon = 0;
      end.tm_mday = 1;
      
      dashYear = year;
      break;
    }
  }
  
  afterTS = mktime(&start);
  beforeTS = mktime(&end);
}

void fetchStravaData() {
  if (!hasStravaCredentials()) {
    USBSerial.println("No Strava credentials - using placeholder data");
    kmDone = 0;
    timeHours = 0;
    activitiesCount = 0;
    lastTitle = "No Strava";
    lastLine = "Configure in Ibis Setup app";
    lastPolyline = "";
    return;
  }
  
  USBSerial.println("=== Fetching Strava Data ===");
  feedWatchdog();
  
  // Reset stats
  kmDone = 0;
  timeHours = 0;
  activitiesCount = 0;
  lastTitle = "";
  lastLine = "";
  lastPolyline = "";
  lastDistKm = 0;
  lastMovingSecs = 0;
  lastDateStr = "";
  
  if (accessToken == "") {
    USBSerial.println("No access token");
    return;
  }
  
  // Get timestamp range for tracking period
  long afterTS, beforeTS;
  getTrackingPeriodTimestamps(afterTS, beforeTS);
  
  USBSerial.print("Tracking period: ");
  switch(TRACK_PERIOD) {
    case TRACK_WEEKLY: USBSerial.println("Weekly"); break;
    case TRACK_MONTHLY: USBSerial.println("Monthly"); break;
    default: USBSerial.println("Yearly"); break;
  }
  USBSerial.print("Filtering for sport type: ");
  USBSerial.println(SPORT_TYPE);
  
  bool gotLast = false;
  int page = 1;
  int totalActivities = 0;  // Track all activities fetched
  
  while (true) {
    feedWatchdog();
    
    HTTPClient http;
    String req = "https://www.strava.com/api/v3/athlete/activities?";
    req += "after=" + String(afterTS) + "&before=" + String(beforeTS);
    req += "&per_page=200&page=" + String(page);
    
    http.begin(req);
    http.addHeader("Authorization", "Bearer " + accessToken);
    
    int code = http.GET();
    
    if (code != 200) {
      USBSerial.print("API error: ");
      USBSerial.println(code);
      http.end();
      break;
    }
    
    String payload = http.getString();
    JsonDocument doc;
    
    if (deserializeJson(doc, payload)) {
      USBSerial.println("JSON parse error");
      http.end();
      break;
    }
    
    if (!doc.is<JsonArray>()) {
      USBSerial.println("Response is not an array");
      http.end();
      break;
    }
    
    JsonArray arr = doc.as<JsonArray>();
    
    if (arr.size() == 0) {
      USBSerial.println("No more activities");
      http.end();
      break;
    }
    
    USBSerial.print("Page ");
    USBSerial.print(page);
    USBSerial.print(": ");
    USBSerial.print(arr.size());
    USBSerial.println(" activities");
    
    for (JsonObject act : arr) {
      totalActivities++;
      
      String actType = act["type"].as<String>();
      
      // Debug: show what we're comparing
      if (totalActivities <= 3) {
        USBSerial.print("  Activity type: '");
        USBSerial.print(actType);
        USBSerial.print("' vs filter: '");
        USBSerial.print(SPORT_TYPE);
        USBSerial.println("'");
      }
      
      // Filter by selected sport type - EXACT MATCH (case-sensitive)
      if (actType == SPORT_TYPE) {
        if (!gotLast) {
          // Get last activity details with validation
          lastTitle = act["name"].as<String>();
          if (lastTitle.length() == 0) {
            lastTitle = "Untitled " + SPORT_TYPE;
          }
          
          String dt = act["start_date_local"].as<String>();
          
          // Parse date into "2 February" format
          // dt format from Strava: "2025-01-02T14:30:00Z"
          if (dt.length() >= 10) {
            int month = dt.substring(5, 7).toInt();
            int day = dt.substring(8, 10).toInt();
            const char* monthNames[] = {"January", "February", "March", "April", 
                                        "May", "June", "July", "August", 
                                        "September", "October", "November", "December"};
            if (month >= 1 && month <= 12) {
              lastDateStr = String(day) + " " + monthNames[month - 1];
            } else {
              lastDateStr = dt.substring(0, 10);
            }
          } else {
            lastDateStr = dt;
          }
          
          float dkm = act["distance"].as<float>() / 1000.0;
          int movingSecs = act["moving_time"].as<int>();
          float hrs = movingSecs / 3600.0;
          
          // Validate the numbers are real
          if (dkm > 0 && dkm < 1000 && hrs > 0 && hrs < 100) {
            lastDistKm = dkm;
            lastMovingSecs = movingSecs;
            lastLine = String(dkm, 1) + " km  " + String(hrs, 1) + "h  " + lastDateStr;
            gotLast = true;
            
            if (!act["map"]["summary_polyline"].isNull()) {
              lastPolyline = act["map"]["summary_polyline"].as<String>();
            }
          }
        }
        
        // Accumulate stats with validation
        float distance = act["distance"].as<float>() / 1000.0;
        float time = act["moving_time"].as<float>() / 3600.0;
        
        // Only add if values are reasonable
        if (distance > 0 && distance < 1000 && time > 0 && time < 100) {
          kmDone += distance;
          timeHours += time;
          activitiesCount++;
        }
      }
    }
    
    http.end();
    page++;
    
    // Safety: don't fetch more than 10 pages
    if (page > 10) {
      USBSerial.println("Reached max pages (10)");
      break;
    }
    
    delay(200);  // Be nice to Strava API
  }
  
  // Update timestamp
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%y", &timeinfo);
    lastUpdateTime = String(timeStr);
    lastStravaFetchEpoch = mktime(&timeinfo);
  }
  
  USBSerial.println("=== Strava Fetch Complete ===");
  USBSerial.print("Total activities fetched: "); USBSerial.println(totalActivities);
  USBSerial.print("Matching activities ("); USBSerial.print(SPORT_TYPE); USBSerial.print("): ");
  USBSerial.println(activitiesCount);
  USBSerial.print("Distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("Time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  
  // Validate we got reasonable data
  if (totalActivities > 0 && activitiesCount == 0) {
    USBSerial.println("WARNING: Got activities but none matched sport type!");
    USBSerial.print("Check that SPORT_TYPE='");
    USBSerial.print(SPORT_TYPE);
    USBSerial.println("' matches Strava activity types exactly");
  }
}

bool shouldFetchStrava() {
  if (lastStravaFetchEpoch == 0) return true;
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return true;
  
  time_t currentEpoch = mktime(&timeinfo);
  time_t timeSinceLastFetch = currentEpoch - lastStravaFetchEpoch;
  unsigned long fetchIntervalSeconds = (unsigned long)REFRESH_HOURS * 3600;
  
  return (timeSinceLastFetch >= (time_t)fetchIntervalSeconds);
}

// =============================================================================
// SECTION 16: MAIN DASHBOARD DISPLAY
// =============================================================================

String getHeaderTitle() {
  // Generate header title based on settings
  String title = "";
  
  // Use custom title if set
  if (CUSTOM_TITLE.length() > 0) {
    return CUSTOM_TITLE;
  }
  
  // Build automatic title
  String name = (USER_NAME.length() > 0) ? USER_NAME + "'s " : "";
  
  switch (TRACK_PERIOD) {
    case TRACK_WEEKLY:
      title = name + SPORT_TYPE + " Week " + String(dashWeek);
      break;
    case TRACK_MONTHLY: {
      const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      title = name + SPORT_TYPE + " " + months[dashMonth - 1] + " " + String(dashYear);
      break;
    }
    default:
      title = name + "Strava Stats " + String(dashYear);
      break;
  }
  
  return title;
}

String getActivityLabel() {
  // Return appropriate label based on sport type
  if (SPORT_TYPE == "Run") return "RUNS";
  if (SPORT_TYPE == "Ride") return "RIDES";
  if (SPORT_TYPE == "Swim") return "SWIMS";
  if (SPORT_TYPE == "Hike") return "HIKES";
  if (SPORT_TYPE == "Walk") return "WALKS";
  return "ACTIVITIES";
}

void drawDashboard() {
  USBSerial.println("=== Drawing Dashboard ===");
  feedWatchdog();
  
  // Power stabilization
  delay(1000);
  feedWatchdog();
  
  // Reinit PMU for accurate readings
  PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
  delay(500);
  
  // Check power stability
  float currentVoltage = PMU.getBattVoltage() / 1000.0;
  bool usbOK = PMU.isVbusIn();
  
  if (usbOK && currentVoltage < 3.5) {
    delay(2000);
    feedWatchdog();
    currentVoltage = PMU.getBattVoltage() / 1000.0;
    if (currentVoltage < 3.3) {
      USBSerial.println("Power unstable - skipping refresh");
      return;
    }
  }
  
  // CRITICAL: Full display reinitialization
  USBSerial.println("Reinitializing display hardware...");
  SPI.end();
  delay(100);
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  delay(200);
  feedWatchdog();
  
  // Calculate progress
  float pct = (YEARLY_GOAL > 0) ? (kmDone / YEARLY_GOAL) : 0;
  if (pct > 1) pct = 1;
  if (pct < 0) pct = 0;
  
  // Update colors
  if (lowBatteryMode) {
    C_HEADER = GxEPD_BLUE;
    C_TEXT_DIM = GxEPD_BLUE;
  } else {
    C_HEADER = GxEPD_RED;
    C_TEXT_DIM = GxEPD_RED;
  }
  
  USBSerial.println("Starting display refresh...");
  USBSerial.flush();
  delay(200);
  
  esp_task_wdt_delete(NULL);
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    feedWatchdog();
    display.fillScreen(GxEPD_WHITE);
    
    const int margin = 20;
    const int innerPad = 18;
    
    int16_t tx1, ty1;
    uint16_t tw, th;
    
    // ===== LAYOUT CALCULATION =====
    // Two anchors:
    //   Top: banner(70) + 46px = 116 (DISTANCE label baseline) - looks perfect
    //   Bottom: H - 12 = 468 (same breathing room as status text)
    //
    // Fixed content between anchors = 200px
    // Available space = 468 - 116 = 352px
    // Two section gaps = (352 - 200) / 2 = 76px each
    
    const int headerH = 70;
    const int topAnchor = 121;           // DISTANCE label baseline
    const int bottomAnchor = H - 12;     // 468 - values baseline
    const int labelToValue = 36;         // Title to value within a section
    const int barGapAbove = 16;          // Value to bar top
    const int barH = 34;                 // Progress bar height (thicker)
    const int barGapBelow = 20;          // Bar bottom to % text
    const int headerGapSmall = 30;       // Date→headers and headers→values
    const int sectionGap = 51;           // Even gap between major sections (was at 54)
    
    // ===== HEADER =====
    display.fillRect(0, 0, W, headerH, C_HEADER);
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    
    String headerText = getHeaderTitle();
    display.getTextBounds(headerText, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor((W - tw) / 2, 48);
    display.print(headerText);
    
    // ===== DISTANCE SECTION =====
    int distLabelY = topAnchor;  // 116
    
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(margin + innerPad, distLabelY);
    display.print("DISTANCE");
    
    int distValueY = distLabelY + labelToValue;  // 146
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(margin + innerPad, distValueY);
    display.print(String(kmDone, 1) + " km / " + String((int)YEARLY_GOAL) + " km");
    
    // Progress bar
    int barX = margin + innerPad;
    int barY = distValueY + barGapAbove;  // 158
    int barW = (W - 2 * margin) - 2 * innerPad;
    
    display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    display.drawRect(barX + 1, barY + 1, barW - 2, barH - 2, GxEPD_BLACK);
    
    int fillW = (int)((barW - 4) * pct);
    if (fillW > 0) {
      display.fillRect(barX + 2, barY + 2, fillW, barH - 4, GxEPD_GREEN);
    }
    
    float kmToGo = YEARLY_GOAL - kmDone;
    if (kmToGo < 0) kmToGo = 0;
    int pctTextY = barY + barH + barGapBelow;
    display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(barX, pctTextY);
    display.print(String(pct * 100, 1) + "% of goal  " + String(kmToGo, 1) + " km to go");
    
    // ===== RUNS / TIME / LAST ROUTE =====
    int runsLabelY = pctTextY + sectionGap;  // 280
    int runsValueY = runsLabelY + labelToValue;  // 310
    
    // Column positions (3 equal columns)
    int colGap = 14;
    int colW = (W - 2 * margin - 2 * colGap) / 3;
    int x1 = margin;
    int x2 = margin + colW + colGap;
    int x3 = margin + 2 * (colW + colGap);
    
    // Activities panel
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(x1 + innerPad, runsLabelY);
    display.print(getActivityLabel());
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(x1 + innerPad, runsValueY);
    display.print(String(activitiesCount));
    
    // Time panel - centered
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.getTextBounds("TIME", 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(x2 + (colW - tw) / 2, runsLabelY);
    display.print("TIME");
    
    int hours = (int)timeHours;
    int minutes = (int)((timeHours - hours) * 60);
    String timeStr = String(hours) + "h " + String(minutes) + "m";
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.getTextBounds(timeStr, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(x2 + (colW - tw) / 2, runsValueY);
    display.print(timeStr);
    
    // Last Route panel
    int routeTopY = runsLabelY - 32;
    int routeH = H - routeTopY - 30;
    display.fillRect(x3, routeTopY, colW, routeH, GxEPD_WHITE);
    
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(x3 + innerPad, runsLabelY);
    display.print("LAST ROUTE");
    
    // Draw polyline
    int titleAreaHeight = 45;
    int polylineAreaX = x3;
    int polylineAreaY = routeTopY + titleAreaHeight;
    int polylineAreaW = colW;
    int polylineAreaH = routeH - titleAreaHeight;
    
    if (lastPolyline.length() > 0) {
      std::vector<Point> pts = decodePolyline(lastPolyline);
      
      if (pts.size() >= 2) {
        float minLat = pts[0].lat, maxLat = pts[0].lat;
        float minLon = pts[0].lon, maxLon = pts[0].lon;
        for (auto &p : pts) {
          if (p.lat < minLat) minLat = p.lat;
          if (p.lat > maxLat) maxLat = p.lat;
          if (p.lon < minLon) minLon = p.lon;
          if (p.lon > maxLon) maxLon = p.lon;
        }
        
        float latRange = maxLat - minLat;
        if (latRange < 1e-6) latRange = 1e-6;
        float lonRange = maxLon - minLon;
        if (lonRange < 1e-6) lonRange = 1e-6;
        
        int drawMargin = 2;
        int drawWidth = polylineAreaW - (2 * drawMargin);
        int drawHeight = polylineAreaH - (2 * drawMargin);
        
        float routeAspect = lonRange / latRange;
        float zoneAspect = (float)drawWidth / (float)drawHeight;
        
        int actualDrawWidth, actualDrawHeight;
        int offsetX = 0, offsetY = 0;
        
        if (routeAspect > zoneAspect) {
          actualDrawWidth = drawWidth;
          actualDrawHeight = (int)(drawWidth / routeAspect);
          offsetY = (drawHeight - actualDrawHeight) / 2;
        } else {
          actualDrawHeight = drawHeight;
          actualDrawWidth = (int)(drawHeight * routeAspect);
          offsetX = (drawWidth - actualDrawWidth) / 2;
        }
        
        int drawX = polylineAreaX + drawMargin;
        int drawY = polylineAreaY + drawMargin;
        
        for (size_t i = 1; i < pts.size(); i++) {
          float normLon0 = (pts[i - 1].lon - minLon) / lonRange;
          float normLat0 = (pts[i - 1].lat - minLat) / latRange;
          float normLon1 = (pts[i].lon - minLon) / lonRange;
          float normLat1 = (pts[i].lat - minLat) / latRange;
          
          int px0 = drawX + offsetX + (int)(normLon0 * actualDrawWidth);
          int py0 = drawY + offsetY + (int)((1.0 - normLat0) * actualDrawHeight);
          int px1 = drawX + offsetX + (int)(normLon1 * actualDrawWidth);
          int py1 = drawY + offsetY + (int)((1.0 - normLat1) * actualDrawHeight);
          
          display.drawLine(px0, py0, px1, py1, GxEPD_BLACK);
          display.drawLine(px0 + 1, py0, px1 + 1, py1, GxEPD_BLACK);
        }
      }
    }
    
    // ===== LATEST ACTIVITY PANEL =====
    // Layout:
    //   LATEST RUN
    //   2 February - Afternoon Run
    //   Distance    Pace       Time
    //   8.09 km     5:08 /km   41m 35s  ← lands at bottomAnchor (468)
    
    int latestLabelY = runsValueY + sectionGap;  // 386
    int latestW = colW * 2 + colGap;
    
    // Row 1: "LATEST RUN" title
    display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
    display.setTextColor(C_TEXT_DIM);
    display.setCursor(margin + innerPad, latestLabelY);
    String sportLabel = SPORT_TYPE.substring(0, SPORT_TYPE.length() > 4 ? 4 : SPORT_TYPE.length());
    sportLabel.toUpperCase();
    display.print("LATEST " + sportLabel);
    
    // Row 2: Date + activity name (e.g. "2 February - Afternoon Run")
    int dateNameY = latestLabelY + labelToValue;  // 416
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(margin + innerPad, dateNameY);
    if (lastDateStr.length() > 0) {
      display.print(lastDateStr);
    } else if (lastTitle.length() > 0) {
      display.print(lastTitle);  // Fallback if no date
    }
    
    // Row 3: Column headers
    int colHeaderY = dateNameY + headerGapSmall;  // 442
    int col1X = margin + innerPad;
    int col2X = margin + innerPad + (latestW - 2 * innerPad) / 3;
    int col3X = margin + innerPad + 2 * (latestW - 2 * innerPad) / 3;
    
    display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(col1X, colHeaderY);
    display.print("Distance");
    display.setCursor(col2X, colHeaderY);
    display.print("Pace");
    display.setCursor(col3X, colHeaderY);
    display.print("Time");
    
    // Row 4: Values - positioned relative to column headers
    int valY = colHeaderY + headerGapSmall;  // Position relative to headers above
    display.setFont(&fonnts_com_Maison_Neue_Light15pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    if (lastDistKm > 0) {
      display.setCursor(col1X, valY);
      display.print(String(lastDistKm, 2) + " km");
      
      display.setCursor(col2X, valY);
      if (lastDistKm > 0.01) {
        int totalPaceSecs = (int)(lastMovingSecs / lastDistKm);
        int paceMin = totalPaceSecs / 60;
        int paceSec = totalPaceSecs % 60;
        char paceBuf[12];
        snprintf(paceBuf, sizeof(paceBuf), "%d:%02d /km", paceMin, paceSec);
        display.print(paceBuf);
      } else {
        display.print("--");
      }
      
      display.setCursor(col3X, valY);
      int totalMin = lastMovingSecs / 60;
      int timeSec = lastMovingSecs % 60;
      if (totalMin >= 60) {
        int timeH = totalMin / 60;
        int timeM = totalMin % 60;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%dh %02dm", timeH, timeM);
        display.print(timeBuf);
      } else {
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%dm %02ds", totalMin, timeSec);
        display.print(timeBuf);
      }
    } else {
      display.setCursor(col1X, valY);
      display.print("--");
      display.setCursor(col2X, valY);
      display.print("--");
      display.setCursor(col3X, valY);
      display.print("--");
    }
    
    // ===== STATUS INDICATOR =====
    char displayStr[40];
    
    if (lowBatteryMode) {
      snprintf(displayStr, sizeof(displayStr), "Battery: %.0f%%", batteryPercentage);
      display.setTextColor(GxEPD_RED);
      display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    } else if (isUsbConnected) {
      if (batteryPercentage >= 100) {
        snprintf(displayStr, sizeof(displayStr), "I-MUUT! 100%%");
        display.setTextColor(GxEPD_GREEN);
        display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
      } else if (isCharging) {
        snprintf(displayStr, sizeof(displayStr), "Charging: %.0f%%", batteryPercentage);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
      } else {
        snprintf(displayStr, sizeof(displayStr), "Battery: %.0f%%", batteryPercentage);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
      }
    } else {
      snprintf(displayStr, sizeof(displayStr), "Updated: %s", lastUpdateTime.c_str());
      display.setTextColor(GxEPD_BLACK);
      display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
    }
    
    display.getTextBounds(displayStr, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(W - tw - margin, H - 12);
    display.print(displayStr);
    
  } while (display.nextPage());
  
  epd_wait_busy();
  esp_task_wdt_add(NULL);
  feedWatchdog();
  delay(1000);
  yield();
  USBSerial.flush();
  delay(500);
  
  USBSerial.println("Dashboard update complete!");
}

// =============================================================================
// SECTION 17: HIGH-LEVEL UPDATE FUNCTIONS
// =============================================================================

bool fetchStravaDataWithValidation() {
  // Returns true if data was successfully fetched AND validated
  USBSerial.println("\n=== FETCHING & VALIDATING STRAVA DATA ===");
  
  // Step 1: Connect WiFi
  USBSerial.println("Step 1: Connecting WiFi...");
  connectWiFi();
  
  if (WiFi.status() != WL_CONNECTED) {
    USBSerial.println("WiFi connection failed!");
    return false;
  }
  USBSerial.println("[OK] WiFi connected");
  delay(1000);  // Stabilization
  feedWatchdog();
  
  // Step 2: Sync time
  USBSerial.println("Step 2: Syncing time...");
  initTime();
  delay(500);
  feedWatchdog();
  
  // Step 3: Refresh token
  USBSerial.println("Step 3: Refreshing access token...");
  if (!refreshAccessToken()) {
    USBSerial.println("[FAIL] Token refresh failed!");
    disconnectWiFi();
    return false;
  }
  USBSerial.println("[OK] Access token refreshed");
  delay(500);
  feedWatchdog();
  
  // Step 4: Fetch data
  USBSerial.println("Step 4: Fetching Strava activities...");
  fetchStravaData();
  delay(500);
  feedWatchdog();
  
  disconnectWiFi();
  
  // Step 5: Validate results
  USBSerial.println("Step 5: Validating results...");
  USBSerial.print("  Activities: "); USBSerial.println(activitiesCount);
  USBSerial.print("  Distance: "); USBSerial.print(kmDone, 1); USBSerial.println(" km");
  USBSerial.print("  Time: "); USBSerial.print(timeHours, 1); USBSerial.println(" hours");
  
  // Data is valid if:
  // - We have at least fetched (even 0 activities is valid)
  // - Numbers are reasonable (not negative, not insanely large)
  bool dataValid = (kmDone >= 0 && kmDone < 100000 && 
                    timeHours >= 0 && timeHours < 50000 &&
                    activitiesCount >= 0 && activitiesCount < 10000);
  
  if (dataValid) {
    USBSerial.println("[OK] Data validation passed!");
  } else {
    USBSerial.println("[FAIL] Data validation failed - values out of range!");
  }
  
  return dataValid;
}

void updateStravaAndDisplay(bool forceFetch) {
  USBSerial.println("\n========== FULL UPDATE ==========");
  feedWatchdog();
  
  bool dataFetched = false;
  
  if (forceFetch || shouldFetchStrava()) {
    // Try to fetch fresh data
    dataFetched = fetchStravaDataWithValidation();
    
    if (!dataFetched) {
      USBSerial.println("WARNING: Could not fetch Strava data!");
      // Continue with cached/zero data
    }
  } else {
    // Just update time
    connectWiFi();
    if (WiFi.status() == WL_CONNECTED) {
      initTime();
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeStr[10];
        strftime(timeStr, sizeof(timeStr), "%d-%m-%y", &timeinfo);
        lastUpdateTime = String(timeStr);
        dashYear = timeinfo.tm_year + 1900;
      }
    }
    disconnectWiFi();
  }
  
  // Get fresh battery reading
  printBatteryStatus();
  
  USBSerial.println("Drawing dashboard...");
  delay(500);
  feedWatchdog();
  
  drawDashboard();
  resetCrashCounter();
  
  USBSerial.println("========== UPDATE COMPLETE ==========\n");
}

void updateDisplayOnly() {
  USBSerial.println("\n=== DISPLAY REFRESH (cached data) ===");
  feedWatchdog();
  printBatteryStatus();
  
  SPI.end();
  delay(100);
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  feedWatchdog();
  
  drawDashboard();
}


// =============================================================================
// SECTION 18: SERIAL COMMAND HANDLER (for Ibis Setup app)
// =============================================================================

void handleSerialCommands() {
  while (USBSerial.available()) {
    char c = USBSerial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        processSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else {
      // Prevent buffer overflow
      if (serialInputBuffer.length() < 2048) {
        serialInputBuffer += c;
      }
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  // Debug: show what we received
  USBSerial.print("CMD[");
  USBSerial.print(command.length());
  USBSerial.print("]: ");
  if (command.length() > 50) {
    USBSerial.print(command.substring(0, 50));
    USBSerial.println("...");
  } else {
    USBSerial.println(command);
  }
  
  if (command == "GET_CONFIG") {
    sendCurrentConfig();
  }
  else if (command.startsWith("SET_CONFIG:")) {
    String jsonStr = command.substring(11);
    saveConfigFromSerial(jsonStr);
  }
  else if (command == "WIPE_CONFIG") {
    // Internal wipe - does NOT draw screen (used when updating settings)
    wipeConfig();
  }
  else if (command == "DELETE_DATA") {
    // User-initiated delete - wipes AND shows setup screen
    wipeConfigAndShowSetup();
  }
  else if (command == "TEST_WIFI") {
    // Test WiFi connection with current credentials
    loadConfiguration();
    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (testWiFiConnection()) {
      USBSerial.println("WIFI_OK");
    } else {
      USBSerial.println("WIFI_FAILED");
    }
  }
  else if (command == "FETCH_STRAVA") {
    // "Finish Setup" button in EXE - fetch fresh data and draw dashboard
    USBSerial.println("\n=== FINISH SETUP ===");
    loadConfiguration();;
    
    if (WIFI_SSID.length() == 0) {
      USBSerial.println("NO_WIFI_CREDENTIALS");
    } else if (!hasStravaCredentials()) {
      USBSerial.println("NO_STRAVA_CREDENTIALS");
    } else {
      USBSerial.println("Connecting to WiFi...");
      connectWiFi();
      
      if (WiFi.status() != WL_CONNECTED) {
        USBSerial.println("WIFI_CONNECT_FAILED");
      } else {
        USBSerial.println("Syncing time...");
        initTime();
        feedWatchdog();
        
        USBSerial.println("Refreshing token...");
        if (!refreshAccessToken()) {
          USBSerial.println("TOKEN_REFRESH_FAILED");
          disconnectWiFi();
        } else {
          USBSerial.println("Fetching Strava data...");
          feedWatchdog();
          fetchStravaData();
          disconnectWiFi();
          
          USBSerial.println("STRAVA_OK");
          USBSerial.print("Activities: "); USBSerial.println(activitiesCount);
          USBSerial.print("Distance: "); USBSerial.print(kmDone); USBSerial.println(" km");
          USBSerial.print("Time: "); USBSerial.print(timeHours); USBSerial.println(" hours");
          
          // Update battery and draw
          printBatteryStatus();
          USBSerial.println("Drawing dashboard...");
          drawDashboard();
          USBSerial.println("DASHBOARD_DRAWN");
          
          // If on battery, request sleep after drawing
          if (!PMU.isVbusIn()) {
            USBSerial.println(">>> On battery - will sleep <<<");
            sleepRequested = true;
          }
        }
      }
    }
  }
  else if (command == "SHOW_SETUP_SCREEN") {
    // Draw setup screen (used after wipe)
    USBSerial.println("Drawing setup screen...");
    drawSetupScreen();
    USBSerial.println("SETUP_SCREEN_DRAWN");
  }
  else if (command == "GO_SLEEP") {
    // Explicit sleep command
    USBSerial.println("Sleep requested");
    USBSerial.println("OK");
    delay(100);
    sleepRequested = true;
  }
  else if (command == "RESTART") {
    USBSerial.println("OK");
    USBSerial.println("Restarting...");
    delay(500);
    ESP.restart();
  }
  else if (command == "PING") {
    USBSerial.println("PONG");
    USBSerial.println("IBIS_DASH_V40");
  }
  else if (command.length() > 0) {
    USBSerial.print("Unknown command: ");
    USBSerial.println(command);
    USBSerial.println("ERROR");
  }
}

void sendCurrentConfig() {
  USBSerial.println("Sending configuration...");
  
  preferences.begin("config", true);
  
  DynamicJsonDocument doc(2048);
  
  doc["ssid"] = preferences.getString("ssid", "");
  doc["password"] = preferences.getString("password", "");
  doc["name"] = preferences.getString("name", "");
  doc["sport"] = preferences.getString("sport", "Run");
  doc["goal"] = preferences.getFloat("goal", 1000.0);
  doc["clientID"] = preferences.getString("clientID", "");
  doc["clientSecret"] = preferences.getString("clientSecret", "");
  doc["refreshToken"] = preferences.getString("refreshToken", "");
  doc["refreshHours"] = preferences.getInt("refreshHours", 12);
  doc["trackPeriod"] = preferences.getInt("trackPeriod", TRACK_YEARLY);
  doc["title"] = preferences.getString("title", "");
  doc["configured"] = (preferences.getString("ssid", "").length() > 0);
  doc["hasStrava"] = (preferences.getString("clientID", "").length() > 0);
  doc["firmwareVersion"] = "4.0";
  doc["usbIdentity"] = "Ibis Dash";
  
  preferences.end();
  
  String output;
  serializeJson(doc, output);
  USBSerial.println(output);
  USBSerial.println("OK");
}

void saveConfigFromSerial(String jsonStr) {
  USBSerial.println("Parsing configuration...");
  USBSerial.print("JSON length: ");
  USBSerial.println(jsonStr.length());
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    USBSerial.print("JSON error: ");
    USBSerial.println(error.c_str());
    USBSerial.println("ERROR");
    return;
  }
  
  USBSerial.println("JSON parsed OK, saving to NVS...");
  
  preferences.begin("config", false);
  
  if (doc.containsKey("ssid")) {
    preferences.putString("ssid", doc["ssid"].as<String>());
    USBSerial.println("  - ssid saved");
  }
  if (doc.containsKey("password")) {
    preferences.putString("password", doc["password"].as<String>());
    USBSerial.println("  - password saved");
  }
  if (doc.containsKey("name")) {
    preferences.putString("name", doc["name"].as<String>());
  }
  if (doc.containsKey("sport")) {
    preferences.putString("sport", doc["sport"].as<String>());
  }
  if (doc.containsKey("goal")) {
    preferences.putFloat("goal", doc["goal"].as<float>());
  }
  if (doc.containsKey("clientID")) {
    preferences.putString("clientID", doc["clientID"].as<String>());
    USBSerial.println("  - clientID saved");
  }
  if (doc.containsKey("clientSecret")) {
    preferences.putString("clientSecret", doc["clientSecret"].as<String>());
    USBSerial.println("  - clientSecret saved");
  }
  if (doc.containsKey("refreshToken")) {
    preferences.putString("refreshToken", doc["refreshToken"].as<String>());
    USBSerial.println("  - refreshToken saved");
  }
  if (doc.containsKey("refreshHours")) {
    preferences.putInt("refreshHours", doc["refreshHours"].as<int>());
  }
  if (doc.containsKey("trackPeriod")) {
    preferences.putInt("trackPeriod", doc["trackPeriod"].as<int>());
  }
  if (doc.containsKey("title")) {
    preferences.putString("title", doc["title"].as<String>());
  }
  
  preferences.end();
  
  // Reload configuration
  loadConfiguration();
  
  // Clear token cache if Strava credentials were updated
  if (doc.containsKey("clientID") || doc.containsKey("clientSecret") || doc.containsKey("refreshToken")) {
    cachedAccessToken[0] = '\0';
    tokenExpiresAt = 0;
    USBSerial.println("  - Token cache cleared");
  }
  
  USBSerial.println("Configuration saved!");
  USBSerial.println("SUCCESS");
}

void wipeConfig() {
  // Called by WIPE_CONFIG command - clears everything but does NOT draw
  // (Used internally by EXE when updating settings)
  USBSerial.println("Wiping all configuration...");
  
  preferences.begin("config", false);
  preferences.clear();
  preferences.end();
  
  // Clear runtime variables
  WIFI_SSID = "";
  WIFI_PASS = "";
  CLIENT_ID = "";
  CLIENT_SECRET = "";
  REFRESH_TOKEN = "";
  USER_NAME = "";
  CUSTOM_TITLE = "";
  isConfigured = false;
  
  // Clear RTC data
  kmDone = 0;
  timeHours = 0;
  activitiesCount = 0;
  lastStravaFetchEpoch = 0;
  lastNtpSyncEpoch = 0;
  
  // Clear cached data
  lastTitle = "";
  lastLine = "";
  lastPolyline = "";
  lastUpdateTime = "";
  lastDistKm = 0;
  lastMovingSecs = 0;
  lastDateStr = "";
  
  // Clear token cache
  cachedAccessToken[0] = '\0';
  tokenExpiresAt = 0;
  
  USBSerial.println("Configuration wiped!");
  USBSerial.println("WIPED");
}

void wipeConfigAndShowSetup() {
  // Called by DELETE_DATA command - clears everything AND shows setup screen
  // (Used when user explicitly deletes all data)
  wipeConfig();  // First wipe
  
  // Then draw setup screen
  USBSerial.println("Drawing setup screen...");
  drawSetupScreen();
  USBSerial.println("SETUP_SCREEN_DRAWN");
}


// =============================================================================
// SECTION 19: SLEEP & WAKE
// =============================================================================

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  wasManualWake = false;
  
  USBSerial.println("\n========== WAKE UP REASON ==========");
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      USBSerial.println("Wakeup: USB connected! (GPIO 21 - PMU IRQ)");
      // USB wake is NOT manual wake - just stay idle for commands
      wasManualWake = false;
      
      // Verify USB is actually connected
      delay(500);  // Give PMU time to stabilize
      if (PMU.isVbusIn()) {
        USBSerial.println("  ✓ USB confirmed connected");
        USBSerial.println("  → Staying idle, waiting for serial commands");
      } else {
        USBSerial.println("  ⚠ USB not detected (false wake?)");
      }
      break;
      
    case ESP_SLEEP_WAKEUP_EXT1:
      USBSerial.println("Wakeup: BOOT button pressed");
      wasManualWake = true;  // Only button press is manual wake
      USBSerial.println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_TIMER:
      USBSerial.println("Wakeup: Timer expired (scheduled refresh)");
      USBSerial.println("  → Will fetch fresh Strava data");
      break;
      
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      USBSerial.println("Wakeup: Power-on or Reset button");
      break;
      
    default:
      USBSerial.print("Wakeup: Unknown reason (");
      USBSerial.print(wakeup_reason);
      USBSerial.println(")");
      break;
  }
  
  USBSerial.println("====================================\n");
}

void go_to_deep_sleep() {
  USBSerial.println("\n========== ENTERING DEEP SLEEP ==========");
  
  // Check USB status before sleeping
  bool usbNowConnected = PMU.isVbusIn();
  USBSerial.print("USB status before sleep: ");
  USBSerial.println(usbNowConnected ? "CONNECTED" : "DISCONNECTED");
  
  if (usbNowConnected) {
    USBSerial.println("WARNING: USB is connected - should not sleep!");
    USBSerial.println("Returning to loop() instead of sleeping...");
    return;  // Don't sleep if USB is connected!
  }
  
  // Re-read config from NVS to get the latest refresh hours
  preferences.begin("config", true);
  REFRESH_HOURS = preferences.getInt("refreshHours", 12);
  bool hasConfig = (preferences.getString("ssid", "").length() > 0);
  preferences.end();
  
  // Calculate sleep duration based on configuration
  uint64_t sleepDuration;
  if (!hasConfig) {
    sleepDuration = SLEEP_DURATION_UNCONFIGURED_US;
    USBSerial.println("Not configured - sleeping 1 week");
  } else {
    sleepDuration = (uint64_t)REFRESH_HOURS * 60ULL * 60ULL * 1000000ULL;
    USBSerial.print("Sleeping for ");
    USBSerial.print(REFRESH_HOURS);
    USBSerial.println(" hours");
  }
  
  feedWatchdog();
  disconnectWiFi();
  
  // Configure wake sources
  USBSerial.println("Configuring wake sources:");
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  
  // 1. Timer wake (for scheduled refresh)
  esp_sleep_enable_timer_wakeup(sleepDuration);
  USBSerial.print("  ✓ Timer: ");
  USBSerial.print(REFRESH_HOURS);
  USBSerial.println(" hours");
  
  // 2. BOOT button wake (GPIO 0)
  const uint64_t button_mask = 1ULL << GPIO_NUM_0;
  esp_sleep_enable_ext1_wakeup(button_mask, ESP_EXT1_WAKEUP_ANY_LOW);
  USBSerial.println("  ✓ BOOT button (GPIO 0)");
  
  // 3. USB connection wake (GPIO 21 - PMU IRQ)
  // PMU will trigger IRQ on GPIO 21 when USB is plugged in
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);  // Wake on LOW (IRQ active low)
  USBSerial.println("  ✓ USB connection (GPIO 21 - PMU IRQ)");
  
  // Configure PMU to generate IRQ on USB insertion
  pmu_prepare_for_esp32_sleep();
  
  USBSerial.println("Going to sleep NOW...");
  USBSerial.println("Wake triggers: Timer | BOOT button | USB plug-in");
  USBSerial.flush();
  
  digitalWrite(ACT_LED_PIN, LOW);
  esp_task_wdt_delete(NULL);
  
  delay(100);  // Give serial time to flush
  
  esp_deep_sleep_start();
  // Never returns
}


// =============================================================================
// SECTION 20: ARDUINO SETUP
// =============================================================================

// RTC flag to prevent repeated setup screen draws on brownout reboot
RTC_DATA_ATTR bool setupScreenDrawn = false;

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Initialize PMU FIRST - before anything else
  // This prevents the PMU from doing anything weird with VBUS during boot
  Wire.begin(PMU_SDA, PMU_SCL);
  delay(50);  // Brief I2C stabilization
  
  if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    // Immediately accept USB power and stop PMU from interfering
    PMU.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
    PMU.disableVbusVoltageMeasure();
    PMU.disableSleep();
  }
  
  // Initialize USB composite device (CDC + HID)
  // USBSerial.begin() and USB.begin() both happen inside this function
  // in the correct order so the PC can enumerate successfully
  initUSBComposite();
  
  // Stabilize after USB enumeration
  delay(2000);
  
  // Load configuration FIRST (only once at boot)
  loadConfiguration();
  
  // Initialize pins
  pinMode(ACT_LED_PIN, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_CS, OUTPUT);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PWR_BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(ACT_LED_PIN, LOW);
  digitalWrite(EPD_RST, HIGH);
  digitalWrite(EPD_DC, LOW);
  digitalWrite(EPD_CS, HIGH);
  
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  USBSerial.println("\n======================================================================");
  USBSerial.println("              IBIS DASH V4.0 - USB Composite Device");
  USBSerial.println("              Board identifies as: Ibis Dash (CDC+HID)");
  USBSerial.println("======================================================================");
  USBSerial.print("Configured: "); USBSerial.println(isConfigured ? "YES" : "NO");
  USBSerial.print("Boot count: "); USBSerial.println(bootCount);
  USBSerial.print("USB Composite: "); USBSerial.println(usbCompositeStarted ? "ACTIVE" : "FAILED");
  
  initWatchdog();
  
  USBSerial.println("Checking factory reset...");
  if (checkFactoryReset()) {
    setupScreenDrawn = false;
    // Never returns
  }
  USBSerial.println("[OK] No factory reset");
  
  USBSerial.println("Checking crash loop...");
  if (checkCrashLoop()) {
    enterSafeMode();
  }
  USBSerial.println("[OK] No crash loop");
  
  USBSerial.println("Initializing PMU IRQ...");
  pmu_irq_init();
  USBSerial.println("[OK] PMU IRQ ready");
  
  // Verify PMU
  delay(900); // Let I2C settle before talking to PMU
  USBSerial.println("Verifying PMU...");
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    USBSerial.println("PMU init failed!");
  } else {
    USBSerial.println("[OK] PMU verified");
  }
  
  print_wakeup_reason();
  
  USBSerial.println("Configuring PMU for awake state...");
  pmu_configure_awake();
  USBSerial.println("[OK] PMU configured");
  
  // Delay for PMU stabilization
  USBSerial.println("PMU stabilization (2s)...");
  delay(2000);
  feedWatchdog();
  printBatteryStatus();
  
  // Check USB connection status EARLY
  bool usbConnected = PMU.isVbusIn();
  USBSerial.print("USB Status: ");
  USBSerial.println(usbConnected ? "CONNECTED" : "DISCONNECTED");
  
  // Initialize display
  USBSerial.println("Initializing display...");
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  epd_deep_init();
  display.init(115200, true, 2, false);
  display.setRotation(2);
  USBSerial.println("[OK] Display ready");
  
  // ===== MAIN LOGIC =====
  
  // Determine configuration state
  bool hasWifi = (WIFI_SSID.length() > 0);
  bool hasStrava = hasStravaCredentials();
  bool fullyConfigured = hasWifi && hasStrava;
  
  // Determine wake reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool isUsbWake = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0);
  bool isTimerWake = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  bool isButtonWake = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1);
  bool isPowerOn = (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED);
  
  USBSerial.print("Has WiFi: "); USBSerial.println(hasWifi ? "YES" : "NO");
  USBSerial.print("Has Strava: "); USBSerial.println(hasStrava ? "YES" : "NO");
  USBSerial.print("Fully Configured: "); USBSerial.println(fullyConfigured ? "YES" : "NO");
  USBSerial.print("Wake Type: ");
  if (isUsbWake) USBSerial.println("USB");
  else if (isTimerWake) USBSerial.println("TIMER");
  else if (isButtonWake) USBSerial.println("BUTTON");
  else if (isPowerOn) USBSerial.println("POWER-ON");
  else USBSerial.println("UNKNOWN");
  
  if (!fullyConfigured) {
    // ===== NOT CONFIGURED - SETUP MODE =====
    USBSerial.println("\n>>> SETUP REQUIRED <<<");
    
    // Draw setup screen only if:
    // 1. Never drawn before (setupScreenDrawn = false), OR
    // 2. Power-on boot (fresh start)
    // Skip drawing if USB wake (screen is already correct)
    
    if (!setupScreenDrawn || isPowerOn) {
      USBSerial.println("Drawing setup screen...");
      drawSetupScreen();
      setupScreenDrawn = true;
    } else if (isUsbWake) {
      USBSerial.println("USB wake - setup screen already displayed, staying idle");
    } else {
      USBSerial.println("Setup screen already drawn - skipping");
    }
    
    if (usbConnected) {
      USBSerial.println("\n>>> USB CONNECTED - Staying awake for setup <<<");
      // Stay awake in loop(), never sleep while on USB
    } else {
      USBSerial.println("\n>>> ON BATTERY - Going to sleep <<<");
      setupScreenDrawn = false;  // Reset for next boot
      go_to_deep_sleep();
    }
    
  } else {
    // ===== FULLY CONFIGURED - NORMAL OPERATION =====
    USBSerial.println("\n>>> FULLY CONFIGURED <<<");
    
    // Decide if we should fetch and draw:
    // - Timer wake: YES (scheduled refresh)
    // - Button wake: YES (manual refresh)
    // - USB wake: NO (just stay idle for commands)
    // - Power-on: YES (first boot)
    
    bool shouldUpdate = false;
    
    if (isUsbWake) {
      USBSerial.println("USB wake - staying idle, NOT fetching data");
      USBSerial.println("(Dashboard already on screen, waiting for commands)");
      shouldUpdate = false;
      
    } else if (isButtonWake) {
      USBSerial.println("Button wake - fetching fresh data");
      shouldUpdate = true;
      
    } else if (isTimerWake) {
      USBSerial.println("Timer wake - fetching scheduled update");
      shouldUpdate = true;
      
    } else if (isPowerOn || bootCount == 1) {
      USBSerial.println("First boot or power-on - fetching initial data");
      shouldUpdate = true;
      
    } else {
      USBSerial.println("Unknown wake - fetching data to be safe");
      shouldUpdate = true;
    }
    
    if (shouldUpdate) {
      if (wasManualWake) {
        USBSerial.println("Manual wake - stabilizing...");
        for (int i = 3; i > 0; i--) {
          USBSerial.print(i); USBSerial.println("...");
          delay(1000);
          feedWatchdog();
        }
        blinkLED(3);
      }
      
      bool forceFetch = (bootCount == 1) || wasManualWake || isTimerWake;
      updateStravaAndDisplay(forceFetch);
      blinkLED(2);
    } else {
      USBSerial.println("Skipping update - dashboard already current");
    }
    
    // Check USB status AFTER any updates
    usbConnected = PMU.isVbusIn();
    
    if (usbConnected) {
      USBSerial.println("\n>>> USB CONNECTED - Staying awake for editing <<<");
      // Stay awake in loop(), never sleep while on USB
    } else {
      USBSerial.println("\n>>> ON BATTERY - Going to sleep <<<");
      go_to_deep_sleep();
    }
  }
  
  bootCount++;
}


// =============================================================================
// SECTION 21: ARDUINO LOOP (runs when USB connected)
// =============================================================================

void loop() {
  static unsigned long lastBatteryCheck = 0;
  static unsigned long lastUsbCheck = 0;
  static int usbDisconnectCount = 0;
  
  feedWatchdog();
  
  // =====================================================================
  // FOOLPROOF USB DETECTION - TRIPLE VERIFICATION
  // Board NEVER sleeps unless ALL THREE checks confirm USB is gone:
  // 1. PMU says disconnected
  // 2. USBSerial is not working
  // 3. 10 consecutive checks (10 seconds) confirm disconnection
  // =====================================================================
  
  unsigned long currentMillis = millis();
  
  // Check USB status every 1 second
  if (currentMillis - lastUsbCheck >= 1000) {
    lastUsbCheck = currentMillis;
    
    // TRIPLE VERIFICATION:
    // Check 1: PMU reports USB status
    PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
    delay(50);
    bool pmuSaysConnected = PMU.isVbusIn();
    
    // Check 2: USBSerial connection is alive
    bool serialWorks = (bool)USBSerial;
    
    // Check 3: Combined verification
    bool usbActuallyConnected = pmuSaysConnected || serialWorks;
    
    if (!usbActuallyConnected) {
      // USB might be disconnected - start counting
      usbDisconnectCount++;
      
      USBSerial.print("⚠️  USB disconnect check ");
      USBSerial.print(usbDisconnectCount);
      USBSerial.print("/10 (PMU: ");
      USBSerial.print(pmuSaysConnected ? "ON" : "OFF");
      USBSerial.print(", USBSerial: ");
      USBSerial.print(serialWorks ? "WORKS" : "DEAD");
      USBSerial.println(")");
      
      // ONLY sleep after 10 consecutive "disconnected" readings (10 seconds)
      if (usbDisconnectCount >= 10) {
        USBSerial.println("\n╔════════════════════════════════════════════╗");
        USBSerial.println("║  USB CONFIRMED DISCONNECTED (10 checks)   ║");
        USBSerial.println("║  Going to sleep...                        ║");
        USBSerial.println("╚════════════════════════════════════════════╝\n");
        delay(500);
        go_to_deep_sleep();
        return;
      }
      
    } else {
      // USB is connected - reset counter
      if (usbDisconnectCount > 0) {
        USBSerial.print("[Ibis Dash] USB reconnected (was at ");
        USBSerial.print(usbDisconnectCount);
        USBSerial.println("/10) - staying awake");
        usbDisconnectCount = 0;
      }
    }
  }
  
  // Handle serial commands from Ibis Setup app
  handleSerialCommands();
  
  // Check if sleep was requested (from serial command)
  if (sleepRequested) {
    sleepRequested = false;
    
    // Triple-check USB before sleeping
    PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
    delay(50);
    bool pmuSaysConnected = PMU.isVbusIn();
    bool serialWorks = (bool)USBSerial;
    
    if (!pmuSaysConnected && !serialWorks) {
      USBSerial.println("\n>>> Executing requested sleep (USB verified off) <<<");
      delay(200);
      go_to_deep_sleep();
      return;
    } else {
      USBSerial.println("⚠️  Sleep requested but USB still connected - STAYING AWAKE");
      USBSerial.print("    PMU: ");
      USBSerial.print(pmuSaysConnected ? "CONNECTED" : "DISCONNECTED");
      USBSerial.print(", USBSerial: ");
      USBSerial.println(serialWorks ? "WORKS" : "DEAD");
    }
  }
  
  // ===== BOOT BUTTON - Manual refresh =====
  if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
    USBSerial.println("\n>>> BOOT BUTTON - MANUAL REFRESH <<<");
    while (digitalRead(BOOT_BUTTON_PIN) == LOW) { delay(10); feedWatchdog(); }
    blinkLED(2);
    
    loadConfiguration();
    bool hasWifi = (WIFI_SSID.length() > 0);
    bool hasStrava = hasStravaCredentials();
    
    if (hasWifi && hasStrava) {
      USBSerial.println("Fetching fresh Strava data...");
      updateStravaAndDisplay(true);
      
      // Triple-check before sleeping
      PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL);
      delay(50);
      bool pmuSaysConnected = PMU.isVbusIn();
      bool serialWorks = (bool)USBSerial;
      
      if (!pmuSaysConnected && !serialWorks) {
        USBSerial.println(">>> On battery - sleeping <<<");
        go_to_deep_sleep();
        return;
      } else {
        USBSerial.println(">>> USB connected - staying awake <<<");
      }
    } else {
      // Not fully configured, show setup screen
      if (!setupScreenDrawn) {
        drawSetupScreen();
        setupScreenDrawn = true;
      }
    }
  }
  
  // ===== KEY BUTTON - Reserved for future use =====
  if (digitalRead(USER_BUTTON_PIN) == LOW) {
    USBSerial.println("\n>>> KEY BUTTON <<<");
    while (digitalRead(USER_BUTTON_PIN) == LOW) { delay(10); feedWatchdog(); }
    blinkLED(1);
    // No action currently
  }
  
  // LED indicator - slow blink while ready for commands
  digitalWrite(ACT_LED_PIN, ((currentMillis / 1000) % 2) ? HIGH : LOW);
  
  // Battery check every 30 seconds
  if (currentMillis - lastBatteryCheck >= 30000) {
    printBatteryStatus();
    lastBatteryCheck = currentMillis;
  }
  
  delay(100);
}


