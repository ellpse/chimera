#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " <filename>" << endl;
        return 1;
    }

    ifstream file(argv[1]);
    if (!file) {
        cerr << "could not open file " << argv[1] << endl;
        return 1;
    }

    string line;
    while (getline(file, line)) {
        cout << line << "\n";
    }

    file.close();
    return 0;
}
