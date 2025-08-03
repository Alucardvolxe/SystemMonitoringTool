#include <vector>
#include <iostream>

int main(){
    std::vector<int> number;

    number.push_back(1);


    for(int i: number){
        std::cout<<i;
    }
}