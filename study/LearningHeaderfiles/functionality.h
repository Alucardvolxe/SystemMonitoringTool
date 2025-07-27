#include <iostream>
#include <string>


int add(int, int);

int sub(int, int);

void output_message (std::string);

class Car {
public:
    int carModel;
    std::string carName;
    int yearMade;

    Car(int model, std::string name, int made);
    void carInfo();
};