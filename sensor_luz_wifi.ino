
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUDP.h>
#define PIR_TIME 30000 //30 S
#define CONNECTION_TIMEOUT 120000 // 2 MINS

// variables para conexion UDP
WiFiUDP UDP;
unsigned int localPort = 7777;
boolean udpConnected = false;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char ReplyBuffer[] = "OK"; // a string to send back
IPAddress target_ip(192, 168, 1, 44);
unsigned int target_port = 8888;

boolean connectUDP();

//
const char* ssid     = "MOVISTAR_BECA";
const char* password = "uQxPYkqYxa6haRTbXGfX";
const char* host = "utcnist2.colorado.edu"; // de donde cojo la hora
const char* sunrise_api_request = "http://api.sunrise-sunset.org/json?lat=40.694909&lng=-4.027316";
String TimeDate = "", dia = "", mes = "", fecha = "", hora = "", mins = "";
int hora_actual = 200, dia_actual = 0, anyo = 0;
int timeout = 0, timeout_prev = 0;
int vera = 0;
int date_time_handler = 0;
bool noche = false;
const int meses[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const int anos_bisiestos[4] = {2016, 2020, 2024, 2028};
const int horario_verano[13][3] = {
  {2016, 87, 304}, // {año,comienzo horario, fin horario}
  {2017, 85, 302},
  {2018, 84, 301},
  {2019, 90, 300},
  {2020, 89, 299},
  {2021, 87, 304},
  {2022, 86, 303},
  {2023, 85, 302},
  {2024, 91, 301},
  {2025, 89, 299},
  {2026, 88, 298},
  {2027, 87, 304},
  {2028, 86, 303}
};
//                  {en,fb,mr,ab,my,jn,jl,ag,sp,oc,nv,dc};
const int RELAY = 14, PIR = 16;
int timeoutPIR = 0,timeoutPIR_prev = 0;
int pirValue = 0, pirValue_prev = 0;


int stringtoInt(String str);
int horaEnMinutos(String h, String m);
void diaDelAno(String d, String m);
void verano();
bool bisiesto();

void setup() {
  Serial.begin(115200);                   // diagnostic channel
  //Serial.begin(74880);
  delay(10);

  // We start by connecting to a WiFi network

  pinMode(PIR, INPUT);
  pinMode(RELAY, OUTPUT);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  delay(100);


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udpConnected = connectUDP();
  if (!udpConnected) {

  }
  timeout = millis();
  timeoutPIR = millis();
}

void loop()
{

  timeout_prev = millis();
  int diff_time = timeout_prev - timeout;
  if (timeout_prev - timeout > CONNECTION_TIMEOUT) // asi solo solocito la info cada 1 minuto
  {

    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 13;
    // obtencion de la hora

    Serial.print("connecting to ");
    Serial.println(host);

    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      date_time_handler = 0;
      return;
    }
    else{
    	Serial.println("connected utcnist2");
    	date_time_handler = 1;
    }



    // This will send the request to the server
	client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");
    delay(100);


    if (date_time_handler){    
      yield();
      String line = client.readStringUntil('\r');
      Serial.println("Mensaje completo del server:");
      Serial.println(line);
      fecha = line.substring(7, 15);
      mes   = fecha.substring(3, 5);
      dia   = fecha.substring(6, 8);
      hora  = line.substring(16, 18);
      mins  = line.substring(19, 21);
      anyo  = stringtoInt(fecha.substring(0, 2)) + 2000;
      diaDelAno(dia, mes);
      hora_actual = horaEnMinutos(hora, mins);
      verano();
      Serial.println("Fecha:" + dia + "/" + mes + "/" + anyo);
      Serial.println("Hora ->" + hora + ":" + mins + " UTC");
      if (vera == 1)
        Serial.println("horario de verano");
      else
        Serial.println("horario de invierno");
      Serial.printf("Dia del anyo: %d \n", dia_actual);
      Serial.printf("hora en minutos: %d \n", hora_actual);      
      date_time_handler = 0;
    } // fin solicitud Hora
    if ((WiFi.status() == WL_CONNECTED)) {
      yield();
      HTTPClient http;
      char buff[400];
      Serial.print("connecting to ");
      Serial.println("sunrise_sunset_api");
      http.begin(sunrise_api_request);// construida en el setup
      Serial.print("[HTTP] begin...\n");

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      int httpCode = http.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          payload.toCharArray(buff, 400);
          Serial.println("Respuesta de la API sunrise-sunset:");
          char str1[23] = "", str2[23] = "";

          char *sunrise = strstr(buff, "sunrise");
          char *sunset = strstr(buff, "sunset");
          strncpy(str1, sunrise, 17);
          strncpy(str2, sunset, 17);
          int h_sunrise = ( atoi(str1 + 10) + vera + 1) * 60 + atoi(str1 + 12); // + vera + por gmt+1/+2
          int h_sunset = ( atoi(str2 + 9) + vera + 1 + 12) * 60 + atoi(str2 + 11); // +12 por ser pm
          Serial.printf("Amanece a las : %d \ny anochece a las: %d \n", h_sunrise, h_sunset);
          if (hora_actual < h_sunset && hora_actual > h_sunrise)
          {
            noche = false;
            Serial.println("Es de dia");
          }

          else
          {
            noche = true;
            Serial.println("Es de noche");

          }
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();


    } // fin solicitud api sunrise-sunset

    timeout = timeout_prev;
    Serial.println();
    Serial.println("closing connection");
  }
  pirValue = digitalRead(PIR);

  if (pirValue && ( pirValue != pirValue_prev))
  {
   // timeoutPIR_prev = millis();
    Serial.print("Ha saltado el PIR : ");  

    if (noche)
    {
      digitalWrite(RELAY, HIGH);
      Serial.println(" NOCHE ON : LUZ ON");
    }
    else
    {
      digitalWrite(RELAY, LOW);
      Serial.println("NOCHE OFF : LUZ OFF");
    }
    
      Serial.println("Enviando Comando al timbre inalambrico");
      if(udpConnected)
      {
      delay(20);
      UDP.beginPacket(target_ip, target_port);
      UDP.write("pir");
      UDP.endPacket();        
      }
      else
      Serial.println("No se pudo conectar con el timbre");
  
  }
  else if (!(pirValue) && (pirValue != pirValue_prev))
  {
    digitalWrite(RELAY, LOW);
    Serial.println("PIR OFF");
  }



  delay(10);
  pirValue_prev = pirValue;
  if (udpConnected) {
    // if there’s data available, read a packet
    yield();
    int packetSize = UDP.parsePacket();
    if (packetSize)
    {
      Serial.println("");
      Serial.print("Received packet of size ");
      Serial.println(packetSize);
      Serial.print("From ");
      IPAddress remote = UDP.remoteIP();
      for (int i = 0; i < 4; i++)
      {
        Serial.print(remote[i], DEC);
        if (i < 3)
        {
          Serial.print(".");
        }
      }
      Serial.print(", port ");
      Serial.println(UDP.remotePort());
      // read the packet into packetBufffer
      UDP.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
      Serial.println("Contents:");
      for (int i = 0; i < packetSize; i++)
        Serial.print(packetBuffer[i]);
      Serial.println();
    }
    delay(10);
  }
}// fin void lood


int stringtoInt(String str)
{
  char buff[12];
  str.toCharArray(buff, 10);
  return atoi(buff);
}
int horaEnMinutos(String h, String m)
{
  return (stringtoInt(h) + 1 + vera) * 60 + stringtoInt(m); // GMT +1
}
void diaDelAno(String d, String m)
{
  int aux = 0;
  int dd = stringtoInt(d);
  int mm = stringtoInt(m);
  for (int i = 0; i < mm - 1; i++)
  {
    aux += meses[i];
    if (bisiesto() && i == 1)
      aux += 1;  // febrero en los bisiestos tiene 29 dias
  }
  dia_actual = aux += dd;
}
void verano()
{
  int prim = 0, oto = 0;
  for (int i = 0; i < 13; i++)
  {
    if ( anyo == horario_verano[i][0])
    {
      prim = horario_verano[i][1];
      oto  = horario_verano[i][2];
      Serial.printf("\nPara el anyo %d, el horario de verano es :\nDel %d al %d \n", anyo, prim, oto);
    }
  }
  if ( oto > dia_actual && dia_actual >= prim)
    vera = 1;
  else
    vera = 0;
}
bool bisiesto()
{
  for (int i = 0; i < 4; i++)
  {
    if ( anyo == anos_bisiestos[i]) return true;
  }
  return false;
}
boolean connectUDP() {
  boolean state = false;
  Serial.println("");
  Serial.println("Connecting to UDP");
  if (UDP.begin(localPort) == 1) {
    Serial.println("Connection successful");
    state = true;
  }
  else {
    Serial.println("Connection failed");
  }
  return state;
}
