// drone_functions.h
#ifndef DRONE_FUNCTIONS_H
#define DRONE_FUNCTIONS_H

#include <iostream>
#include <array>
#include <random>
#include <iomanip>

// Motor de randomizacao
std::mt19937& genRandMotor();

// Gera uma posicao aleatoria
std::array<double, 2> genInitRandPosition();

// Gera um angulo aleatorio
float genInitRandAngle();

float randAngle(float angle_error);


#endif // DRONE_FUNCTIONS_H