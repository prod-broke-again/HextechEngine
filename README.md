# Hextech Engine

C++20-движок для игр на **Vulkan** и **EnTT** (ECS). Цель — собирать следующую игру на том же API ядра, без переписывания слоёв. Симуляция детерминирована (фиксированный тик: одинаковый ввод → одинаковый результат). Скриптового слоя нет: логика на C++, баланс и контент — в JSON с горячей перезагрузкой (F5).

Архитектурный рефакторинг по [`docs/REFACTORING_BRIEF.md`](docs/REFACTORING_BRIEF.md) (этапы 0–10) **закрыт**. Оба приложения живут на модульном ядре. Дальше — игровая логика в `apps/`, не новые слои движка.

## Приложения

### `apps/era_sandbox` — город на сетке

Градостроительная песочница в духе Anno: Каменный → Бронзовый век, склад в центре (костёр / ратуша), добыча, жильё, налоги.

**Уже в игре:**
- Постройка и снос с клетки; перед установкой **R** крутит здание на 90° (четыре положения).
- Производственные буферы на лесорубке, рыбалке и т.п.; курьеры забирают товар на склад.
- Курьеры идут **только по ортогонали** (без диагоналей). По траве сами оставляют узкую грязную тропу; по ней же возвращаются.
- **Мостовая** (`Paved Road`) кладётся поверх тропы: курьеры предпочитают её и идут чуть быстрее.
- Жильё потребляет рыбу и дрова со склада, платит налог, растёт/пустеет от удовлетворённости. До домов курьеры пока **не** возят — только склад ↔ добыча.
- Данные зданий и эпох — `assets/data/*.json`.

**Редактор в той же демке:** инспектор полей по `TypeRegistry`, undo/redo, гизмо translate/rotate, save/load сцены.

### `apps/sandbox` — физика

Песочница на Jolt: спавн тел, рейкасты, свободная камера. То же модульное ядро и тот же редактор сцен. Ввод мыши обрабатывается каждый кадр отрисовки (не только на тике симуляции).

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

## Статус

План в [`docs/REFACTORING_BRIEF.md`](docs/REFACTORING_BRIEF.md) выполнен:

| Этап | Что появилось |
|---|---|
| 1–6 | Фундамент, runtime, ECS как единственное состояние, команды, детерминизм, `SaveModule` |
| 7 | Здания и эпохи из JSON, `DataModule`, горячая перезагрузка |
| 8 | `EconomyModule`, `TimerModule`, `SpatialModule`, `StateMachineModule` |
| 9 | `sandbox` на том же ядре, `PhysicsBridge` в L4, старый `Application` удалён |
| 10 | Инспектор по `TypeRegistry`, undo/redo, гизмо, save/load сцен |

**Сейчас в работе — геймплей `era_sandbox`:** тропы и дороги уже есть; следующие крупные дыры — доставка до домов, фундаменты больше 1×1, сейв самой симуляции (не только editor-сцены).

ADR — в [`docs/adr/`](docs/adr/).

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
./build/apps/sandbox/Release/sandbox.exe
./build/apps/era_sandbox/Release/era_sandbox.exe
```

Путь зависит от генератора (у multi-config MSVC это `Release/` внутри `apps/...`). Необязательный `engine_config.json` в рабочей директории подхватывается сам.

### Тесты

```bash
cmake --build build --config Release --target engine_tests
ctest --test-dir build -C Release
```

Тесты (`tests/`, doctest) покрывают foundation, модули, команды города, **pathfinder курьеров**, детерминизм (replay 10 000 тиков), сейвы, JSON-данные, инспектор и undo. Оба exe принимают `--smoke-test` (100 кадров, код выхода 0).

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
  era_sandbox/     L7  Город: симуляция, pathfinder, UI, процедурные меши
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
