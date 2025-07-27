#include "car.h"
#include <iostream>

Car::Car(int age){
    addAge = age;

}

void Car::increaseAge(){
    std::cout<<++addAge;
}

/*in our implemntation file we have the actual functionallity of our classes */