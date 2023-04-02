#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Adafruit_NeoPixel.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY ""

/* 3. Define the RTDB URL */
#define DATABASE_URL "" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL ""
#define USER_PASSWORD ""

// Define Firebase Data object
FirebaseData stream;
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel STRIP_CH_1 = Adafruit_NeoPixel(60, 1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel STRIP_CH_2 = Adafruit_NeoPixel(60, 2, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel STRIP_CH_3 = Adafruit_NeoPixel(60, 3, NEO_GRB + NEO_KHZ800);

unsigned long sendDataPrevMillis = 0;
unsigned long colorWipeaPrevMillis = 0;

String parentPath = "/channel_porops";
String childPath[4] = {"/ch_1", "/ch_2"};

int count = 0;

volatile bool dataChanged = false;

// Brightness
bool brightness_power = false;
float brightness_curr = 0;
float brightness_prev = 0;

void streamCallback(MultiPathStreamData stream)
{
  size_t numChild = sizeof(childPath) / sizeof(childPath[0]);

   for (size_t i = 0; i < numChild; i++)
   {
    if (stream.get(childPath[i]))
     {
      Serial.printf("path: %s, event: %s, type: %s, value: %s%s", stream.dataPath.c_str(), stream.eventType.c_str(), stream.type.c_str(), stream.value.c_str(), i < numChild - 1 ? "\n" : "");
     }
   }

  Serial.println(numChild);

  // This is the size of stream payload received (current and max value)
  // Max payload size is the payload size under the stream path since the stream connected
  // and read once and will not update until stream reconnection takes place.
  // This max value will be zero as no payload received in case of ESP8266 which
  // BearSSL reserved Rx buffer size is less than the actual stream payload.
  // Serial.printf("Received stream payload size: %d (Max. %d)\n\n", stream.payloadLength(), stream.maxPayloadLength());

  // Due to limited of stack memory, do not perform any task that used large memory here especially starting connect to server.
  // Just set this flag and check it status later.
  dataChanged = true;
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void setup()
{

  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  // Or use legacy authenticate method
  // config.database_url = DATABASE_URL;
  // config.signer.tokens.legacy_token = "<database secret>";

  // To connect without auth in Test Mode, see Authentications/TestMode/TestMode.ino

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  // Recommend for ESP8266 stream, adjust the buffer size to match your stream data size
#if defined(ESP8266)
  stream.setBSSLBufferSize(2048 /* Rx in bytes, 512 - 16384 */, 512 /* Tx in bytes, 512 - 16384 */);
#endif

  // The data under the node being stream (parent path) should keep small
  // Large stream payload leads to the parsing error due to memory allocation.

  // The MultiPathStream works as normal stream with the payload parsing function.

  if (!Firebase.beginMultiPathStream(stream, parentPath))
    Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());

  Firebase.setMultiPathStreamCallback(stream, streamCallback, streamTimeoutCallback);


  STRIP_CH_1.begin();
  STRIP_CH_2.begin();
  STRIP_CH_3.begin();
  STRIP_CH_1.setBrightness(50);
  STRIP_CH_2.setBrightness(50);
  STRIP_CH_3.setBrightness(50);
  STRIP_CH_1.show();
  STRIP_CH_2.show();
  STRIP_CH_3.show();

  String read_brightness_curr = "0";

  Firebase.RTDB.getBool(&fbdo, "/channels/7/power", &brightness_power);
  Firebase.RTDB.getString(&fbdo, "/channels/7/brightness", &read_brightness_curr);

  brightness_curr = read_brightness_curr.toInt();
}

void onChange_FirebaseData()
{
  if (!dataChanged)
    return;
  dataChanged = false;

  // read Firebase
  String read_brightness_curr = "0";
  Firebase.RTDB.getBool(&fbdo, "/channels/7/power", &brightness_power);
  Firebase.RTDB.getString(&fbdo, "/channels/7/brightness", &read_brightness_curr);
  brightness_curr = read_brightness_curr.toInt();

  // Serial.println(255 * brightness.toInt() / 100);
}

void onChange_Brightness()
{
  // if( ! brightness_power ) brightness_curr = 0;

  if (brightness_prev != brightness_curr)
  {
    // Fade value transition
    if (brightness_prev >= brightness_curr - 1 && brightness_prev <= brightness_curr + 1)
    {
      brightness_prev = brightness_curr;
    }
    if (brightness_prev < brightness_curr)
    {
      brightness_prev += 0.5;
    }
    if (brightness_prev > brightness_curr)
    {
      brightness_prev -= 0.5;
    }
  }
  sendDataPrevMillis = millis();
}

void colorWipe(Adafruit_NeoPixel strip, uint32_t c)
{
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, c);
    strip.show();
  }
}

void loop()
{

  onChange_FirebaseData();
  if ((millis() - sendDataPrevMillis > 5 || sendDataPrevMillis == 0))
    onChange_Brightness();

  analogWrite(D8, 255 * brightness_prev / 100);

  if (millis() - colorWipeaPrevMillis > 10 || colorWipeaPrevMillis == 0)
  {
    colorWipe(STRIP_CH_1, STRIP_CH_1.Color(255, 0, 0));
    colorWipe(STRIP_CH_2, STRIP_CH_2.Color(0, 255, 0));
    colorWipe(STRIP_CH_3, STRIP_CH_3.Color(0, 0, 255));
  }
}
