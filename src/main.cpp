#include "config.h"
#include "secure.h"

TTGOClass *ttgo = nullptr;
AXP20X_Class *power = nullptr;

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
GxEPD_Class *ePaper = nullptr;
PCF8563_Class *rtc = nullptr;
Button2 *btn = nullptr;
bool pwIRQ = false;
uint32_t loopMillis = 0;

WiFiClient wifiClient;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 3;
const int daylightOffset_sec = 0;

#define DHTPIN 33
DHTesp dht;
BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
									// Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

static int16_t timeX = 5, timeY = 5;

void setupDisplay()
{
	u8g2Fonts.begin(*ePaper);                   // connect u8g2 procedures to Adafruit GFX
	u8g2Fonts.setFontMode(1);                   // use u8g2 transparent mode (this is default)
	u8g2Fonts.setFontDirection(0);              // left to right (this is default)
	u8g2Fonts.setForegroundColor(GxEPD_BLACK);  // apply Adafruit GFX color
	u8g2Fonts.setBackgroundColor(GxEPD_WHITE);  // apply Adafruit GFX color
	u8g2Fonts.setFont(u8g2_font_inr38_mn);      // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
}

void connectNet(uint32_t recon_delay = 500)
{
	Serial.printf("Connecting to %s \n", SSID_NAME);
	WiFi.begin(SSID_NAME, SSID_PASS);
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(recon_delay);
	}
	Serial.println(" CONNECTED");
}

void mainPage(bool fullScreen)
{
	uint16_t tbw, tbh;
	static uint16_t x, y;
	static uint16_t timeW, timeH;
	static uint8_t hh = 0, mm = 0;
	char buff[64] = "";

	RTC_Date d = rtc->getDateTime();
	if (mm == d.minute && !fullScreen)
	{
		return ;
	}
	mm = d.minute;
	hh = d.hour;
	if (fullScreen)
	{
		ePaper->fillScreen(GxEPD_WHITE);
	}

	// время
	snprintf(buff, sizeof(buff), "%02d:%02d", hh, mm);
	u8g2Fonts.setFont(u8g2_font_inr33_mn);
	if (fullScreen)
	{
		timeW = u8g2Fonts.getUTF8Width(buff);
		timeH = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
	}
	else
	{
		ePaper->fillRect(timeX, timeY + 20, timeW, timeH, GxEPD_WHITE);
	}
	u8g2Fonts.setCursor(timeX, timeY + u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent());
	u8g2Fonts.print(buff);
	if (!fullScreen)
	{
		ePaper->updateWindow(timeX, timeY, timeW, timeH, false);
	}

	// питание - заряд
	char pwr[64] = "";
	snprintf(pwr, sizeof(pwr), "%d%%", power->getBattPercentage());
	u8g2Fonts.setFont(u8g2_font_10x20_t_cyrillic);
	tbw = u8g2Fonts.getUTF8Width(pwr);
	tbh = u8g2Fonts.getFontAscent() - u8g2Fonts.getFontDescent();
	x = ePaper->width() - tbw - 10;
	y = tbh;
	ePaper->fillRect(x - 2, y - 2, tbw + 4, tbh + 4, GxEPD_WHITE);
	u8g2Fonts.setCursor(x, y + 10);
	u8g2Fonts.print(pwr);
	if (!fullScreen)
	{
		ePaper->updateWindow(x - 2, y - 2, tbw + 4, tbh + 4, false);
	}

	// FIXME не полноэкранно обновлять только значения
	// TODO проверить не полноэкранное обновление на разных значениях
	//float h = dht.getHumidity(); //Измеряем влажность
	//float t = dht.getTemperature(); //Измеряем температуру
	float t(NAN), h(NAN), p(NAN), a(NAN);
	BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
	BME280::PresUnit presUnit(BME280::PresUnit_inHg);
	bme.read(p, t, h, tempUnit, presUnit);
	p = p * 25.4; // inch to mm

	ThingSpeak.setField(TS_F_TEMPERATURE, t);
	ThingSpeak.setField(TS_F_HUDIMITY, h);
	ThingSpeak.setField(TS_F_PRESSURE, p);
	ThingSpeak.writeFields(TS_CH_ID, TS_CH_WKEY);

	char s[64] = "";
	char s2[64] = "";
	char s3[64] = "";
	//if (dht.getStatus() != 0)
	if (false)
	{
		snprintf(s, sizeof(s), "%s", dht.getStatusString());
		snprintf(s2, sizeof(s2), "", "");
		snprintf(s3, sizeof(s3), "", "");
	}
	else
	{
		snprintf(s, sizeof(s), "Влажность: %2.2f%%", h);
		snprintf(s2, sizeof(s2), "Температура: %2.2f*C", t);
		snprintf(s3, sizeof(s3), "Давление: %2.2fмрс", p);
	}
	u8g2Fonts.setFont(u8g2_font_10x20_t_cyrillic);
	ePaper->fillRect(0, 100, 240, 45, GxEPD_WHITE);
	u8g2Fonts.setCursor(0, 100);
	u8g2Fonts.print(s);
	u8g2Fonts.setCursor(0, 115);
	u8g2Fonts.print(s2);
	u8g2Fonts.setCursor(0, 130);
	u8g2Fonts.print(s3);
	if (!fullScreen)
	{
		ePaper->updateWindow(0, 100, 240, 45, false);
	}

	if (fullScreen)
	{
		ePaper->update();
	}
}

void setup()
{
	Serial.begin(115200);

	//Get watch instance
	ttgo = TTGOClass::getWatch();

	// Initialize the hardware
	ttgo->begin();

	power = ttgo->power;

	btn = ttgo->button;

	rtc = ttgo->rtc;

	//dht.setup(DHTPIN, DHTesp::DHT11);
	Wire.begin();
	while(!bme.begin())
	{
		Serial.println("Could not find BME280 sensor!");
		delay(1000);
	}
	switch(bme.chipModel())
	{
		case BME280::ChipModel_BME280:
			Serial.println("Found BME280 sensor! Success.");
			break;
		case BME280::ChipModel_BMP280:
			Serial.println("Found BMP280 sensor! No Humidity available.");
			break;
		default:
			Serial.println("Found UNKNOWN sensor! Error!");
	}

	ePaper = ttgo->ePaper;

	// ADC monitoring must be enabled to use the AXP202 monitoring function
	power->adc1Enable(AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);

	// Turn on power management button interrupt
	power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);

	// Clear power interruption
	power->clearIRQ();

	// Set MPU6050 to sleep
	ttgo->mpu->setSleepEnabled(true);

	// Set Pin to interrupt
	pinMode(AXP202_INT, INPUT_PULLUP);
	attachInterrupt(AXP202_INT, [] {
		pwIRQ = true;
	}, FALLING);


	btn->setPressedHandler([]() {
		// esp_restart();

		/*if (touch_vaild) {
			ePaper->fillScreen(GxEPD_WHITE);
			ePaper->update();
		} else {*/
			ePaper->fillScreen(GxEPD_WHITE);
			mainPage(true);
		//}
	});

	btn->setDoubleClickHandler([]() {
		esp_restart();
	});

	// Initialize the ink screen
	setupDisplay();

	//connect to WiFi
	connectNet();

	//init and get the time
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

	struct tm timeinfo;
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time, Restart in 3 seconds");
		delay(3000);
		esp_restart();
		while (1);
	}
	Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
	Serial.println("Time synchronization succeeded");
	// Sync local time to external RTC
	rtc->syncToRtc();

	// Use compile time as RTC input time
	//rtc->check();

	ThingSpeak.begin(wifiClient);


	// Initialize the interface
	mainPage(true);

	// Reduce CPU frequency
	//setCpuFrequencyMhz(40);

}

void loop()
{



	btn->loop();

	if (pwIRQ)
	{
		pwIRQ = false;

		// Get interrupt status
		power->readIRQ();

		if (power->isPEKShortPressIRQ())
		{
			//do something
		}
		// After the interruption, you need to manually clear the interruption status
		power->clearIRQ();
	}

	if (millis() - loopMillis > 1000)
	{
		loopMillis = millis();

		// Connect or reconnect to WiFi
		if (WiFi.status() != WL_CONNECTED)
		{
			connectNet(5000);
		}

		// Partial refresh
		mainPage(false);
	}



}
