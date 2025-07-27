#include <iostream>
#include "functionality.h"


using namespace std;


int main(){
    cout<< add(3, 4)<<endl;
    cout<< sub(6, 7)<<endl;
    

    string Carname;
    int yearm;
    int model;
    cout<<"Enter the name of the car: ";
    cin>>Carname;

    cout<<"Enter the Model of the car: ";
    cin>>model;

    cout<<"Enter the year it was made: ";
    cin>>yearm;

    Car c1(model, Carname, yearm);
    c1.carInfo();

    return 0;
}