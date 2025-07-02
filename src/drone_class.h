// drone_class.h
#ifndef DRONE_CLASS_H
#define DRONE_CLASS_H

#include <array>
#include <string>
#include "mosquittopp.h"
#include "json.hpp"


using json = nlohmann::json;


class Drone : public mosqpp::mosquittopp {
private:
    // Atributos de estado do Drone
    std::array<double, 2> position;
    std::array<double, 2> antena_position;
    float angle;
    bool reached_antena;

    // Atributos de comunicação MQTT
    pid_t pid;
    std::string topic_to_antenna;
    std::string topic_to_drone;

    // Flags de controle
    bool connected_to_broker;
    bool is_sync;

    // Metodos assincronos da biblioteca MosquittoMQTT
    void on_connect(int rc) override;
    void on_message(const struct mosquitto_message *message) override;
    void on_subscribe(int mid, int qos_count, const int *granted_qos) override;

public:
    Drone(const char *id, const char *host, int port);

    // Getters e Setters
    std::array<double, 2> getPosition();

    void setPosition(std::array<double, 2> position);

    std::array<double, 2> getAntenaPosition();

    void setAntenaPosition(std::array<double, 2> antena_position);

    float getAngle();

    void setAngle(float angle);

    bool getReachedAntena();
    
    void setReachedAntena(bool reached_antena);

    // Metodos de Acao do Drone
    void walk();
    bool checkEnd();
    void start();
    
    // Metodos de comunicacao
    void sendMessage(const json& msg_json);

};

#endif // DRONE_CLASS_H