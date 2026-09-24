# Hextech Engine

C++20-движок для игр на **Vulkan** и **EnTT** (ECS), рассчитанный на переиспользование между проектами: цель — собирать следующую игру без переписывания API ядра. Полный детерминизм симуляции (фиксированный тик, одинаковый ввод → бит-в-бит одинаковый результат), скриптового слоя нет — вся логика на C++, баланс и контент — в JSON с горячей перезагрузкой.

Проект находится в процессе поэтапного архитектурного рефакторинга (strangler fig: новый каркас строится рядом со старым, потребители переводятся по одному). Подробности и статус — ниже и в `docs/`.

## Приложения

- **`apps/era_sandbox`** — градостроительная стратегия со сменой эпох (логистика в стиле Anno: производственные буферы, курьеры, эволюция Каменный → Бронзовый век). Полностью переведена на новую архитектуру (Command/System/Module, ECS как единственный источник состояния, EventBus, детерминированный тик).
- **`apps/sandbox`** — физическая песочница (Jolt Physics, спавн объектов, рейкасты). На том же модульном ядре, что и `era_sandbox`.

## Архитектура

Движок разбит на слои со строго однонаправленными зависимостями (слой N зависит только от N−1 и ниже):

```
L7  apps            apps/sandbox, apps/era_sandbox      Сборка конкретной игры
L6  modules         engine/modules/*                    save, data, economy, timer, spatial, statemachine
L5  runtime         engine/runtime/*                     IModule, ModuleRegistry, Engine, FixedTimestepLoop
L4  world           engine/world/*                       World, CommandRegistry, CommandQueue/Log, WorldHasher
L3  subsystems      engine/render, engine/physics,       Vulkan / Jolt / audio / vfx / ui
                    engine/audio, engine/vfx, engine/ui
L2  resources       engine/assets                        AssetManager, загрузчики моделей и текстур
L1  foundation      engine/foundation                     Handle, TypeRegistry, EventBus, Rng, Result, StringHash
L0  platform        engine/core, engine/integration       Окно, ввод, файлы, время, лог, конфиг, GLFW/ImGui-интеграция
```

Полная спецификация правил (владение состоянием, Command/System/Module, фазы тика, события, детерминизм, рефлексия типов, запрещённые практики) — в [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md). Это конституция проекта: если код противоречит документу — неправ код.

## Статус рефакторинга

По [`docs/REFACTORING_BRIEF.md`](docs/REFACTORING_BRIEF.md), план разбит на этапы 0–10. На данный момент:

**Готово:**
- Этапы 1–6 — фундамент, runtime, ECS-состояние, команды/UI, детерминизм, `SaveModule`.
- Этап 7 — здания и эпохи из `assets/data/*.json`, горячая перезагрузка, `DataModule`.
- Этап 8 — `EconomyModule`, `TimerModule`, `SpatialModule`, `StateMachineModule` с применением в `era_sandbox`.
- Этап 9 — `apps/sandbox` на модульном ядре, `PhysicsBridge` в L4, мёртвый `Application` удалён.
- Этап 10 — редактор: инспектор по `TypeRegistry`, undo/redo полей, save/load сцен, гизмо translate/rotate.

Архитектурные решения фиксируются как ADR в [`docs/adr/`](docs/adr/).

## Стек

- **Рендер:** Vulkan SDK, Vulkan Memory Allocator (VMA)
- **ECS:** [EnTT](https://github.com/skypjack/entt)
- **Физика:** [Jolt Physics](https://github.com/jrouwe/JoltPhysics)
- **Окно/ввод:** GLFW
- **UI (инструментальный):** Dear ImGui
- **Математика:** GLM
- **Данные:** nlohmann/json
- **Звук:** miniaudio
- **Ассеты:** cgltf (glTF), stb_image
- **Тесты:** doctest
- **Сборка:** CMake 3.24+, C++20 (MSVC 2022+ на Windows, GCC/Clang на Linux)

Все зависимости, кроме Vulkan SDK, подтягиваются автоматически через `FetchContent` (`cmake/Dependencies.cmake`).

## Сборка

### Предварительные требования

- CMake 3.24+
- [Vulkan SDK](https://vulkan.lunarg.com/) — на Windows переменная `VULKAN_SDK` выставляется установщиком автоматически; если нет, `cmake/Dependencies.cmake` попробует найти SDK в `C:/VulkanSDK`, `D:/VulkanSDK`, `E:/VulkanSDK`
- Компилятор с поддержкой C++20

### Конфигурация и сборка

```bash
cmake -B build -S .
cmake --build build --config Release
```

Опции CMake (все по умолчанию `ON`): `ENGINE_BUILD_SANDBOX`, `ENGINE_BUILD_ERA_SANDBOX`, `ENGINE_BUILD_TESTS`.

### Запуск

```bash
./build/Release/sandbox.exe
./build/Release/era_sandbox.exe
```

(Точный путь к бинарникам зависит от генератора CMake/IDE.) Необязательный `engine_config.json` в рабочей директории подхватывается автоматически, если присутствует.

### Тесты

```bash
cmake --build build --config Release --target engine_tests
ctest --test-dir build -C Release
```

Тесты (`tests/`, на doctest) покрывают foundation, `ModuleRegistry`, `CommandRegistry`, команды `era_sandbox`, детерминизм, сейвы, загрузку данных, универсальные модули и запись полей инспектора. Обе точки входа также проверяются smoke-тестом (`--smoke-test`, 100 кадров, код выхода 0).

Шейдеры GLSL — в `assets/shaders/`, SPIR-V (`.spv`) собирается автоматически при наличии `glslc` (входит в Vulkan SDK) в `PATH`.

## Структура репозитория

```
engine/
  core/            L0  Окно, ввод, лог, конфиг, время (GLFW-интеграция)
  integration/     L0  Адаптеры GLFW/ImGui
  foundation/      L1  Handle, TypeRegistry, EventBus, Rng, Result, StringHash
  assets/          L2  Загрузка моделей (cgltf) и текстур (stb_image), кэши по хендлам
  renderer/vulkan/ L3  Vulkan-бэкенд, VMA, PBR-рендерер, шейдеры
  physics/         L3  Обёртка Jolt Physics и мост к ECS
  audio/           L3  Звуковой движок (miniaudio)
  vfx/             L3  Система частиц
  ecs/             L3  Общие ECS-компоненты
  ui/              L3  Инспектор, undo/redo, гизмо (ImGui)
  world/           L4  World, CommandRegistry, CommandQueue/Log, WorldHasher
  runtime/         L5  IModule, ModuleRegistry, Engine, FixedTimestepLoop
  modules/         L6  save, data, economy, timer, spatial, statemachine
apps/
  sandbox/         L7  Физическая песочница
  era_sandbox/     L7  Градостроительная песочница
tests/                 Юнит- и детерминизм-тесты (doctest)
docs/
  ARCHITECTURE.md      Целевая архитектура — обязательна к прочтению перед изменением ядра
  REFACTORING_BRIEF.md  Диагноз, план миграции по этапам, правила для агента-исполнителя
  adr/                  Architecture Decision Records
assets/                Шейдеры, модели, звуки, данные
third_party/           Реализации header-only библиотек (cgltf, miniaudio)
```

## Разработка

Перед изменением кода в `engine/` — прочитать [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), особенно раздел 16 (чек-лист запрещённых практик). Правила коротко:

- Игровое состояние — только в `entt::registry`, никаких параллельных карт `id → entity`.
- Связь между подсистемами — только через `EventBus::enqueue`, не колбэки и не синхронный `trigger()`.
- Игровая логика не видит `Time::delta()` — только фиксированный `Tick`.
- Новый компонент — с регистрацией в `TypeRegistry`; новый публичный тип — с тестом в том же PR.
- В `engine/` не должно быть игровых терминов — только в `apps/`.
