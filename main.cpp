#include "engine/threedl.hpp"

int main() {
    tdl::ThreeDL app {};

    const auto plane_one = tdl::make_model("../assets/F-18.obj");
    const auto plane_two = tdl::make_model("../assets/B-2.obj", "../assets/B-2.png");

    const auto camera = tdl::make_camera<tdl::DefaultController>(16.0f/9.0f);

    app.setCamera(camera);

    plane_one->rotate({0, -std::numbers::pi/2, 0}, {0, 0, 0});
    //plane_one->translate({0, 1, 0});

    app.add(plane_one);
    app.add(plane_two);

    app.start([plane_one]() {
       // plane_one->rotate({0, 0.001, 0});
    });

    return 0;
}