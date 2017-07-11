// Inspiring links and other info that helped me make this:
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiAccessPoint/WiFiAccessPoint.ino
// http://www.esp8266.com/viewtopic.php?f=29&t=12124
// https://github.com/costonisp/ESP8266-I2C-OLED/blob/master/ESP_Messenger_v1.0
// https://arduino.stackexchange.com/questions/19795/how-to-read-bitmap-image-on-arduino
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/FSBrowser/FSBrowser.ino
// http://stackoverflow.com/questions/9072320/split-string-into-string-array
// the adafruit SSD1306_128x32_i2c example
// and of course https://en.wikipedia.org/wiki/BMP_file_format


//Connections Oled - ESP8266:
//
//Oled-SDA connected to GPIO0 of ESP
//
//Oled-SCL connected to GPIO2 of ESP The I2C Oled board used has a SDD1306 controller. 



#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_address  0x3C                            
#define SSID "Prntyrmsg"                              
int IPparts[] = {10,10,10,1};
#define PASS "accessgranted"                          
IPAddress apIP(IPparts[0], IPparts[1], IPparts[2], IPparts[3]);
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

boolean rotated = false;

String htmlpage =
  "<p>"
  "<center>"
  "<h1>Print your message!</h1>"
  "<form action='msg'><pre>Message to print: <input type='text' name='msg' size=50 autofocus> scroll? <input type='checkbox' name='scroll' value='scroll'> text size: <select name='size'><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option> </select> <input type='submit' value='Submit'> </form>"
  "<br>  <form enctype='multipart/form-data' action='bmp' method='POST'> Trying for a bitmap <input type='file' name='bodybmp' accept='.bmp' size='chars' > <input type='submit' value='Submit'></form>"
  "</center>";

ESP8266WebServer server(80);                             

void setup(void) {  
  Wire.begin(0,2);                               

  display.begin(SSD1306_SWITCHCAPVCC, OLED_address); 
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   
  WiFi.softAP(SSID, PASS);

  server.on("/", []() {
    server.send(200, "text/html", htmlpage);
  });
  server.on("/msg", text);                  
  server.on("/bmp", HTTP_POST, bmp);
  server.begin();                               

  displayInfo();

}

void loop(void) {
  server.handleClient();                       
}

void displayInfo(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Wifi network name:   "); display.print(SSID);
  display.println();
  display.println();
  display.println("    Connect to me");
  display.print("and visit: "); display.print(IPparts[0]);display.print(".");display.print(IPparts[1]);display.print(".");display.print(IPparts[2]);display.print(".");display.print(IPparts[3]);
  display.println();
  display.println();
  display.print("        WHOO!");
  display.display();
}

void text(){
  server.send(200, "text/html", htmlpage);    // Send same page so they can send another msg

  String msg = server.arg("msg");
  int size = server.arg("size").toInt();
  display.setTextSize(size);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.clearDisplay();
  unRotate();

  int startInt= 0;

  for(int i = 0;i<=msg.length();i++){
    if(msg.charAt(i)==0x5C && i<msg.length() && msg.charAt(i+1)==0x6E){ // check for \n
      display.println(msg.substring(startInt, i));
      startInt=i+2; //2 because of the n
    }
    if(i==msg.length()&&startInt<msg.length()){
      display.println(msg.substring(startInt, i));
    }
  }

  display.stopscroll();
  display.display();
  delay(1);

  if(server.hasArg("scroll")){
    display.startscrollright(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);
    display.startscrollleft(0x00, 0x0F);
    delay(2000);
    display.stopscroll();
    delay(1000);    
    display.startscrolldiagright(0x00, 0x07);
    delay(2000);
    display.startscrolldiagleft(0x00, 0x07);
    delay(2000);
    display.stopscroll();
    display.startscrollright(0x00, 0x0F);
  } 
}

//Watch out! This function is only correct for sure with with windows paint-made monochrome bitmaps
void bmp(){
  HTTPUpload& upload = server.upload();

  int startX=0;
  int startY=0;

  long width = 0;
  width |= upload.buf[0x12];  
  width |= (long)upload.buf[0x13] << 8;
  width |= (long)upload.buf[0x14] << 16;
  width |= (long)upload.buf[0x15] << 24;
  long height = 0;
  height |= upload.buf[0x16];
  height |= (long)upload.buf[0x17] << 8;
  height |= (long)upload.buf[0x18] << 16;
  height |= (long)upload.buf[0x19] << 24;

  long startAddressPixelArray = 0;
  startAddressPixelArray |= abs(upload.buf[0x0A]);  
  startAddressPixelArray |= ((long)abs(upload.buf[0x0B])) << 8;
  startAddressPixelArray |= ((long)abs(upload.buf[0x0C])) << 16;
  startAddressPixelArray |= (long)abs((upload.buf[0x0D])) << 24;

  if(width>128||height>64 || width<2||height<2){
     server.send(500, "text/plain", "image to big or small for now :(");
    return;
  }
  
  startX =(128-width)/2; 
  if(startX<0){
    startX=0;
  }
  
  startY =(64-height)/2; 
  if(startY<0){
    startY=0;
  }
  
  uint8_t copy[2048];

  for(long i = startAddressPixelArray;i<2048;i++){
    copy[i-startAddressPixelArray]=upload.buf[i];
  }

  display.stopscroll();
  display.clearDisplay();
  rotate();
  
  int16_t i, j, byteWidth = (width + 7) / 8;
  uint8_t byte = 0;

  for(j=1; j<height; j++) { //dunno if its paint artifact or what ever but hmmmm..... was a whit line on bottom and side of bitmaps made with paint so set to 1 instead of zero.
   for(i=width; i>=1; i-- ) {
     if(i & 7) byte <<= 1;
     else      byte   = copy[j * byteWidth - i / 8];
     if(byte & 0x80) display.writePixel(startX+i, startY+j, WHITE);
    }
  }
  
  display.display();

  server.send(200, "text/html", htmlpage);    // Send same page so they can send another msg
}

void unRotate(){
  if(rotated){
    display.setRotation(0);
    rotated=false;
  }
}

void rotate(){
  if(!rotated){
    display.setRotation(2);
    rotated=true;
  }
}


