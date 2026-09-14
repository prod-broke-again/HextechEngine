#include <doctest/doctest.h>
#include <engine/foundation/Result.hpp>
#include <engine/foundation/StringHash.hpp>
#include <engine/foundation/Handle.hpp>
#include <engine/foundation/TypeRegistry.hpp>
#include <engine/foundation/EventBus.hpp>
#include <engine/foundation/Rng.hpp>

using namespace engine;

TEST_CASE("Result - Value semantics") {
    Result<int, std::string> res1 = 42;
    CHECK(res1.isOk() == true);
    CHECK(res1.isErr() == false);
    CHECK(res1.value() == 42);

    Result<int, std::string> res2 = std::string("error");
    CHECK(res2.isOk() == false);
    CHECK(res2.isErr() == true);
    CHECK(res2.error() == "error");
}

TEST_CASE("Result - Void semantics") {
    Result<void, int> res1 = Result<void, int>::ok();
    CHECK(res1.isOk() == true);
    CHECK(static_cast<bool>(res1) == true);
    CHECK_FALSE(!res1);

    Result<void, int> res2 = Result<void, int>::error(404);
    CHECK(res2.isOk() == false);
    CHECK(!res2);
    CHECK(res2.error() == 404);
}

TEST_CASE("StringHash - Deterministic Hashing") {
    StringHash hash1("TestString");
    StringHash hash2("TestString");
    StringHash hash3("AnotherString");

    CHECK(hash1.value() == hash2.value());
    CHECK(hash1 == hash2);
    CHECK(hash1 != hash3);

    // Compile-time
    constexpr StringHash ctHash("Constexpr");
    CHECK(ctHash.value() != 0);
}

struct DummyEntity {
    int x;
};

TEST_CASE("Handle - Verification") {
    Handle<DummyEntity> h1{1, 1};
    Handle<DummyEntity> h2{1, 1};
    Handle<DummyEntity> h3{1, 2};
    Handle<DummyEntity> hEmpty;

    CHECK(h1 == h2);
    CHECK(h1 != h3);
    CHECK(h1.isValid() == true);
    CHECK(hEmpty.isValid() == false);
}

struct TestEvent {
    int value;
};

struct EventListener {
    int receivedValue = 0;
    void onEvent(const TestEvent& e) {
        receivedValue = e.value;
    }
};

TEST_CASE("EventBus - Enqueue and Drain") {
    EventBus bus;
    EventListener listener;
    
    bus.connect<TestEvent, &EventListener::onEvent>(&listener);
    bus.enqueue(TestEvent{42});
    
    // Not drained yet
    CHECK(listener.receivedValue == 0);
    
    bus.drain();
    
    // Drained
    CHECK(listener.receivedValue == 42);
}

TEST_CASE("Rng - Deterministic Generation") {
    Rng rng1(12345);
    Rng rng2(12345);
    Rng rng3(54321);

    uint32_t val1 = rng1.next();
    uint32_t val2 = rng2.next();
    uint32_t val3 = rng3.next();

    CHECK(val1 == val2);
    CHECK(val1 != val3);
    
    // Check float generation range
    float f = rng1.nextFloat();
    CHECK(f >= 0.0f);
    CHECK(f < 1.0f);
    
    float f2 = rng1.nextFloat(-10.0f, 10.0f);
    CHECK(f2 >= -10.0f);
    CHECK(f2 < 10.0f);
    
    int i = rng1.nextInt(5, 10);
    CHECK(i >= 5);
    CHECK(i <= 10);
}
