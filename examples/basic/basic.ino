#include <SerialWebLog.h>

/////////////////////////////////////
#define HOST_NAME "TEST"
/////////////////////////////////////

SerialWebLog mylog;

void setup() {
	mylog.setup(HOST_NAME, "MyWifi", "MyPass");
}


void loop() {
	mylog.update();
	delay(5);
}
