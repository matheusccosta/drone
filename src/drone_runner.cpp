#include <iostream>
#include <vector>
#include <unistd.h>    
#include <sys/wait.h>    
#include <chrono>        
#include <thread>       
#include "drone_functions.cpp"

const int NUM_DRONES = 5;

// Inicia a quantidade de drones necessárias com o target em startDrone()
void drone_runner(int id) {

    // Linhas abaixo são apenas para teste, nao representam o que a funcao realmente fará
    std::cout << "Processo Drone " << id << " (PID: " << getpid() << ") iniciou a execução." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Processo Drone " << id << " (PID: " << getpid() << ") finalizou a execução." << std::endl;
}

// Main so deve chemar drone_runner() e ele faz toda esta gerencia
int main() {
    std::cout << "Processo Principal (PID: " << getpid() << ") iniciando " << NUM_DRONES << " drones..." << std::endl;

    std::vector<pid_t> pids;

    for (int i = 0; i < NUM_DRONES; ++i) {
        pid_t pid = fork();

        // Falha ao criar o processo
        if (pid < 0) {
            std::cerr << "Failed to fork" << std::endl;
            return 1;

        // Processo criado corretamente
        } else if (pid == 0) {
            // Se o pid for 0, este é o código que o PROCESSO FILHO executa.
            drone_runner(i + 1);

            // O processo filho deve terminar aqui para não continuar executando o loop do pai.
            // exit(0) encerra o processo filho com sucesso.
            exit(0);

        } else {
            // Se o pid for positivo, este é o código que o PROCESSO PAI executa.
            // A variável 'pid' contém o ID do processo filho que acabamos de criar.
            pids.push_back(pid);
        }
    }

    std::cout << "Processo Principal aguardando todos os processos filhos darem 'join'..." << std::endl;

    // O processo pai agora espera por cada filho terminar.
    // É o análogo do 'thread::join()'.
    for (int i = 0; i < NUM_DRONES; ++i) {
        int status;
        wait(&status);
    }
    // Alternativamente, para esperar por PIDs específicos:
    // for (pid_t pid : pids) {
    //     waitpid(pid, &status, 0);
    // }


    std::cout << "Todos os processos drones finalizaram. O programa principal será encerrado." << std::endl;

    return 0;
}