#include "Application/Application.h"

#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
    try {
        Application application;
        return application.run(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error\n";
        return 1;
    }
}
