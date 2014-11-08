#include "Adafruit_NeoPixel.h"
#include <SPI.h>
#include <boards.h>
#include <RBL_nRF8001.h>
#include <AES.h>

AES aes ;
byte key[] = 
{
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d ,0x0e ,0x0f,
} ;
byte plain[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
} ;
byte enc_txt[16];
char password[] = "ALEX";

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream
//   NEO_GRB     Pixels are wired for GRB bitstream
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA pixels)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
const int numPixel = 30;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(30, 8, NEO_GRB + NEO_KHZ800);

int R = 0;
int G = 0;
int B = 0;

int count = 0;
int cleancount = 0;
int colorcount = 0;
int buf[((numPixel*6)+17*2)]; 
int color[3];
int ledcount = 0;
int brightness = 0;
int program = 0;

void setup() 
{
  Serial.begin(9600);
  strip.begin();
  strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.setPixelColor(0, strip.Color(255, 0, 0));
  strip.show(); // Initialize all pixels to 'off'
  ble_set_name( "IKEA RGB" );
  ble_begin();
}

/* escape sign 0xFF
   EOF sign 0x00
  */
boolean colorwipe_sw = false;
void loop() 
{
  if(ble_available()){
    buf[count] = ble_read();
    if( count > 0 ){
      //EOF found
      if( buf[count] == 0 && buf[count-1] != 0xFF ){
       
       //Serial.println("EOF");
       processLamps();
       cleancount = 0;
       colorcount = 0;
       ledcount = 0;
       count = 0;
     }else{
       count++;
       if( count >= ((numPixel*6)+17*2) ){
         count = 0;
         ledcount = 0;
         colorcount = 0;
         cleancount = 0;
       }
     }
    }else{
      count++;
    }
  }
 
  ble_do_events();
}

void processLamps(){
     for( int i = 0; i < count; i++ ){
         if( buf[i] == 0xFF ){ 
           i++;
           if( buf[i] == 0x55 ){
             buf[i] = 0xFF;
           }
         }
         
         if( cleancount == 0 ){ //  Programm
           program = buf[i];
         }else if( cleancount == 1 ){  // Brightness
           brightness = buf[i]; 
         }else if( cleancount < 18 ){ // AES Password
           enc_txt[ cleancount-2 ] = buf[i];
         }else if( cleancount >= 18){ // Lamps
           if( cleancount == 18 ){
              byte succ = aes.set_key(key, 128);
              succ = aes.decrypt(enc_txt, plain);
             
              // Compare the decrypt string with the defined one. If not then reset
              if( strncmp( (char*)plain, password, 4 ) != 0 ) return;
              
              /* 0 lamps off
                 1 rainbow cycle
                 2 wipecolor
                 3-... brightness
              */
              if( program == 0 ){
                clear_lamps();
                return;
              }else if( program == 1 ){
                rainbowCycle( 20 );
                return;
              }else if( program == 2 ){
                colorwipe_sw = true;
              }
           }
           
           color[colorcount] = buf[i];
           if( colorcount == 2 ){
             colorcount = 0;
             if( colorwipe_sw ){
               colorWipe( strip.Color(color[0], color[1], color[2]), 500 );
               colorwipe_sw = false;
               return;
             }else{
               for( int n = 0; n < 3; n++ ){
                 strip.setPixelColor(ledcount, strip.Color(color[0], color[1], color[2]) );
                 /*
                 Serial.print("LED ");
                 Serial.print(ledcount);
                 Serial.print( " " );
                 Serial.print(color[0]);
                 Serial.print( " " );
                 Serial.print(color[1]);
                 Serial.print( " " );
                 Serial.println(color[2]);
                 */
                 ledcount++;
               }
             }
           }else{
             colorcount++;
           }      
         }
         cleancount++;
    }
       strip.setBrightness( brightness );
       strip.show();
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(uint8_t wait) {
  uint16_t i, j, n;
  uint32_t c = 0;
  
  for(j=0; j<256; j++) { // 5 cycles of all colors on wheel
  
    for(i=0, n=0; i< strip.numPixels()/3; i+=3, n++) {
      c = Wheel(((n * 256 / (strip.numPixels()/3)) + j) & 255);
      strip.setPixelColor(i, c);
      strip.setPixelColor(i+1, c);
      strip.setPixelColor(i+2, c);
    }
    strip.setBrightness( brightness );
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint16_t wait) 
{
  for(uint16_t i=0; i<strip.numPixels()/3; i+=3) 
  {
      strip.setPixelColor(i, c);
      strip.setPixelColor(i+1, c);
      strip.setPixelColor(i+2, c);
      strip.setBrightness( brightness );
      strip.show();
      delay(wait);
  }
}

void clear_lamps(){
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
      strip.setPixelColor(i, 0);  
  }
  strip.setBrightness( brightness );
  strip.show();
}
