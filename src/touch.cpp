#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    std::ofstream file(argv[1]);
    file.close();
    std::cout << "created file " << argv[1] << '\n';
    return 0;
}
