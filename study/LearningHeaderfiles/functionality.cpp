#include <iostream>
#include "functionality.h"

int add(int a, int b)
{
    return a + b;
}

int sub(int a, int b)
{
    return a - b;
}

void message(std::string mes)
{
    std::cout << mes;
}

Car::Car(int model, std::string name, int made)
    : carModel(model), carName(name), yearMade(made) 
{}

void Car::carInfo() {
    std::cout << "Car Name: " << carName << "\n";
    std::cout << "Car Model: " << carModel << "\n";
    std::cout << "Year Made: " << yearMade << "\n";
}