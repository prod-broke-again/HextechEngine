#include <doctest/doctest.h>

#include "engine/foundation/TypeRegistry.hpp"
#include "engine/ui/FieldAccess.hpp"

using namespace engine;
using namespace engine::ui;

namespace {

struct InspectSample {
    int count = 0;
    float speed = 0.0f;
    bool enabled = false;
};

struct NestedBlob {
    int inner = 0;
};

struct InspectWithUnknown {
    int visible = 1;
    NestedBlob blob{};
    bool flag = true;
};

} // namespace

TEST_CASE("FieldAccess - write and read primitive fields") {
    TypeRegistry types;
    types.registerComponent<InspectSample>("InspectSample", 1)
        .field("count", &InspectSample::count)
        .field("speed", &InspectSample::speed)
        .field("enabled", &InspectSample::enabled);

    const ComponentDesc* desc = types.getComponentDesc<InspectSample>();
    REQUIRE(desc != nullptr);
    REQUIRE(desc->fields.size() == 3);

    InspectSample sample;
    REQUIRE(writeField(&sample, desc->fields[0], 7));
    REQUIRE(writeField(&sample, desc->fields[1], 2.5f));
    REQUIRE(writeField(&sample, desc->fields[2], true));

    CHECK(sample.count == 7);
    CHECK(sample.speed == 2.5f);
    CHECK(sample.enabled);

    InspectSample copy{};
    REQUIRE(readFieldBytes(&sample, desc->fields[0], &copy.count, sizeof(copy.count)));
    REQUIRE(readFieldBytes(&sample, desc->fields[1], &copy.speed, sizeof(copy.speed)));
    REQUIRE(readFieldBytes(&sample, desc->fields[2], &copy.enabled, sizeof(copy.enabled)));
    CHECK(copy.count == sample.count);
    CHECK(copy.speed == sample.speed);
    CHECK(copy.enabled == sample.enabled);

    int count = 0;
    float speed = 0.0f;
    bool enabled = false;
    REQUIRE(readField(&sample, desc->fields[0], count));
    REQUIRE(readField(&sample, desc->fields[1], speed));
    REQUIRE(readField(&sample, desc->fields[2], enabled));
    CHECK(count == 7);
    CHECK(speed == 2.5f);
    CHECK(enabled);

    CHECK_FALSE(writeField(&sample, desc->fields[0], 1.0f));
}

TEST_CASE("FieldAccess - unknown and transient fields remain traversable") {
    TypeRegistry types;
    types.registerComponent<InspectWithUnknown>("InspectWithUnknown", 1)
        .field("visible", &InspectWithUnknown::visible)
        .transientField("blob", &InspectWithUnknown::blob)
        .field("flag", &InspectWithUnknown::flag);

    const ComponentDesc* desc = types.getComponentDesc<InspectWithUnknown>();
    REQUIRE(desc != nullptr);
    REQUIRE(desc->fields.size() == 3);

    CHECK(desc->fields[0].name == "visible");
    CHECK(desc->fields[0].type == FieldType::Int32);
    CHECK_FALSE(desc->fields[0].transient);

    CHECK(desc->fields[1].name == "blob");
    CHECK(desc->fields[1].type == FieldType::Unknown);
    CHECK(desc->fields[1].transient);

    CHECK(desc->fields[2].name == "flag");
    CHECK(desc->fields[2].type == FieldType::Bool);
    CHECK_FALSE(desc->fields[2].transient);

    InspectWithUnknown sample{3, NestedBlob{11}, true};
    int visible = 0;
    REQUIRE(readField(&sample, desc->fields[0], visible));
    CHECK(visible == 3);

    bool flag = true;
    REQUIRE(writeField(&sample, desc->fields[2], false));
    REQUIRE(readField(&sample, desc->fields[2], flag));
    CHECK_FALSE(flag);
    CHECK(sample.blob.inner == 11);
}
