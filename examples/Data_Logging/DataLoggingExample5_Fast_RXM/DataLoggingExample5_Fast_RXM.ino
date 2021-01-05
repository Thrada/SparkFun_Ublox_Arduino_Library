/*
  Configuring the GNSS to automatically send RXM SFRBX and RAWX reports over I2C and log them to file on SD card
  without using callbacks and ** as fast as your module can go! **
  By: Paul Clark
  SparkFun Electronics
  Date: December 30th, 2020
  License: MIT. See license file for more information but you can
  basically do whatever you want with this code.

  This example shows how to configure the u-blox GNSS to send RXM SFRBX and RAWX reports automatically
  and log the data to SD card in UBX format without using callbacks and ** as fast as your module can go! **

  ** Please note: this example will only work on u-blox ADR or High Precision GNSS or Time Sync products **

  ** Please note: this example will only work on processors like the Artemis which have plenty of RAM available **

  Data is logged in u-blox UBX format. Please see the u-blox protocol specification for more details.
  You can replay and analyze the data using u-center:
  https://www.u-blox.com/en/product/u-center
  Or you can use (e.g.) RTKLIB to analyze the data and extract your precise location or produce
  Post-Processed Kinematic data:
  https://rtklibexplorer.wordpress.com/
  http://rtkexplorer.com/downloads/rtklib-code/

  This code is intended to be run on the MicroMod Data Logging Carrier Board using the Artemis Processor
  but can be adapted by changing the chip select pin and SPI definitions:
  https://www.sparkfun.com/products/16829
  https://www.sparkfun.com/products/16401

  Hardware Connections:
  Please see: https://learn.sparkfun.com/tutorials/micromod-data-logging-carrier-board-hookup-guide
  Insert the Artemis Processor into the MicroMod Data Logging Carrier Board and secure with the screw.
  Connect your GNSS breakout to the Carrier Board using a Qwiic cable.
  Connect an antenna to your GNSS board if required.
  Insert a formatted micro-SD card into the socket on the Carrier Board.
  Connect the Carrier Board to your computer using a USB-C cable.
  Ensure you have the SparkFun Apollo3 boards installed: http://boardsmanager/All#SparkFun_Apollo3
  This code has been tested using version 1.2.1 of the Apollo3 boards on Arduino IDE 1.8.13.
  Select "SparkFun Artemis MicroMod" as the board type.
  Press upload to upload the code onto the Artemis.
  Open the Serial Monitor at 115200 baud to see the output.

  To minimise I2C bus errors, it is a good idea to open the I2C pull-up split pad links on
  both the MicroMod Data Logging Carrier Board and the u-blox module breakout.

  Feel like supporting open source hardware?
  Buy a board from SparkFun!
  ZED-F9P RTK2: https://www.sparkfun.com/products/15136
  NEO-M8P RTK: https://www.sparkfun.com/products/15005
*/

#include <SPI.h>
#include <SD.h>
#include <Wire.h> //Needed for I2C to GNSS

#include <SparkFun_Ublox_Arduino_Library.h> //Click here to get the library: http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GPS myGPS;

File myFile; //File that all GNSS data is written to

#define sdChipSelect CS //Primary SPI Chip Select is CS for the MicroMod Artemis Processor. Adjust for your processor if necessary.

#define sdWriteSize 512 // Write data to the SD card in blocks of 512 bytes
#define fileBufferSize 32768 // Allocate 32KBytes of RAM for UBX message storage

unsigned long lastPrint; // Record when the last Serial print took place
unsigned long bytesWritten = 0; // Record how many bytes have been written to SD card

void setup()
{
  Serial.begin(115200);
  while (!Serial); //Wait for user to open terminal
  Serial.println("SparkFun u-blox Example");

  pinMode(LED_BUILTIN, OUTPUT); // Flash LED_BUILTIN each time we write to the SD card
  digitalWrite(LED_BUILTIN, LOW);

  Wire.begin(); // Start I2C communication

#if defined(AM_PART_APOLLO3)
  Wire.setPullups(0); // On the Artemis, we can disable the internal I2C pull-ups too to help reduce bus errors
#endif

  while (Serial.available()) // Make sure the Serial buffer is empty
  {
    Serial.read();
  }

  Serial.println(F("Press any key to start logging."));

  while (!Serial.available()) // Wait for the user to press a key
  {
    ; // Do nothing
  }

  delay(100); // Wait, just in case multiple characters were sent
  
  while (Serial.available()) // Empty the Serial buffer
  {
    Serial.read();
  }

  Serial.println("Initializing SD card...");

  // See if the card is present and can be initialized:
  if (!SD.begin(sdChipSelect))
  {
    Serial.println("Card failed, or not present. Freezing...");
    // don't do anything more:
    while (1);
  }
  Serial.println("SD card initialized.");

  // Create or open a file called "Fast_RXM.ubx" on the SD card.
  // If the file already exists, the new data is appended to the end of the file.
  myFile = SD.open("Fast_RXM.ubx", FILE_WRITE);
  if(!myFile)
  {
    Serial.println(F("Failed to create UBX data file! Freezing..."));
    while (1);
  }

  //myGPS.enableDebugging(); // Uncomment this line to enable lots of helpful GNSS debug messages on Serial
  //myGPS.enableDebugging(Serial, true); // Or, uncomment this line to enable only the important GNSS debug messages on Serial

  myGPS.disableUBX7Fcheck(); // RAWX data can legitimately contain 0x7F, so we need to disable the "7F" check in checkUbloxI2C

  // RAWX messages can be over 2KBytes in size, so we need to make sure we allocate enough RAM to hold all the data.
  // SD cards can occasionally 'hiccup' and a write takes much longer than usual. The buffer needs to be big enough
  // to hold the backlog of data if/when this happens.
  // getMaxFileBufferAvail will tell us the maximum number of bytes which the file buffer has contained.
  myGPS.setFileBufferSize(fileBufferSize); // setFileBufferSize must be called _before_ .begin

  if (myGPS.begin() == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing..."));
    while (1);
  }

  // Uncomment the next line if you want to reset your module back to the default settings with 1Hz navigation rate
  // (This will also disable any "auto" messages that were enabled and saved by other examples and reduce the load on the I2C bus)
  //myGPS.factoryDefault(); delay(5000);

  myGPS.setI2COutput(COM_TYPE_UBX); //Set the I2C port to output UBX only (turn off NMEA noise)
  myGPS.saveConfigSelective(VAL_CFG_SUBSEC_IOPORT); //Save (only) the communications port settings to flash and BBR

  // Modules like the ZED-F9P can produce RAW navigation data at rates of up to 25Hz but not while using all of the GNSS constellations.
  // Please consult the data sheet for the Performance figures for your module.
  // In this example we make sure GPS is enabled and then disable Galileo, GLONASS, BeiDou, SBAS and QZSS to achieve 25Hz.
  boolean success = true;
  success &= myGPS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GPS); // Make sure GPS is enabled (we must leave at least one major GNSS enabled!)
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_SBAS); // Disable SBAS
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_GALILEO); // Disable Galileo
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_BEIDOU); // Disable BeiDou
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_IMES); // Disable IMES
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_QZSS); // Disable QZSS
  success &= myGPS.enableGNSS(false, SFE_UBLOX_GNSS_ID_GLONASS); // Disable GLONASS
  if (!success) Serial.println(F("Warning: at least one call to enableGNSS failed!")); // Warn the user if any call to enableGNSS failed

  delay(2000); // Give the module some extra time to get ready

  //Produce 8 navigation solutions per second. That's a LOT of RAWX data!
  //The SD library and card need to be able to cope with the data rate too! You may need a faster SD library to go above 8Hz.
  myGPS.setNavigationFrequency(8);

  myGPS.setAutoRXMSFRBX(true, false); // Enable automatic RXM SFRBX messages: without callback; without implicit update
  
  myGPS.logRXMSFRBX(); // Enable RXM SFRBX data logging

  myGPS.setAutoRXMRAWX(true, false); // Enable automatic RXM RAWX messages: without callback; without implicit update
  
  myGPS.logRXMRAWX(); // Enable RXM RAWX data logging

  Serial.println(F("Press any key to stop logging."));

  lastPrint = millis(); // Initialize lastPrint
}

void loop()
{
  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  myGPS.checkUblox(); // Check for the arrival of new data and process it.
  myGPS.checkCallbacks(); // Check if any callbacks are waiting to be processed.

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  while (myGPS.fileBufferAvailable() >= sdWriteSize) // Check to see if we have at least sdWriteSize waiting in the buffer
  {
    digitalWrite(LED_BUILTIN, HIGH); // Flash LED_BUILTIN each time we write to the SD card
  
    uint8_t myBuffer[sdWriteSize]; // Create our own buffer to hold the data while we write it to SD card

    myGPS.extractFileBufferData((uint8_t *)&myBuffer, sdWriteSize); // Extract exactly sdWriteSize bytes from the UBX file buffer and put them into myBuffer

    myFile.write(myBuffer, sdWriteSize); // Write exactly sdWriteSize bytes from myBuffer to the ubxDataFile on the SD card

    bytesWritten += sdWriteSize; // Update bytesWritten

    // In case the SD writing is slow or there is a lot of data to write, keep checking for the arrival of new data
    myGPS.checkUblox(); // Check for the arrival of new data and process it.
    myGPS.checkCallbacks(); // Check if any callbacks are waiting to be processed.

    digitalWrite(LED_BUILTIN, LOW); // Turn LED_BUILTIN off again
  }

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  if (millis() > (lastPrint + 1000)) // Print bytesWritten once per second
  {
    Serial.print(F("The number of bytes written to SD card is: ")); // Print how many bytes have been written to SD card
    Serial.println(bytesWritten);

    uint16_t maxBufferBytes = myGPS.getMaxFileBufferAvail(); // Get how full the file buffer has been (not how full it is now)
    
    //Serial.print(F("The maximum number of bytes which the file buffer has contained is: ")); // It is a fun thing to watch how full the buffer gets
    //Serial.println(maxBufferBytes);

    if (maxBufferBytes > ((fileBufferSize / 5) * 4)) // Warn the user if fileBufferSize was more than 80% full
    {
      Serial.println(F("Warning: the file buffer has been over 80% full. Some data may have been lost."));
    }
    
    lastPrint = millis(); // Update lastPrint
  }

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

  if (Serial.available()) // Check if the user wants to stop logging
  {
    myGPS.setAutoRXMSFRBX(false, false); // Disable the automatic RXM SFRBX messages
    myGPS.setAutoRXMRAWX(false, false); // Disable the automatic RXM RAWX messages

    delay(1000); // Allow time for any remaining messages to arrive
    myGPS.checkUblox(); // Process any remaining data

    uint16_t remainingBytes = myGPS.fileBufferAvailable(); // Check if there are any bytes remaining in the file buffer
    
    while (remainingBytes > 0) // While there is still data in the file buffer
    {
      digitalWrite(LED_BUILTIN, HIGH); // Flash LED_BUILTIN while we write to the SD card
      
      uint8_t myBuffer[sdWriteSize]; // Create our own buffer to hold the data while we write it to SD card

      uint16_t bytesToWrite = remainingBytes; // Write the remaining bytes to SD card sdWriteSize bytes at a time
      if (bytesToWrite > sdWriteSize)
      {
        bytesToWrite = sdWriteSize;
      }
  
      myGPS.extractFileBufferData((uint8_t *)&myBuffer, bytesToWrite); // Extract bytesToWrite bytes from the UBX file buffer and put them into myBuffer

      myFile.write(myBuffer, bytesToWrite); // Write bytesToWrite bytes from myBuffer to the ubxDataFile on the SD card

      bytesWritten += bytesToWrite; // Update bytesWritten

      remainingBytes -= bytesToWrite; // Decrement remainingBytes
    }

    digitalWrite(LED_BUILTIN, LOW); // Turn LED_BUILTIN off

    Serial.print(F("The total number of bytes written to SD card is: ")); // Print how many bytes have been written to SD card
    Serial.println(bytesWritten);

    uint16_t maxBufferBytes = myGPS.getMaxFileBufferAvail(); // Show how full the file buffer has been (not how full it is now)
    Serial.print(F("The maximum number of bytes which the file buffer has contained is: "));
    Serial.println(maxBufferBytes);

    myFile.close(); // Close the data file
    
    Serial.println(F("Logging stopped. Freezing..."));
    while(1); // Do nothing more
  }

  // =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
}
