#include "engine/threedl.hpp"

int main() {
    tdl::ThreeDL app {};

    const auto plane_one = tdl::make_model("../assets/Floor.obj", tdl::big_buck_bunny);

    const auto camera = tdl::make_camera<tdl::DefaultController>(16.0f/9.0f);

    const auto light = tdl::make_light<tdl::PointLight>(glm::vec4 {0, 0, 20, 0});

    app.setCamera(camera);

    plane_one->rotate({-std::numbers::pi/2, 0, 0}, {0, 0, 0});
    plane_one->translate({0, 2, 0});

    app.add(plane_one);

    app.add(light);

    float t = 0;
    app.start([&]() {
    });

    return 0;
}