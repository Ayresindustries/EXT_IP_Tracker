EXT_IP_Tracker
==============

Arduino External IP tracker with email notification

Arduino with Ethernet shield is used to periodically obtain external IP address from checkip.dyndns.org. 
If the IP address is different than the last query an email is sent via SMTP indicating the new IP address. 
The IP address is also displayed on a LCD using a 74HC595 to interface with the Arduino. 

This allow you to remotely monitor the External IP address of an internet connection without needing to use DDNS service which 
would require a PC running a DDNS client. 

For more info check out http://hackaday.io/project/2495-external-ip-address-tracker

Setup information is in the Arduino sketch. 
