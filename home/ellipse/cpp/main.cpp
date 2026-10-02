#include <iostream>

int main() {
    int foo = 67;
    int* ptr = &foo;
    
    std::cout << ptr << '\n';
    return 0;
}
