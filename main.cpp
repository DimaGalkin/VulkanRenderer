#include "engine/threedl.hpp"

const auto plane_one = makeObject("../assets/F-18.obj", "../assets/F-18.png");

int main() {
    ThreeDL<tdl::Control::Controlled> app {};

    const auto camera = std::make_shared<tdl::FPSController>(16.0f/9.0f);

    app.setCamera<tdl::FPSController>(camera);

    plane_one->rotate({0, -glm::pi<float>() / 2, 0}, {0, 0, 0});

    app.add(plane_one);

    app.start([]() {
        plane_one->translate({0, 0, 0.05});
        plane_one->rotate({0.0001, 0.0001, 0}, SELFCENTRE);
    });

    return 0;
}