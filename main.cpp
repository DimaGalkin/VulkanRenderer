#include "engine/threedl.hpp"

const auto plane_one = tdl::make_model("../assets/F-18.obj");

int main() {
    tdl::ThreeDL app {};

    const auto camera = tdl::make_camera<tdl::DefaultController>(16.0f/9.0f);

    app.setCamera(camera);

    plane_one->translate({0, 1, 0});

    app.add(plane_one);

    app.start();

    return 0;
}