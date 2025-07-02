#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Configurações do Ponto de Acesso
const char* AP_SSID = "DroneControlAP";       
const char* AP_PASSWORD = "control1234";    

// Configurações MQTT - USAR IP DIRETO
const IPAddress MQTT_BROKER(192, 168, 4, 2); // IP do seu PC na rede do ESP
const int MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "ESP_Antena_Central";

// Tópicos MQTT
const char* TOPIC_DRONE_SYNC = "drone/+/to_antenna";
const char* TOPIC_DRONE_COMMAND = "drone/%s/to_drone";

// Posição da antena (alvo)
const double ANTENNA_LAT = 0.0;
const double ANTENNA_LON = 0.0;

WiFiClient espClient;
PubSubClient client(espClient);

// Protótipos de função
void callback(char* topic, byte* payload, unsigned int length);
void handleSyncMessage(String droneId, JsonDocument& doc);
void handlePositionMessage(String droneId, JsonDocument& doc);
void sendDroneCommand(String droneId, JsonDocument& message);
void reconnect();

void setup_ap() {
  Serial.begin(115200);
  delay(1000); // Espera para serial estabilizar
  Serial.println("\nConfigurando ponto de acesso para controle de drones...");
  
  // Cria o ponto de acesso
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.print("SSID da Central: ");
  Serial.println(AP_SSID);
  Serial.print("Senha: ");
  Serial.println(AP_PASSWORD);
  Serial.print("IP da Central: ");
  Serial.println(WiFi.softAPIP());
  
  // Mostra informações de rede
  Serial.print("Máscara de rede: ");
  Serial.println(IPAddress(255, 255, 255, 0)); // Máscara fixa para /24
  Serial.print("Gateway: ");
  Serial.println(WiFi.softAPIP()); // O próprio AP é o gateway
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Converter payload para String
  String payloadStr;
  for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  Serial.print("\nMensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(payloadStr);

  // Parse do JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  
  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Extrair ID do drone do tópico
  String topicStr(topic);
  int start = topicStr.indexOf('/') + 1;
  int end = topicStr.indexOf('/', start);
  String droneId = topicStr.substring(start, end);
  
  // Verificar tipo de mensagem
  String tipo = doc["tipo"].as<String>();
  
  if (tipo == "sync") {
    handleSyncMessage(droneId, doc);
  } 
  else if (tipo == "posicao") {
    handlePositionMessage(droneId, doc);
  }
}

void handleSyncMessage(String droneId, JsonDocument& doc) {
  Serial.print("Recebido pedido de sincronização do drone ");
  Serial.println(droneId);

  // Verificar se é um pedido de drone pronto
  if (doc["acao"] == "drone_pronto") {
    // Preparar resposta
    StaticJsonDocument<256> response;
    response["tipo"] = "sync";
    response["acao"] = "inicio_permitido";
    response["dados"]["lat"] = ANTENNA_LAT;
    response["dados"]["lon"] = ANTENNA_LON;
    
    // Enviar resposta
    sendDroneCommand(droneId, response);
    Serial.print("Enviado 'inicio_permitido' para drone ");
    Serial.println(droneId);
  }
}

void handlePositionMessage(String droneId, JsonDocument& doc) {
  double droneLat = doc["dados"]["lat"];
  double droneLon = doc["dados"]["lon"];
  double droneAngle = doc["dados"]["angulo"];
  
  Serial.print("Posição recebida do drone ");
  Serial.print(droneId);
  Serial.print(": Lat=");
  Serial.print(droneLat, 6);
  Serial.print(", Lon=");
  Serial.print(droneLon, 6);
  Serial.print(", Ângulo=");
  Serial.println(droneAngle);
  
  // Calcular correção angular (simplificado)
  double deltaLon = ANTENNA_LON - droneLon;
  double deltaLat = ANTENNA_LAT - droneLat;
  double targetAngle = atan2(deltaLat, deltaLon) * 180.0 / PI;
  
  // Normalizar ângulo
  if (targetAngle < 0) targetAngle += 360;
  
  double angleCorrection = 0.9*(targetAngle - droneAngle);
  
  // Ajustar para diferença mínima
  if (angleCorrection > 180) angleCorrection -= 360;
  else if (angleCorrection < -180) angleCorrection += 360;
  
  // Enviar correção
  StaticJsonDocument<128> command;
  command["tipo"] = "comando";
  command["acao"] = "corrigir_angulo";
  command["correcao_angular"] = angleCorrection;
  
  sendDroneCommand(droneId, command);
  
  Serial.print("Enviado comando de correção para drone ");
  Serial.print(droneId);
  Serial.print(": ");
  Serial.print(angleCorrection);
  Serial.println(" graus");
}

void sendDroneCommand(String droneId, JsonDocument& message) {
  // Construir tópico específico para o drone
  char topic[50];
  snprintf(topic, sizeof(topic), TOPIC_DRONE_COMMAND, droneId.c_str());
  
  // Serializar JSON
  String payload;
  serializeJson(message, payload);
  
  // Publicar mensagem
  client.publish(topic, payload.c_str());
}

void reconnect() {
  static int attempt = 0;
  
  while (!client.connected()) {
    attempt++;
    Serial.print("Tentativa ");
    Serial.print(attempt);
    Serial.println(": Conectando ao Broker MQTT...");
    
    if (client.connect(MQTT_CLIENT_ID)) {
      Serial.println("Conectado com sucesso!");
      attempt = 0;
      client.subscribe(TOPIC_DRONE_SYNC);
      Serial.print("Inscrito no tópico: ");
      Serial.println(TOPIC_DRONE_SYNC);
    } else {
      Serial.print("Falha na conexão, rc=");
      Serial.print(client.state());
      Serial.println(". Tentando novamente em 10s...");
     
      
      delay(10000); // Espera 10 segundos
    }
  }
}

void setup() {
  setup_ap();
  
  // Configurar cliente MQTT com IPAddress
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  
  // Adicionar delay para estabilização
  delay(3000);
  
  // Conectar ao broker
  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
