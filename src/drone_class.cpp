#include <iostream>
#include <array>
#include <random>
#include <iomanip>
#include <chrono>        
#include <thread>
#include <cmath>
#include <unistd.h>

#include "drone_class.h" // Inclui a definicao da classe
#include "drone_functions.h" // Suas funcões de geracao aleatória

#define M_PI 3.14159265358979323846

static const double WALK_DISTANCE = 10.0;
static const float MIN_DIST_TO_ANTENA = 10.1;
static const float ANGLE_VARIATION_ERROR = 10.0;

Drone::Drone(const char *id, const char *host, int port) : mosqpp::mosquittopp(id) {
    this->position = genInitRandPosition();
    this->antena_position = {0.0,0.0};
    this->angle = genInitRandAngle();
    this->reached_antena = false;

    this->pid = getpid();
    this->connected_to_broker = false;
    this->is_sync = false;

    this->topic_to_antenna = "drone/" + std::string(id) + "/to_antenna";
    this->topic_to_drone = "drone/" + std::string(id) + "/to_drone";
    
     // Inicia a conexao com o broker
    connect(host, port, 60);
    // Inicia o loop de rede em uma thread separada
    loop_start();
}

std::array<double, 2> Drone::getPosition() {return this->position;}

void Drone::setPosition(std::array<double, 2> position) {this->position = position;}

std::array<double, 2> Drone::getAntenaPosition() {return this->antena_position;}

void Drone::setAntenaPosition(std::array<double, 2> antena_position) {this->antena_position = antena_position;}

float Drone::getAngle() {return this->angle;}

void Drone::setAngle(float angle) {this->angle = angle;}

bool Drone::getReachedAntena() {return this->reached_antena;}

void Drone::setReachedAntena(bool reached_antena) {this->reached_antena = reached_antena;}


// Evento: conexao estabelecida
void Drone::on_connect(int rc) {
    if (rc == 0) {
        std::cout << "[DRONE " << pid << "] Conectado ao Broker." << std::endl;
        subscribe(NULL, this->topic_to_drone.c_str());
    } else {
        std::cerr << "[DRONE " << pid << "] Falha na conexao." << std::endl;
    }
}

// Evento: inscricao confirmada
void Drone::on_subscribe(int mid, int qos_count, const int *granted_qos) {
    std::cout << "[DRONE " << pid << "] Inscrito para receber comandos." << std::endl;
    this->connected_to_broker = true;
    
    // Envia a mensagem para a antena pedindo para iniciar
    std::cout << "[DRONE " << pid << "] Enviando pedido de sincronizacao para a central..." << std::endl;
    json ready_msg;
    ready_msg["tipo"] = "sync";
    ready_msg["acao"] = "drone_pronto";
    ready_msg["topicos_mqtt"]["topic_to_antenna"] = this ->topic_to_antenna;
    ready_msg["topicos_mqtt"]["topic_to_drone"] = this ->topic_to_drone;
    sendMessage(ready_msg);
}

// Evento: mensagem recebida
void Drone::on_message(const struct mosquitto_message *message) {
    std::string payload(static_cast<char*>(message->payload), message->payloadlen);
    json msg_json;
    try { msg_json = json::parse(payload); } catch(...) { return; }

    // Recebe uma mensagem da central permitindo o inicio
    if (msg_json.value("tipo", "") == "sync" && msg_json.value("acao", "") == "inicio_permitido") {
        std::cout << "[DRONE " << pid << "] Permissao para iniciar recebida!" << std::endl;
        this->antena_position[0] = msg_json["dados"]["lat"];
        this->antena_position[1] = msg_json["dados"]["lon"];
        this->is_sync = true;
    }
    // Recebe correcao do angulo
    else if (msg_json.value("tipo", "") == "comando" && msg_json.value("acao", "") == "corrigir_angulo") {
        std::cout << "[DRONE " << pid << "] Correcao de angulo recebida: " << msg_json["correcao_angular"] << std::endl;
        float angle_fix = msg_json["correcao_angular"];
        // Soma o angulo atual com a correcao recebida
        this->angle += angle_fix;
    }
}

// Envio de mensagem MQTT
void Drone::sendMessage(const json& msg_json) {
    std::string payload = msg_json.dump();
    publish(NULL, this->topic_to_antenna.c_str(), payload.length(), payload.c_str());
}


// Anda
void Drone::walk() {

    // Salvando o angulo antes da troca pra nao perder a variaavel
    float angle_bkp = this->angle;
    // Adiciona uma mudanca inesperada de direcao durante a movimentacao
    this->angle += randAngle(ANGLE_VARIATION_ERROR);
    std::cout << "[DRONE " << pid << "] Angulo alterado na movimentacao: " << (this->angle - angle_bkp) << std::endl;

    // Converter o angulo de graus para radianos
    double angle_rad = this->angle * M_PI / 180.0;

    // Calcular o deslocamento em X (longitude) e Y (latitude)
    double delta_x = WALK_DISTANCE * cos(angle_rad);
    double delta_y = WALK_DISTANCE * sin(angle_rad);

    // Calcular a nova posicao somando o deslocamento à posicao atual
    // Assumindo que position[0] = latitude (eixo Y) e position[1] = longitude (eixo X)
    std::array<double, 2> new_position = {
        this->position[0] + delta_y,
        this->position[1] + delta_x
    };

    // Atualizar a posicao do drone
    this->setPosition(new_position);

    std::cout << "[DRONE " << this->pid << "] Andou. "
                << "Angulo: " << this->angle << " deg. "
                << "Nova Posicao: (Lat: " << new_position[0] 
                << ", Lon: " << new_position[1] << ")" << std::endl;
}


// Verifica se esta na mesma posicao da antena
bool Drone::checkEnd() {
    double dx = getPosition()[1] - getAntenaPosition()[1];
    double dy = getPosition()[0] - getAntenaPosition()[0];
    double distance_to_target = std::sqrt(dx*dx + dy*dy);
    
    // Verifica se a distancia do target esta dentro do range definido
    if (distance_to_target < MIN_DIST_TO_ANTENA) {
        this->reached_antena = true;
    }
    return this->reached_antena;
}


// Inicia o Drone
void Drone::start() {
// Espera a sincronizacao inicial
    std::cout << "[DRONE " << pid << "] Aguardando permissao da central..." << std::endl;
    while (!this->is_sync) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[DRONE " << pid << "] Sincronizado! Iniciando loop principal." << std::endl;

    // Loop principal
    while (!this->reached_antena) {
        
        // Anda e checa se chegou no fim
        walk();
        if (checkEnd()) break; // Se chegou, fim do loop

        // Envia posicao e angulo
        json pos_msg;
        pos_msg["tipo"] = "posicao";
        pos_msg["dados"]["lat"] = this->position[0];
        pos_msg["dados"]["lon"] = this->position[1];
        pos_msg["dados"]["angulo"] = this->angle;
        sendMessage(pos_msg);

        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::cout << "[DRONE " << pid << "] Verificando se houve correcao e andando novamente..." << std::endl;

        // A atualizacao do angulo do drone deve acontecer no maximo ate esta linha

        // Anda com angulo corrigido e checa se chegou no fim
        walk();
        if (checkEnd()) break; // Se chegou, fim do loop

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "================================\n";
    std::cout << "[DRONE " << pid << "] DESTINO ALCANCADO!\n";
    std::cout << "================================\n";
}


int main() {
    mosqpp::lib_init();

    std::string client_id = std::to_string(getpid());
    const char* host = "192.168.4.2";
    int port = 1883;

    std::cout << "Iniciando Drone com ID: " << client_id << std::endl;

    Drone drone(client_id.c_str(), host, port);

    drone.start();

    mosqpp::lib_cleanup();

    std::cout << "Programa do drone encerrado." << std::endl;
    return 0;
}