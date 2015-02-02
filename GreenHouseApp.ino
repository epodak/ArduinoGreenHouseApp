
//ethernet SD chip uses digital pins 10, 11, 12, and 13 for SPI buss.


#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>



class SDCard
{
public:

	void initRoot(int chipSelected)
	{
		chipSelect = chipSelected;
		if (!card.init(SPI_HALF_SPEED, chipSelect)){
			Serial.println("initialization failed.");
			return;
		}

		if (!volume.init(card)) {
			Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
			return;
		}

		root.openRoot(volume);

	}

	void SDWriteLogPins(String DateTimeStamp)
	{
		initRoot(chipSelect);

		
		int numberOfPins = 6; //number of analog pins to log
		String dataString = DateTimeStamp + ":";
		for (int analogPin = 0; analogPin < numberOfPins; analogPin++) {
			int sensor = analogRead(analogPin);
			dataString += String(sensor);
			if (analogPin < (numberOfPins - 1)) {
				dataString += ",";
			}
		}

		SdFile file;
		file.open(root, "datalog.txt", O_CREAT | O_APPEND | O_WRITE);    //Open or create the file 'name' in 'root' for writing to the end of the file.
		if (file.isFile()){

			file.println(dataString);
			file.close();
			// print to the serial port too:
			Serial.println(dataString);
		}
		else{
			Serial.println("error opening datalog.txt");
		}
	}

	void SDGetCardInfo()
	{



		initRoot(chipSelect);

		// print the type and size of the first FAT-type volume
		uint32_t volumesize;
		Serial.print("\nVolume type is FAT");
		Serial.println(volume.fatType(), DEC);
		Serial.println();

		volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
		volumesize *= volume.clusterCount();       // we'll have a lot of clusters
		volumesize *= 512;                            // SD card blocks are always 512 bytes
		Serial.print("Volume size (Mbytes): ");
		volumesize /= 1024;
		Serial.println(volumesize);

		// list all files in the card with date and size
		root.ls(LS_R | LS_DATE | LS_SIZE);
		Serial.println();
	}

	//boolean WebFileExists(){
	//	if(!SD.exists("index.htm")) {
	//		Serial.println("ERROR - Can't find index.htm file!");
	//		return false;  // can't find index file
	//	}
	//	else
	//	{
	//		return true;
	//	}
	//	
	//}

private:
	int chipSelect;
	Sd2Card card;
	SdVolume volume;
	SdFile root;

};


// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 4;
const int outputEthernetShieldSDPin = 10; //to turn on SD chip (disables ethernet)
const int outputEthernetShieldEthernetPin = 4; //to turn on Ethernet chip (disables SD)
SDCard *card;

// MAC address from Ethernet shield sticker under board
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 0, 0, 20); // IP address, may need to change depending on network
EthernetServer server(80);  // create a server at port 80

File webFile;

void setup()
{

	// Open serial communications and wait for port to open:
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for Leonardo only
	}

	card = new SDCard();
	card->initRoot(chipSelect);
	//card->WebFileExists();


	Ethernet.begin(mac, ip);  // initialize Ethernet device
	server.begin();           // start to listen for clients

	// make sure that the default chip select pin is set to
	// output, even if you don't use it:
	/*pinMode(outputEthernetShieldSDPin, OUTPUT);*/
	//digitalWrite(outputEthernetShieldSDPin, HIGH);

	//pinMode(outputEthernetShieldEthernetPin, OUTPUT);



}

void loop()
{

	checkWebRequest();

	card->SDWriteLogPins("2015-01-12 12:00:01");
	card->SDGetCardInfo();

	delay(3000);

}


void checkWebRequest()
{

	EthernetClient client = server.available();  // try to get client
	if (client) {  // got client?
		boolean currentLineIsBlank = true;
		while (client.connected()) {
			if (client.available()) {   // client data available to read
				char c = client.read(); // read 1 byte (character) from client
				// last line of client request is blank and ends with \n
				// respond to client only after last line received
				if (c == '\n' && currentLineIsBlank) {
					// send a standard http response header
					client.println("HTTP/1.1 200 OK");
					client.println("Content-Type: text/html");
					client.println("Connection: close");
					client.println();
					// send web page
					webFile = SD.open("index.htm");        // open web page file
					if (webFile) {
						while (webFile.available()) {
							client.write(webFile.read()); // send web page to client
						}
						webFile.close();
					}
					break;
				}
				// every line of text received from the client ends with \r\n
				if (c == '\n') {
					// last character on line of received text
					// starting new line with next character read
					currentLineIsBlank = true;
				}
				else if (c != '\r') {
					// a text character was received from client
					currentLineIsBlank = false;
				}
			} // end if (client.available())
		} // end while (client.connected())
		delay(1000);      // give the web browser time to receive the data
		client.stop(); // close the connection
	}
	else{
		Serial.println(" - no clients.");
	} // end if (client)

}
