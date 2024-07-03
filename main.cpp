#include "engine/threedl.hpp"

int main() {
    ThreeDL app {};

    const auto plane_one = std::make_shared<Object>("../MiG35.obj", "a.png");
    const auto plane_two = std::make_shared<Object>("../B-2.obj", "");

    app.add(plane_one);
    app.add(plane_two);

    app.start();

    return 0;
}