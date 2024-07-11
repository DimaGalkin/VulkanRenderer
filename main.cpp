#include "engine/threedl.hpp"

int main() {
    ThreeDL app {};

    const auto plane_one = std::make_shared<Object>("../assets/MiG35.obj", "../assets/MiG35.png");
    const auto plane_two = std::make_shared<Object>("../assets/B-2.obj", "../assets/B-2.png");

    app.add(plane_one);
    app.add(plane_two);

    app.start();

    return 0;
}