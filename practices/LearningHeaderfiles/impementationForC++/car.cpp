#include "car.h"
#include <iostream>

Car::Car(int age){
    addAge = age;

}

void Car::increaseAge(){
    std::cout<<++addAge;
}

