#include <SPI.h>
#include <Ethernet.h>
#include <TextFinder.h>
#include <EEPROM.h>
#include <LiquidCrystal595.h>  
/*
  Exernal IP Address Tracker Rev B
  https://hackaday.io/project/2495-external-ip-address-tracker
  -Connects to checkip.dns.org, obtains external IP address.
  -Gets local network IP via dhcp.
  -Emails external IP address via SMTP
  -Displays external IP address on LCD connected to arduino via 74HC595
  Rev B Adds:
  -Configuration parameters stored in EEPROM 
  -Configuration parameters are modified through serial port. 
  
  Uses:
    Textfinder.h http://playground.arduino.cc/Code/TextFinder
    LiquidCrystal595.h https://code.google.com/p/arduino-lcd-3pin/

 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * LCD attached to pins 2, 3, 4 via 74HC595
 
 Setup:
 Connect via serial port to modify configuration paramets. Send "s" after reset.
 Your SMTP server must support AUTH LOGIN (use EHLO command to confirm)
 The SMTP server username and password must be base64 encoded 
 The sendmail() function works with Comcast's SMTP email server. 
 The sendmail() function may need to be adjusted to work with your email server. */
 
//*************** Start of Configuration Parameters ****************
// In Rev B these parameters are over written with data read from EEPROM

String smtp = "smtp.comcast.net"; //SMTP server address
char smtp_arr[] = "smtp.comcast.net"; 
String smtp_port_str = "587"; //smtp server port
int smtp_port = 587;

String to_email_addr = "EMAIL@gmail.com"; // destination email address
String from_email_addr = "USER@COMCAST.NET"; //source email address

//base64 encoded username and password
//Use this site to encode: http://webnet77.com/cgi-bin/helpers/base-64.pl/

String emaillogin = "dXNlcm5hbWU="; //username
String password = "cGFzc3dvcmQ="; //password

//frequency to check ext IP 
String delayvalue_str = "7200000"; // 2 hours 
int delayvalue = 30000;

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

unsigned long begin_time = 0;

byte smtp_length;
byte smtp_port_length;  
byte to_email_addr_length;
byte from_email_addr_length;
byte emaillogin_length;
byte password_length;
byte delayvalue_length;

byte smtp_eaddr = 7;
byte smtp_port_eaddr;
byte to_email_addr_eaddr;
byte from_email_addr_eaddr;
byte emaillogin_eaddr;
byte password_eaddr;
byte delayvalue_eaddr;


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
  
  //Obtain config params from EEPROM
  readsettings_eeprom();
  printcurrentsettings();

  //Prompt to modify params
  Serial.println("Press S for Setup");
  begin_time = millis();
  while(millis()-begin_time < 20000)
  { 
    if (Serial.available())
    {
     char i = Serial.read();
      if(i == 'S' || i == 's')
        configuration();
    }//end if
  }// end while 
    
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
 Serial.println("Setup Complete");  
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
  if (client.connect(smtp_arr, smtp_port)) 
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

void printcurrentsettings()
{  
  Serial.print("SMTP Server: ");
  Serial.println(smtp);
  Serial.print("SMTP Port: ");
  Serial.println(smtp_port_str);
  Serial.print("To Email Address: ");
  Serial.println(to_email_addr);
  Serial.print("From Email Address: ");
  Serial.println(from_email_addr);
  Serial.print("Encoded login: ");
  Serial.println(emaillogin);
  Serial.print("Encoded password: ");
  Serial.println(password);
  Serial.print("External address refresh rate: ");
  Serial.print(delayvalue_str);
  Serial.println(" milliSeconds");
  Serial.println();
}// end printcurrentsettings

void configuration()
{
  boolean timeout = 1;
  char write_eeprom = 'n';
  
  Serial.println();
  Serial.println("Configuration");
  Serial.println();
  
  Serial.println("Enter SMTP Server: ");
  getStringFromSerial(smtp);

  Serial.println("Enter SMTP port: ");
  getStringFromSerial(smtp_port_str);
    
 Serial.println("Enter to email address");
 getStringFromSerial(to_email_addr);
   
 Serial.println("Enter from email address");
 getStringFromSerial(from_email_addr);
      
  Serial.println("Enter Encoded login");
  getStringFromSerial(emaillogin);
      
  Serial.println("Enter Encoded password");
  getStringFromSerial(password);

  Serial.println("Enter IP refresh rate in seconds");
  getStringFromSerial(delayvalue_str);
 /* begin_time = millis();
  timeout = 1;
  while( millis()-begin_time < 30000)
  { 
    if (Serial.available())
     { delayvalue = Serial.parseInt() * 1000;
       timeout = 0;
       break;
     } //end if
  } //end while
  if (timeout == 1)
    Serial.println("Timeout");*/
    
 Serial.println(); 
 Serial.println("Current settings in memory:"); 
 printcurrentsettings();
 Serial.println("Write to EEPROM?");
 Serial.println("Y or N");
 begin_time = millis();
 timeout = 1;
 while( millis()-begin_time < 30000)
 { 
    if (Serial.available())
     { write_eeprom = Serial.read();
       timeout = 0;
       break;
     } //end if
  } //end while
  if (timeout == 1)
  { write_eeprom = 'n';
   Serial.println("Timeout");}

 if (write_eeprom == 'y' || write_eeprom == 'Y')
    write_to_eeprom(); 
   
 readsettings_eeprom();
 printcurrentsettings();
}// end configuration()

void getStringFromSerial(String &stringvar)
{
  begin_time = millis();
  boolean inputTimeout = 1;
  while(millis()-begin_time < 30000)
  { 
    if (Serial.available())
     { stringvar = Serial.readString();
       inputTimeout = 0;
       break;
     } //end if
  } //end while
  if (inputTimeout == 1)
    Serial.println("Timeout");
  
 // return inputTimeout;
}// end getStringFromSerial


void readsettings_eeprom()
{ //read settings from eeprom
   /*
  MEM MAP:
  Byte = Length of Var
  0 = smtp
  1 = smtp port (int always = 2)
  2 = to_email_addr
  3 = from_email_addr
  4 = emaillogin
  5 = password
  6 = delayvalue (int always = 2)
  7 = first byte of data
  */
  Serial.println("Reading from EEPROM");
  
  smtp_length = (byte)EEPROM.read(0);
  smtp_port_length = (byte)EEPROM.read(1);
  to_email_addr_length = (byte)EEPROM.read(2);
  from_email_addr_length = (byte)EEPROM.read(3);
  emaillogin_length = (byte)EEPROM.read(4);
  password_length = (byte)EEPROM.read(5);
  delayvalue_length = (byte)EEPROM.read(6);
  
  calculate_address();
 /* Serial.println();
  Serial.println("eaddr :");
  Serial.println(smtp_eaddr);
  Serial.println(smtp_port_eaddr);
  Serial.println(to_email_addr_eaddr);
  Serial.println(from_email_addr_eaddr);
  Serial.println(emaillogin_eaddr);
  Serial.println(password_eaddr);
  Serial.println(delayvalue_eaddr);
  Serial.println();*/
  
  //read bytes out of eeprom
  eeprom_to_string(smtp_eaddr, smtp_length, smtp); 
  eeprom_to_string(smtp_port_eaddr, smtp_port_length, smtp_port_str);
  smtp_port = smtp_port_str.toInt(); //convert string to int
  eeprom_to_string(to_email_addr_eaddr, to_email_addr_length, to_email_addr);
  eeprom_to_string(from_email_addr_eaddr, from_email_addr_length, from_email_addr); 
  eeprom_to_string(emaillogin_eaddr, emaillogin_length, emaillogin);
  eeprom_to_string(password_eaddr, password_length, password);
  eeprom_to_string(delayvalue_eaddr, delayvalue_length, delayvalue_str);
  delayvalue = delayvalue_str.toInt();
  Serial.println("Read from EEPROM Complete");
} //end readsettings_eeprom

void calculate_address()
{
  smtp_eaddr = 7;
  smtp_port_eaddr = smtp_eaddr + smtp_length;
  to_email_addr_eaddr = smtp_port_eaddr + smtp_port_length;
  from_email_addr_eaddr = to_email_addr_eaddr + to_email_addr_length;
  emaillogin_eaddr = from_email_addr_eaddr + from_email_addr_length;
  password_eaddr = emaillogin_eaddr + emaillogin_length;
  delayvalue_eaddr = password_eaddr + password_length;
  
  Serial.println("address: ");
  Serial.println(smtp_eaddr);
  Serial.println(smtp_port_eaddr);
  Serial.println(to_email_addr_eaddr);
  Serial.println(from_email_addr_eaddr);
  Serial.println(emaillogin_eaddr);
  Serial.println(password_eaddr); 
  Serial.println(delayvalue_eaddr);
} 
 
void eeprom_to_string(byte address, byte length, String &estring)
{
 // Serial.println("eeprom to string...");
  estring = "";
  for(byte i = address; i < address + length; i++)
   {char x = EEPROM.read(i);
    estring += x;
   }// end for
/*   Serial.println();
   Serial.print("ESTRING :");
   Serial.println(estring);*/
}// end eeprom_to_string

/****************************************************************************/
void write_to_eeprom()
{
  Serial.println("writing to eeprom");
  // find length of strings
  smtp_length = smtp.length();
  smtp_port_length = smtp_port_str.length();
  to_email_addr_length = to_email_addr.length();
  from_email_addr_length = from_email_addr.length();
  emaillogin_length = emaillogin.length();
  password_length = password.length();
  delayvalue_length = delayvalue_str.length();
  
  
/*  Serial.println("length: ");
  Serial.println(smtp_length);
  Serial.println(to_email_addr_length);
  Serial.println(from_email_addr_length);
  Serial.println(emaillogin_length);
  Serial.println(password_length);  */
  
  EEPROM.write(0, smtp_length);
  delay(100);
  EEPROM.write(1, smtp_port_length); //smtp port
  delay(100);
  EEPROM.write(2, to_email_addr_length);
  delay(100);
  EEPROM.write(3, from_email_addr_length);
  delay(100);
  EEPROM.write(4, emaillogin_length);
  delay(100);
  EEPROM.write(5, password_length);
  delay(100);
  EEPROM.write(6, delayvalue_length);
  delay(100);
  
  calculate_address();
  
  string_to_eeprom(smtp_eaddr, smtp_length, smtp); 
  string_to_eeprom(smtp_port_eaddr, smtp_port_length, smtp_port_str);
  string_to_eeprom(to_email_addr_eaddr, to_email_addr_length, to_email_addr);
  string_to_eeprom(from_email_addr_eaddr, from_email_addr_length, from_email_addr); 
  string_to_eeprom(emaillogin_eaddr, emaillogin_length, emaillogin);
  string_to_eeprom(password_eaddr, password_length, password);
  string_to_eeprom(delayvalue_eaddr, delayvalue_length, delayvalue_str);  
} //end write_to_eeprom

/****************************************************************************/

void string_to_eeprom(byte address, byte length,String &mstring)
{
 // int lengthi = (int)length;
 // int addressi = (int)address;
  char chararray[length];
  mstring.toCharArray(chararray, length + 1);
  
 /* Serial.print("char array :");
  for(byte i = 0; i <= length; i++)
  {
   Serial.print(chararray[i]);
   }
  Serial.println();*/
  
  for(byte i = 0; i <= length; i++)
  {
    EEPROM.write(address + i, chararray[i]);
    delay(100);
  }
}// end string_to_eeprom

