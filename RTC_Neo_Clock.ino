/*
 * RTC Wall Clock with DS3231 RTC for Trinket Pro
 *
 * The date and time can be updated and fetched from the serial console.
 * Serial port settings are 115200, 8N1
 *
 */

#include <RTClib.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

/****** Pinout ******
 *  A4 - SDA        *
 *  A5 - SCL        *
 *  D8 - LED_DAT    *
 *  RX - Serial RX  *
 *  TX - Serial TX  *
 ********************/

#define NEO_PIN     8
#define NUMPIXELS   60
#define BRIGHTNESS  64
#define STARTPIXEL  0  // change if pixel 0 of ring can't be aligned with 12 o'clock
#define OBSERVE_DST 1

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);

RTC_DS3231 rtc;
DateTime the_time;

uint8_t ser_hh = 0, ser_mm = 0, ser_ss = 0;
uint8_t ser_mo = 1, ser_day = 1;
uint16_t ser_yr = 18;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
uint8_t days_in_mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
uint8_t days_in_mo_lp[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

boolean is_dst;
byte clk_adj_flg = 0;

int char_num = 0;
char in_char;
boolean string_complete = false;
boolean prompt_flg = true;
char str[20];
byte temp1, temp2;

void setup() {
    // I2C Init
    Wire.begin();

    // UART Init
    Serial.begin(115200);
    delay(3000);

    Serial.println();

    // RTC Init
    rtc.begin();
    
    // NeoPixel Init
    strip.begin(); // This initializes the NeoPixel library.
    strip.show(); // Init all pixels off
    strip.setBrightness(BRIGHTNESS);

    while (!rtc.begin()){
            led_flash(strip.Color(255, 255, 255), 100);
            Serial.println("Couldn't find RTC");
    }

    if (rtc.lostPower()) {
        // Adjust to January 1, 2018, 12am
        Serial.println("RTC lost power!");
        rtc.adjust(DateTime(2018, 1, 1, 0, 0, 0));
        clk_adj_flg = 1;
    }
    
    // startup LED sequence
    colorWipe(strip.Color(255, 0, 0), 20);    //red
    colorWipe(strip.Color(0, 255, 0), 20);    //green
    colorWipe(strip.Color(0, 0, 255), 20);    //blue
}

void loop() {
    DateTime the_time = rtc.now();

    if (OBSERVE_DST) {
        if (check_dst(the_time)) {
            is_dst = 1;
            the_time = the_time.unixtime() + 3600;    
        }
        else {
            is_dst = 0;
        }
    }

    if (prompt_flg == true){
        prompt();
        prompt_flg = false;
    }

    //set LED ring
    pix_clock(the_time.hour(), the_time.minute(), the_time.second(), clk_adj_flg);

    serial_event();

    if (string_complete) {
        // 't' = update time.  Format is t<hhmmss>
        if (str[0] == 't') {
            if (strlen(str) == 7) {
                //read hour
                temp1 = (byte)str[1] - '0';
                temp2 = (byte)str[2] - '0';
                ser_hh = temp1 * 10 + temp2;
                //read min
                temp1 = (byte)str[3] - '0';
                temp2 = (byte)str[4] - '0';
                ser_mm = temp1 * 10 + temp2;
                //read sec
                temp1 = (byte)str[5] - '0';
                temp2 = (byte)str[6] - '0';
                ser_ss = temp1 * 10 + temp2;

                ser_yr = the_time.year();
                ser_mo = the_time.month();
                ser_day = the_time.day();

                if ((ser_hh > 23) || (ser_hh < 0) || (ser_mm > 59) || (ser_mm < 0) || (ser_ss > 60) || (ser_ss < 0)) {
                    Serial.println("Time not updated!  Invalid format!");
                }
                else {
                    rtc.adjust(DateTime(ser_yr, ser_mo, ser_day, ser_hh, ser_mm, ser_ss));
                    clk_adj_flg = 0;
                }
            }
            else {
                Serial.println("Time not updated! Invalid time format!"); 
                Serial.println("Expected format of hhmmss"); 
            }
        }
        // 'd' = update date.  Format is d<YYMMDD>
        else if (str[0] == 'd') {
            if (strlen(str) == 7) {
                //read year
                temp1 = (byte)str[1] - '0';
                temp2 = (byte)str[2] - '0';
                ser_yr = temp1 * 10 + temp2;
                //read month
                temp1 = (byte)str[3] - '0';
                temp2 = (byte)str[4] - '0';
                ser_mo = temp1 * 10 + temp2;
                //read day
                temp1 = (byte)str[5] - '0';
                temp2 = (byte)str[6] - '0';
                ser_day = temp1 * 10 + temp2;

                ser_hh = the_time.hour();
                ser_mm = the_time.minute();
                ser_ss = the_time.second();

                if ((ser_yr < 0) ||
                    (ser_mo > 12) ||
                    (ser_mo < 1) ||
                    (ser_day < 0) ||
                    (ser_day > ((ser_yr % 4 == 0) ? days_in_mo_lp[ser_mo - 1] : days_in_mo[ser_mo - 1]))
                   ) {
                    Serial.println("Date not updated!  Illogical date!");
                }
                else {
                    rtc.adjust(DateTime(2000 + ser_yr, ser_mo, ser_day, ser_hh, ser_mm, ser_ss));
                }
            }
            else {
                Serial.println("Date not updated!  Invalid date format!\n"); 
                Serial.println("Expected format of YYMMDD"); 
            }
        }
        // 'g' = get date/time
        else if (str[0] == 'g') {
            Serial.print(daysOfTheWeek[the_time.dayOfTheWeek()]);
            Serial.print(", ");
            Serial.print(the_time.month(), DEC);
            Serial.print('/');
            Serial.print(the_time.day(), DEC);
            Serial.print('/');
            Serial.print(the_time.year(), DEC);
            Serial.print(" ");
            Serial.print(the_time.hour(), DEC);
            Serial.print(':');
            Serial.print(the_time.minute(), DEC);
            Serial.print(':');
            Serial.print(the_time.second(), DEC);
            if (is_dst) {
                Serial.println(" PDT");
            }
            else {
                Serial.println(" PST");
            }
        }
        else if (str[0] == '\0') {
            Serial.println();
        }
        else if (str[0] == 'h') {
            Serial.println("Valid commands are as follows:");
            Serial.println(" t<hhmmss> - Update time to hh:mm:ss");
            Serial.println("    ex. t012345 - Update time to 01:23:45");
            Serial.println(" d<YYMMDD> - Update date to DD/MM/20YY");
            Serial.println("    ex. d171025 - Update date to 10/25/17");
            Serial.println(" g - Get current date & time");
            Serial.println(" h - This help menu");
            Serial.println("");
            Serial.println("** Note that the RTC does not store the time for daylight savings time.");
            Serial.println("** While this clock auto-adjusts for DST,");
            Serial.println("** the user must compensate for it when updating date & time.");
        }
        else {
            Serial.println("Unknown command!");  
        }
        string_complete = false;        
        prompt_flg = true;
        memset(str, 0, sizeof(str));
    }    // end brace of string_complete if-statement
}

void pix_clock(uint8_t h, uint8_t m, uint8_t s, boolean adj_flg) {
    uint8_t hr_pix, min_pix, sec_pix;
    uint8_t j, i;
    
    hr_pix = (h % 12) * 5;
    min_pix = m;
    sec_pix = s;

/*    Color Scheme    
    red - hour
    orange - marker
    yellow - hour/sec
    grn - second
    teal - min/sec
    blu - min
    purple - hour/min
    white - hour/min/sec
*/
    
    // set clock markers
    for (j = 0; j < strip.numPixels(); j++){
        i = j + STARTPIXEL;
        if ((i == hr_pix) || (i == hr_pix + 1) || (i == (hr_pix + 59) % 60)){
            if (i == min_pix){
                if (i == sec_pix){    //hr, min, sec at same pix (white)
                    strip.setPixelColor(i, strip.Color(255, 255, 255));
                }
                else{    //hr, min at same pix (purple)
                    strip.setPixelColor(i, strip.Color(255, 0, 255));
                }
            }
            else if (i == sec_pix){    //hr, sec at same pix (yellow)
                strip.setPixelColor(i, strip.Color(255, 255, 0));
            }
            else{ //hr pix (red)
                strip.setPixelColor(i, strip.Color(255, 0, 0));
            }
        }
        else if (i == min_pix){
            if (i == sec_pix) { //min, sec at same pix (teal)
                strip.setPixelColor(i, strip.Color(0, 255, 255));
            }
            else{ //min pix (blue)
                strip.setPixelColor(i, strip.Color(0, 0, 255));
            }
        }
        else if (i == sec_pix) { //sec_pix (green)
            strip.setPixelColor(i, strip.Color(0, 255, 0));
        }
        else if (i % 5 == 0) { //clock marker (orange, but nothing if adj_flg is set)
            if (adj_flg) {
                strip.setPixelColor(i, strip.Color(0, 0, 0));

            }
            else {
                strip.setPixelColor(i, strip.Color(255, 128, 0));
            }
        }
        else { //nothing
            strip.setPixelColor(i, strip.Color(0, 0, 0));
        }
    }
    strip.show();
}

void prompt() {
    Serial.print("CMD>");
}

void serial_event() {
    while (Serial.available()) {
        in_char = (char)Serial.read();
        Serial.write(in_char);
        if (in_char == '\b') {  //backspace
            char_num--;
        }
        else {
            str[char_num] = in_char;
            char_num++;
        }
        if ((in_char == '\n') || (in_char == '\r')) {
            str[char_num - 1] = '\0';
            string_complete = true;
            Serial.println();
            char_num = 0;
        }
    }
}

boolean check_dst(DateTime dst_dt) {
    uint16_t year;

    year = dst_dt.year();

    DateTime mar7 (year, 3, 7, 0, 0, 0);
    DateTime oct31 (year, 10, 31, 0, 0, 0);
    if ((dst_dt.month() > 3) && (dst_dt.month() < 11)) {
        return true;
    }
    else if (dst_dt.month() == 3) {
        if (dst_dt.day() > mar7.day() + (7 - mar7.dayOfTheWeek())) {
            return true;
        }
        else if (dst_dt.day() == mar7.day() + (7 - mar7.dayOfTheWeek())) {
            if (dst_dt.hour() >= 2) {
                return true;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    else if (dst_dt.month() == 11) {
        if (dst_dt.day() < 7 - oct31.dayOfTheWeek()) {
            return true;
        }
        else if (dst_dt.day() == 7 - oct31.dayOfTheWeek()) {
            if (dst_dt.hour() < (2 - 1)) { //fall back at 2am PDT, but RTC doesn't hold time of DST, so we change at RTC 1am
                return true;
            }
            else {
                return false;
            }
        }
        else {
            return false;
        }
    }
    else {
        return false;
    }
}

void led_flash(uint32_t c, uint8_t wait) {
    for(uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
    }
    strip.show();
    delay(wait);
    for(uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    strip.show();
    delay(wait);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    for(uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
        strip.show();
        delay(wait);
    }
}
