#include "engine/threedl.hpp"

int main() {
    ThreeDL app {};

    auto plane_one = std::make_shared<Mesh>("../MiG35.obj");
    auto plane_two = std::make_shared<Mesh>("../mig2.obj");

    app.add(plane_one);
    app.add(plane_one);

    app.start();

    return EXIT_SUCCESS;
}