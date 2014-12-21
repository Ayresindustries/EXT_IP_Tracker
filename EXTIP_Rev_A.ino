#include <SPI.h>
#include <Ethernet.h>
#include <TextFinder.h>
#include <LiquidCrystal595.h>  
/*
  Exernal IP Address Tracker
  https://hackaday.io/project/2495-external-ip-address-tracker
  -Connects to checkip.dns.org, obtains external IP address.
  -Gets local network IP via dhcp.
  -Emails external IP address via SMTP
  -Displays external IP address on LCD connected to arduino via 74HC595
  
  Uses:
    Textfinder.h http://playground.arduino.cc/Code/TextFinder
    LiquidCrystal595.h https://code.google.com/p/arduino-lcd-3pin/

 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * LCD attached to pins 2, 3, 4 via 74HC595
 
 Setup:
 Edit the configuration parameters below. 
 Your SMTP server must support AUTH LOGIN (use EHLO command to confirm)
 The SMTP server username and password must be base64 encoded 
 The sendmail function works with Comcast's SMTP email server. 
 The sendmail() function may need to be adjusted to work with your email server. */
 
//*************** Start of Configuration Parameters ****************
char smtp[] = "smtp.comcast.net"; //SMTP server address
int smtp_port = 587; //smtp server port

String to_email_addr = "EMAIL@gmail.com"; // destination email address
String from_email_addr = "USER@COMCAST.NET"; //source email address

//base64 encoded username and password
//Use this site to encode: http://webnet77.com/cgi-bin/helpers/base-64.pl/

String emaillogin = "dXNlcm5hbWU="; //username

String password = "cGFzc3dvcmQ="; //password

//frequency to check ext IP 
int delayvalue = 7200000; // 2 hours 


//******SET LCD PINS HERE******
LiquidCrystal595 lcd(2,3,4);     // datapin, latchpin, clockpin


// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {  0x90, 0xA2, 0xDA, 0x00, 0x00, 0x00 };

//************** End of Configuration Parameters *************************

char serverName[] = "checkip.dyndns.org";
char externalIP[17];
char lastExternalIP[17];
// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;
TextFinder finder( client);

char c = ' ';
//char i = 1; // counter 
char matchfail = 0; // match failure = true
long starttime = 0;



void setup() 
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("Setup");
  
  lcd.begin(16,2);             // 16 characters, 2 rows
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Startup...");
    
  // start the Ethernet connection:
  while (Ethernet.begin(mac) == 0) 
  { Serial.println("Failed to configure Ethernet using DHCP");
    lcd.setCursor(0,1);
    lcd.print("Init ERROR!!");
    delay(10000);
  }//end while
 
  
    Serial.print("DHCP IP: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++) 
    {   // print the value of each byte of the IP address:
      Serial.print(Ethernet.localIP()[thisByte], DEC);
      Serial.print("."); 
    }//end for
    Serial.println();
    show_dhcp_lcd(); // display on LCD     

 
  // give the Ethernet shield a second to initialize:
  delay(1000); 
} //endsetup

void loop()
{
 Serial.print("connecting... ");
 Serial.println(serverName);

 // if you get a connection, report back via serial:
 if (client.connect(serverName, 80)) 
 { Serial.println("connected");
   //make HTTP request
   client.println("GET /");
   client.println();
   delay(1000);
 }//end if 
 else 
 { // you didn't get a connection to the server:
   Serial.println("connection failed");
 }//end else

/* while (client.available()) {
    char c = client.read();
    Serial.print(c); */
 if (client.available()) 
 { if(finder.find("IP Address: "))
   { for (char k = 0; k < 17; k++)
     { c = client.read();
       if(c != '<')
       { Serial.print(c);
         externalIP[k] = c;
       } //endif
       else 
        break;
      }//end for
   }//end if
 }//end if
 client.flush(); 
 client.stop();
 
 Serial.println();
 for (char k = 0; k < 17; k++)
 { if (lastExternalIP[k] != externalIP[k]) 
   { matchfail = 1;
     break;
   }//end if
   else 
    matchfail = 0;
   
 } //end for
 
 if (matchfail == 1) 
 { Serial.println("DOES NOT MATCH"); 
   sendemail();
 }
 else 
 { Serial.println("MATHCHES"); 
 }
 Serial.println(); 
 show_extip_lcd(); // display ext IP on LCD

 for(char j = 0; j < 17; j++) 
 { //Serial.print(externalIP[j]);    
   lastExternalIP[j] = externalIP[j]; //assign current extIP to last extIP
 }// end for
 
 Serial.println();
 Serial.println("disconnecting.");
 client.stop();
   

 delay(delayvalue);
} //end loop

void waitforresponse()
{
  starttime = millis();
  while (!client.available()) 
    { if ((millis() - starttime) > 100000)
       {
         Serial.println("Timeout");
         break;
       }
    }    
 
    while (client.available())
    { 
      char c = client.read();
      Serial.print(c);
    }//end while
     Serial.println();
    
}

void show_dhcp_lcd()
{
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("DHCP IP: ");
   lcd.setCursor(0,1);
   for (byte thisByte = 0; thisByte < 4; thisByte++) 
   {   // print the value of each byte of the IP address:
    lcd.print(Ethernet.localIP()[thisByte], DEC);
    if(thisByte < 3)
      lcd.print("."); 
   }//end for
}// end show_dhcp_lcd

void show_extip_lcd()
{
   lcd.clear();
   lcd.setCursor(0,0);
   lcd.print("External IP: ");
   lcd.setCursor(0,1);
   for(char j = 0; j < 17; j++) 
   {
     if(externalIP[j] < 39)
       lcd.print(" ");
     else
       lcd.print(externalIP[j]); 
   }//endfor 
}//end show_extip_lcd

void sendemail() 
{
  //connect to SMTP email server.  
  Serial.print("connecting SMTP... ");
  
  // if you get a connection, report back via serial:
  if (client.connect(smtp, smtp_port)) 
  { Serial.println("connected");
    waitforresponse();
    
    client.print("HELO "); // say hello
    client.println(from_email_addr);
    waitforresponse();
    
    client.println("AUTH LOGIN"); // Authentication
    waitforresponse();
 
    client.println(emaillogin); // user name encoded
    waitforresponse();
    
    client.println(password); // password encoded
    waitforresponse();
    
    
     client.print("MAIL FROM:<"); // identify sender 
     client.print(from_email_addr);
     client.println(">");
     waitforresponse();    
 
     client.print("RCPT TO: <"); // identify recipient 
     client.print(to_email_addr);
     client.println(">");
     waitforresponse();
    
    client.println("DATA"); 
    waitforresponse();
    
    // start of email 
    client.print("To: ");
    client.println(to_email_addr);
    client.print("From: ");
    client.println(from_email_addr);
    client.println("Subject: External IP Address");

    client.print("YOUR IP ADDRESS IS "); 
    for(char j = 0; j < 17; j++) 
    { client.print(externalIP[j]); 
    }//endfor
    client.println();  
    client.println("."); // end of email
    waitforresponse();
    
    client.println("QUIT"); // terminate connection 
    Serial.println("Email Complete"); 
  }//endif connect    
   
  if (!client.connected()) 
  { Serial.println();
    Serial.println("Disconnected!!!!!!"); 
  }//endif 

} //end sendemail
