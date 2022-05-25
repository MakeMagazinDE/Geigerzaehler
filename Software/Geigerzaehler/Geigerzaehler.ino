/*************************************************************************
 * Programm: geigerzaehler.ino                                                *
 * Author:   Heinz Behling *                                          *
 * Datum:    8.5.2022                                                *
 *************************************************************************/
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
//#include <PubSubClient.h>
#include <ESP_Mail_Client.h>

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x3f, 20, 4); //Adresse, Zeichen pro Zeile, Zeilenzahl

#define MESSZEIT  15000 //in 1/1000 Sekunden
#define COUNTER_PIN D7
#define zaehlrohrfaktor 0.0067   // Umrechnungsfaktor für M4011-Zählrohre

//EMail-Abteilung
#define SMTP_server "IHR_SMTP_SERVER"
#define SMTP_Port PORTADRESSE
#define sender_email "IHRE_EMAIL_ADRESSE"
#define sender_password "SMTP_PASSWORT"
#define Recipient_email "EMPFÄNGERADRESSE"
SMTPSession smtp;

//WLAN-Abteilung
const char* ssid = "SSID";
const char* password = "WLAN_PASSWORT";

bool notsend = true;
int mailcount = 0;

unsigned long impulse; 
unsigned long cpm;
unsigned long impulseonline; 
unsigned long cpmonline;
unsigned int  mult;   // CPM = (counts in a given interval) * multiplier
unsigned long alarm = 100;
String newHostname = "Geigerzaehler";

// Interrupt-Routine wenn Impuls eintrifft
ICACHE_RAM_ATTR void tube_impulse(){  //ICACHE_RAM_ATRR schreibt Routine ins RAM
   impulse++;
   Serial.print("+");
}
void setup()
{
   Serial.begin(115200);
   WiFi.mode(WIFI_STA);
   WiFi.hostname(newHostname.c_str()); 
   delay(1000);  
   lcd.init();
   lcd.backlight();
   lcd.setCursor(0, 0);
   lcd.print("Make: Geigerzaehler");
   Serial.println("=========================================");
   Serial.println("Make: Geigerzaehler");
   Serial.println("=========================================");
   Serial.println();
   Serial.print("Verbinde mit ");
   Serial.println(ssid);
   lcd.setCursor(0, 1);
   lcd.print("verbinde mit ");
   lcd.setCursor(0, 2);
   lcd.print(ssid);
   
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
     delay(50);
     Serial.print(".");
   }
   Serial.println("");
   Serial.println("verbunden");
   lcd.setCursor(0, 3);
   lcd.print("verbunden");
   server.begin();

   lcd.init();
   lcd.setCursor(0, 0);
   lcd.print("Make: Geigerzaehler");
   //lcd.backlight();
   impulse = 0;
   cpm = 0;
   mult = 60000/MESSZEIT;
   pinMode(COUNTER_PIN, INPUT);                         
      
   
   Serial.print("IP-Adresse: ");
   Serial.println(WiFi.localIP());
   lcd.setCursor(0, 1);
   lcd.print("IP-Adresse:");
   lcd.setCursor(0,2);
   lcd.print(WiFi.localIP());
   Serial.print("Bitte warten Sie ");
   Serial.print(MESSZEIT/1000);
   Serial.println(" sec. bis zur ersten Messung");
   Serial.println();
   
   attachInterrupt(digitalPinToInterrupt(COUNTER_PIN), tube_impulse, FALLING);

}

void loop()
{
      static unsigned long startzeit;
   unsigned long jetzt = millis();
   double uSv;

      
   //Immer wenn MESSZEITvergangen ist, Werte ausgeben
   if (jetzt-startzeit > MESSZEIT) {
      startzeit = jetzt;
      if (impulse) {                    
         cpm = impulse * mult;
         uSv = cpm * zaehlrohrfaktor;
         impulseonline = impulse;
         cpmonline = cpm;
      } else {
        uSv = 0;
      }
      lcd.init();
      lcd.setCursor(0, 0);
      lcd.print("Make: Geigerzaehler");
      lcd.setCursor(0, 1);
      Serial.println("");
      Serial.print("Impulse: ");          
      lcd.print("Impulse: ");      
      Serial.println(impulse);
      lcd.print(impulse);
      lcd.setCursor(0, 2); 
      Serial.print("    CPM: ");      
      lcd.print("    CPM: ");      
      Serial.println(cpm);
      lcd.print(cpm);
      lcd.setCursor(0, 3); 
      Serial.print("  uSv/h: ");
      lcd.print("  uSv/h: ");
      Serial.println(uSv, 4);          // mit 4 Nachkommastellen
      lcd.print(uSv, 4);          
      if ( cpm > alarm ) {
        Serial.println("Alarm: Schwellwert überschritten");
        lcd.init();
        lcd.setCursor(0, 0);
        lcd.print("Make: Geigerzaehler");
        lcd.setCursor(0, 1);
        lcd.print("Impulse: ");      
        lcd.print(impulse);
        lcd.setCursor(0, 2); 
        lcd.print("   CPM: ");      
        lcd.print(cpm);
        lcd.setCursor(0, 3); 
        lcd.println("      Alarm!      ");
      }
      Serial.println("");      
      impulse = 0, cpm = 0;  
   }
   
   WiFiClient client = server.available();
   client.print("<head><title>Make:-Geigerz&auml;hler</title><meta http-equiv='refresh' content='5' /></head>");
   client.print("<h1>Make:-Geigerz&auml;hler </h1><br>");
   client.print("<table>");
   client.print("<tr><td><b>Impulse:</b> </td><td>"); client.print(impulseonline); client.print("<br></td></tr>");
   client.print("<tr><td><b>Impulse/min:</b> </td><td>"); client.print(impulseonline*mult); client.print("<br></td></tr>");
   client.print("<tr><td><b>uSv/h:</b> </td><td>"); client.print(uSv); client.print("<br></td></tr>");
   client.print("<tr><td><b>Schwellwert:</b> </td><td>"); client.print(alarm); client.print("<br></td></tr>");
   if ( cpmonline > alarm ) {
        client.print("<tr><td><b><big><big><big>ALARM!</big></big></big></b> </td><td>"); client.print("Schwellwert ueberschritten!"); client.print("<br></td></tr>");
      }
   client.print("</table>");

   delay(100); 

   if ( cpmonline > alarm ) {
      mailcount = mailcount - 1;
      Serial.println(mailcount);
      if (mailcount == 0){
        notsend = true;
      }
      if ( notsend == true ) {
         notsend = false;
         mailcount = 1200;
         smtp.debug(1);
         ESP_Mail_Session session;
         session.server.host_name = SMTP_server ;
         session.server.port = SMTP_Port;
         session.login.email = sender_email;
         session.login.password = sender_password;
         session.login.user_domain = "";
         SMTP_Message message;
         message.sender.name = "Geigerzaehler";
         message.sender.email = sender_email;         
         message.subject = "Strahlungsalarm";
         message.addRecipient("Heinz",Recipient_email);         
         //Send HTML message
         String htmlMsg = "<div style=\"color:#FF0000;\"><h1>ALARM!</h1><p>Der Strahlungsgrenzwert wurde überschritten!</p></div>";
         message.html.content = htmlMsg.c_str();
         message.html.content = htmlMsg.c_str();
         message.text.charSet = "us-ascii";
         message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
         if (!smtp.connect(&session))
         return;
         if (!MailClient.sendMail(&smtp, &message))
         Serial.println("Error sending Email, " + smtp.errorReason());
         Serial.println("Mail gesendet");        
        }
   } 
}
