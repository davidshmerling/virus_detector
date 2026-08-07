#include "CrashHandler/SegfaultHandler.h"

#include <exception>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[])
{
    installSegfaultHandler();

    try {
        // Application entry point will go here.
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error\n";
        return 1;
    }
}
