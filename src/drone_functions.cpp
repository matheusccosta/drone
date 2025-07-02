#include <iostream>
#include <array>
#include <random>
#include <iomanip>

#include "drone_functions.h"

// Motor de randomizacao
std::mt19937& genRandMotor() {
    static std::mt19937 motor(std::random_device{}());
    return motor;
}

// Gera uma posicao aleatoria
std::array<double, 2> genInitRandPosition() {
    std::uniform_real_distribution<double> distrib_latitude(-90.0,90.0);
    std::uniform_real_distribution<double> distrib_longitude(-180.0,180.0);

    double lat = distrib_latitude(genRandMotor());
    double lon = distrib_longitude(genRandMotor());

    return {lat, lon};
}

// Gera um angulo aleatorio inicial
float genInitRandAngle() {
    std::uniform_real_distribution<float> distrib_angle(-180.0f,180.0f);
    return distrib_angle(genRandMotor());
}

// Gera uma randomizacao de angulo
float randAngle(float angle_error) {
    std::uniform_real_distribution<float> distrib_angle(-1*(angle_error),angle_error);
    return distrib_angle(genRandMotor());
}