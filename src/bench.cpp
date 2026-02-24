#include <benchmark/benchmark.h>
#include <sol/sol.hpp>

// ── Struct definitions ────────────────────────────────────────────────────────

struct Vector2 {
    float x, y;
    Vector2(float x, float y) : x(x), y(y) {}
};
struct Vector3 {
    float x, y, z;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};
struct RectF {
    float x, y, w, h;
    RectF(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
};
struct Point {
    int x, y;
    Point(int x, int y) : x(x), y(y) {}
};

// ── Usertype registration ─────────────────────────────────────────────────────

static void register_usertypes(sol::state& lua) {
    lua.open_libraries(sol::lib::base);

    lua.new_usertype<Vector2>("Vector2",
        sol::call_constructor, sol::constructors<Vector2(float, float)>(),
        "x", &Vector2::x,
        "y", &Vector2::y,
        sol::meta_function::addition, [](const Vector2& a, const Vector2& b) {
            return Vector2{ a.x + b.x, a.y + b.y };
        }
    );

    lua.new_usertype<Vector3>("Vector3",
        sol::call_constructor, sol::constructors<Vector3(float, float, float)>(),
        "x", &Vector3::x,
        "y", &Vector3::y,
        "z", &Vector3::z,
        sol::meta_function::addition, [](const Vector3& a, const Vector3& b) {
            return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z };
        }
    );

    lua.new_usertype<RectF>("RectF",
        sol::call_constructor, sol::constructors<RectF(float, float, float, float)>(),
        "x", &RectF::x,
        "y", &RectF::y,
        "w", &RectF::w,
        "h", &RectF::h
    );

    lua.new_usertype<Point>("Point",
        sol::call_constructor, sol::constructors<Point(int, int)>(),
        "x", &Point::x,
        "y", &Point::y
    );
}

// ── Lua scripts ───────────────────────────────────────────────────────────────

static constexpr const char* USERTYPE_SCRIPT = R"lua(
function do_work(n)
    local sum = 0.0
    for i = 1, n do
        local v2a = Vector2(i, i+1)
        local v2b = Vector2(i+2, i+3)
        sum = sum + v2a.x*v2b.x + v2a.y*v2b.y

        local v3a = Vector3(i, i+1, i+2)
        local v3b = Vector3(i+3, i+4, i+5)
        sum = sum + v3a.x*v3b.x + v3a.y*v3b.y + v3a.z*v3b.z

        local r = RectF(i*0.5, i*0.3, 100.0, 50.0)
        sum = sum + r.w * r.h

        local p = Point(i, i+1)
        sum = sum + p.x*p.x + p.y*p.y

        if v2a.x >= r.x and v2a.y >= r.y then
            sum = sum + 1.0
        end
    end
    return sum
end
)lua";

static constexpr const char* TABLE_SCRIPT = R"lua(
function do_work(n)
    local sum = 0.0
    for i = 1, n do
        local v2a = {x=i,   y=i+1}
        local v2b = {x=i+2, y=i+3}
        sum = sum + v2a.x*v2b.x + v2a.y*v2b.y

        local v3a = {x=i,   y=i+1, z=i+2}
        local v3b = {x=i+3, y=i+4, z=i+5}
        sum = sum + v3a.x*v3b.x + v3a.y*v3b.y + v3a.z*v3b.z

        local r = {x=i*0.5, y=i*0.3, w=100.0, h=50.0}
        sum = sum + r.w * r.h

        local p = {x=i, y=i+1}
        sum = sum + p.x*p.x + p.y*p.y

        if v2a.x >= r.x and v2a.y >= r.y then
            sum = sum + 1.0
        end
    end
    return sum
end
)lua";

// ── Benchmarks ────────────────────────────────────────────────────────────────

static void BM_Usertypes(benchmark::State& state) {
    sol::state lua;
    register_usertypes(lua);
    lua.script(USERTYPE_SCRIPT);
    sol::function do_work = lua["do_work"];
    const auto n = state.range(0);
    for (auto _ : state) {
        double result = do_work(n);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Usertypes)->Arg(100)->Arg(1000)->Arg(10000);

static void BM_Tables(benchmark::State& state) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);
    lua.script(TABLE_SCRIPT);
    sol::function do_work = lua["do_work"];
    const auto n = state.range(0);
    for (auto _ : state) {
        double result = do_work(n);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Tables)->Arg(100)->Arg(1000)->Arg(10000);
