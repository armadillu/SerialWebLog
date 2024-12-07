#include "SerialWebLog.h"
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266mDNS.h>
#endif
#ifdef ARDUINO_ARCH_ESP32
#include <ESPmDNS.h>
#endif


#include <ezTime.h>

#define LOG_START 			"\n----------------------------------------------\n"
#define TIMESTAMP_FORMAT 	(timeStatus() == timeSet ? UTC.dateTime("d-M-Y H:i:s T"): uptime() )
#define TIMESTAMP_LOG  		(TIMESTAMP_FORMAT + " >> ")

String uptime(){
	static char aux[32];
	sprintf(aux, "[boot + %d sec]", millis()/1000);
	return String(aux);
}

uint64_t millis64() {
	static uint32_t low32, high32;
	uint32_t new_low32 = millis();
	if (new_low32 < low32) high32++;
	low32 = new_low32;
	return (uint64_t) high32 << 32 | low32;
}

void SerialWebLog::connectWifi(){
	WiFi.mode(WIFI_STA);
	WiFi.disconnect(true);

	int8_t connectionResult = WL_DISCONNECTED;

	while(connectionResult != WL_CONNECTED){

		if(!connectOnlyToBssid){
			this->printf("Trying to connect to \"%s\" ...\n", SSID);
			WiFi.setAutoReconnect(true);
			WiFi.begin(SSID, wifiPass);
		}else{
			this->printf("Trying to connect to \"%s\" BSSID %02x:%02x:%02x:%02x:%02x:%02x\n", SSID, targetBssid[0], targetBssid[1], targetBssid[2], targetBssid[3], targetBssid[4], targetBssid[5]);
			WiFi.setAutoReconnect(false);
			WiFi.begin(SSID, wifiPass, 0, targetBssid);
		}		
		
		connectionResult = WiFi.waitForConnectResult(5000);

		if(connectionResult == WL_CONNECTED){
			this->printf("Connected to \"%s\" IP address \"%s\"\n", SSID, WiFi.localIP().toString().c_str());		
		}else{
			this->printf("Can't connect (%d)! Retrying in 5 seconds...\n", connectionResult);
			delay(5000);
		}
		yield();
	}	
}


void SerialWebLog::setup(const char * hostname, const char * SSID, const char * wifi_pass, const uint8_t* bssid){

	if(webserver == nullptr){

		textLog.reserve(maxLogSize);

		Serial.begin(9600);
		this->print(LOG_START);
		this->printf("Starting \"%s\" MicroController...\n", hostname);
		#ifdef ARDUINO_ARCH_ESP8266
		this->printf("Restart Reason: %s\n", ESP.getResetReason().c_str());
		#endif

		//register to some events esp8266
		#ifdef ARDUINO_ARCH_ESP8266
		stationConnectedHandler = WiFi.onStationModeConnected([this](const WiFiEventStationModeConnected& evt){
			this->printf("WIFI Connected to \"%02x:%02x:%02x:%02x:%02x:%02x\" - channel %d\n", evt.bssid[0], evt.bssid[1],
				evt.bssid[2], evt.bssid[3],evt.bssid[4], evt.bssid[5], evt.channel);
		});

		stationDisconnectedHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& evt){
			this->printf("WIFI Disconnected. Reason: %d - \"%02x:%02x:%02x:%02x:%02x:%02x\"\n", evt.reason, evt.bssid[0], evt.bssid[1],
				evt.bssid[2], evt.bssid[3],evt.bssid[4], evt.bssid[5]);
			if(connectOnlyToBssid){
				if (evt.reason != WIFI_DISCONNECT_REASON_ASSOC_LEAVE){ //dont want to trigger a reconnect as this event its not really a disconnect... sigh
					shouldReconnect = true;
				}
			}
		});
		#endif

		//register to some events esp32
		#ifdef ARDUINO_ARCH_ESP32
		wifi32connectEvent = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info){
			uint8_t * bssid = &info.wifi_sta_connected.bssid[0];
			this->printf("WiFi connected to: \"%02x:%02x:%02x:%02x:%02x:%02x\", Channel: %d\n", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], info.wifi_sta_connected.channel);
		}, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);

		wifi32disconnectEvent = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info){
			uint8_t * bssid = &info.wifi_sta_disconnected.bssid[0];
			this->printf("WiFi Disconnected. Reason: %d - \"%02x:%02x:%02x:%02x:%02x:%02x\"\n", info.wifi_sta_disconnected.reason, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		}, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
	    #endif

		#ifdef ARDUINO_ARCH_ESP8266
		WiFi.setPhyMode(WIFI_PHY_MODE_11G);
		WiFi.setSleepMode(WIFI_NONE_SLEEP);
		#endif

		this->SSID = String(SSID);
		this->wifiPass = String(wifi_pass);
		WiFi.setHostname(hostname);
		if(bssid == nullptr){
			connectOnlyToBssid = false;			
		}else{
			connectOnlyToBssid = true;
			memcpy(targetBssid, bssid, 6);
			this->printf("WiFi connection constrained to a single BSSID (%02x:%02x:%02x:%02x:%02x:%02x)\n", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
		}
		connectWifi();

		this->printf("NTP Sync start...\n");
		//setServer("1.es.pool.ntp.org");
		//setInterval(60 * 60 * 12 /*seconds*/); //every 12 hours
		waitForSync();
		this->printf("NTP Sync done!\n");

		webserver = new WEB_SERVER_TYPE(80);

		webserver->on("/", [this](){handleRoot();});
		webserver->on("/reset", [this](){handleReset();});
		webserver->on("/log", [this](){handleLog();});
		webserver->on("/clearLog", [this](){clearLog();});
		
		webserver->begin();

		if (!MDNS.begin(hostname)){
			this->print("Error setting up mDNS responder!"); 
		}else{
			this->print("mDNS started!\n"); 
		}
		MDNS.addService("http", "tcp", 80);

	}else{
		this->print("SerialWebLog:: Trying to setup twice! Developer Error!\n");
	}
}


WEB_SERVER_TYPE* SerialWebLog::getServer(){
	return webserver;
}


size_t SerialWebLog::printf(const char *format, ...) {
	va_list arg;
	va_start(arg, format);
	static char temp[512];
	char* buffer = temp;
	size_t len = vsnprintf(temp, sizeof(temp), format, arg);
	va_end(arg);
	if (len > sizeof(temp) - 1) {
		buffer = new (std::nothrow) char[len + 1];
		if (!buffer) {
			return 0;
		}
		va_start(arg, format);
		vsnprintf(buffer, len + 1, format, arg);
		va_end(arg);
	}
	Serial.print(buffer);
	textLog += TIMESTAMP_LOG + String(buffer);
	trimLog();
	if (buffer != temp) {
		delete[] buffer;
	}
	return len;	
}

void SerialWebLog::print(const String &s) {
	textLog += TIMESTAMP_LOG + s;
	Serial.print(s);
	trimLog();
}

void SerialWebLog::print(const char* text){
	textLog += TIMESTAMP_LOG + String(text);
	Serial.print(String(text));
	trimLog();
}

void SerialWebLog::update(){
	if(shouldReconnect){
		shouldReconnect = false;
		connectWifi();
	}
	webserver->handleClient();
	#ifdef ARDUINO_ARCH_ESP8266
	MDNS.update();
	#endif
}

void SerialWebLog::handleLog(){
	webserver->send(200, "text/plain", textLog);
}

void SerialWebLog::handleReset(){
	webserver->send(200, "text/plain", "Reset OK!");
	delay(300);
	ESP.restart();
}

void SerialWebLog::clearLog(){		
	webserver->send(200, "text/plain", "Clear Log OK!");
	textLog = "";
	Serial.print("Cleared Log!\n");
	print(LOG_START);
}

void SerialWebLog::trimLog(){
	if(textLog.length() > maxLogSize){
		uint32_t index = 0;
		bool done = false;
		while(!done){
			index++;
			if(index >= textLog.length() - 1){
				done = true;
				//Serial.print("Can't trim end of log!\n");
				textLog = "";
			}else{
				if(textLog[index] == '\n'){
					if (textLog.length() - index < maxLogSize ){
						//Serial.printf("trim log at index %d!", index);
						done = true;
						textLog = "(...)" + textLog.substring(index);					
					}
				}
			}
		}
	}
}


void SerialWebLog::handleRoot(){

	float mem = ESP.getFreeHeap() / 1024.0f;

	static String a1 = "<html><header><style>body{line-height: 150%; text-align:center;}</style><title>";
	static String a2 = "</title></header><body><br><h1>";
	static String b = "</h1><br><iframe src='/log' name='a' width='80%' height='70%'></iframe> <br><br>";
	static String menu = "<br><a href='/log' target='a'>Log</a> | <a href='/reset' target='a'>Reset ESP</a> | <a href='/clearLog' target='a'>Clear Log</a>";
	static String closure = "</p></body></html>";

	String hstname = String(WiFi.getHostname());

	static char aux[128];
	uint64_t now = millis64();
	uint32_t min = (now / (1000 * 60))%60;
	uint32_t hour = (now / (1000 * 60 * 60))%24;
	uint32_t day = now / (1000 * 60 * 60 * 25);

	String mydate = TIMESTAMP_FORMAT;
	String chip;
	#ifdef ARDUINO_ARCH_ESP8266
	chip = "ESP8266 | Build date: " + String(__TIMESTAMP__);//ESP.getFullVersion();
	#endif
	#ifdef ARDUINO_ARCH_ESP32
	chip = String(ESP.getChipModel()) + " | Built: '" + String(__TIMESTAMP__) + "'";
	#endif
	sprintf(aux, "%.1f KB free | WiFi RSSI: %d dBm<br>Date: %s<br>Uptime: %d days, %d hours, %d minutes.", mem, WiFi.RSSI(), mydate.c_str(), day, hour, min);

	webserver->send(200, "text/html", a1 + hstname + a2 + hstname + b + chip + menu + compiledExtraHTML + String(aux) + closure);
}


void SerialWebLog::addHtmlExtraMenuOption(const String & title, const String & URL){
	extraHTML[title] = URL;
	compiledExtraHTML = "";
	for (auto & it : extraHTML){
		compiledExtraHTML += " | <a href='" + it.second + "' target='a'>" + it.first + "</a> ";
	}
	compiledExtraHTML += "<br>";
}
