# Mosquito Simulator — Series of Prompts for MVP (v0.1)

План шагов для Cursor. Дизайн и числа — в `ai/GDD.md`. Один промт = один шаг. Не переходить дальше, пока текущий не запускается.

**Статус:** Промпты 1–10 выполнены в C++ (без ассетов): модуль/GameMode/PlayerController; `AMosquitoCharacter` — статы + полёт + камера + `ApplySwatHit` + посадка/укус/тик статов; 3 человеков `AHumanCharacter` (FSM Calm→…→Chase, PerformAttack, OnBitten); `ADayNightSystem` (сутки 120 с, солнце/луна/туман); `AMosquitoHUD` (5 баров, состояние человека, часы, прогресс укуса, прицел, вспышка урона). НЕ сделано: геймпад, настоящий уровень, очки погони (Prompt 11), смерть/респавн (Prompt 12), плейтест-гейт (Prompt 13). Детальный журнал: `ai/PROGRESS.md`. Следующий шаг — Prompt 11 (Chase Score).

---

## PROMPT 1 — Project Structure & GameMode

**Цель:** Создать базовую структуру C++ проекта.

**Что сделать:**
1. Создать `Source/MosquitoSimulator/` с `Build.cs`
2. `MosquitoSimulator.h` — модуль (module)
3. `MosquitoSimulator.cpp` — module implementation
4. `MosquitoSimulatorGameModeBase.h/.cpp` — базовый GameMode
5. `MosquitoSimulatorPlayerController.h/.cpp` — базовый PlayerController
6. Настроить `DefaultGame.ini` — указать GameMode по умолчанию
7. Создать начальный уровень `/Game/Maps/MainLevel`

**Ограничения:**
- Минимальная логика, только скелет
- Без Blueprint — всё C++
- Не создавать персонажи, камеры, AI — это следующие шаги

---

## PROMPT 2 — Mosquito Player Character (Base)

**Цель:** Создать базовый класс комара с параметрами выживания.

**Что сделать:**
1. `MosquitoCharacter.h/.cpp` — наследник ACharacter
2. Параметры (UPROPERTY(EditDefaultsOnly, Category = "Mosquito"):
   - `MaxHealth` (float)
   - `CurrentHealth` (float)
   - `MaxBlood` (float)
   - `CurrentBlood` (float)
   - `MaxEnergy` (float)
   - `CurrentEnergy` (float)
   - `FlightSpeed` (float)
   - `NoiseLevel` (float)
3. Базовые getters/setters
4. Регистр компонентов (CapsuleComponent как основа)
5. Подключить как PlayerPawn в GameMode

**Ограничения:**
- Без движения, без камеры — только данные и регистрация

---

## PROMPT 3 — Mosquito Movement & Flight

**Цель:** Реализовать управление полётом комара.

**Что сделать:**
1. Добавить в `MosquitoCharacter`:
   - `MoveForward(float)` — движение вперёд/назад
   - `MoveRight(float)` — движение влево/вправо
   - `MoveUp(float)` — вертикальное движение (W/S или Q/E)
   - `Turn(float)` — поворот камеры
   - `LookUp(float)` — наклон камеры вверх/вниз
2. Использовать `AddMovementInput` + `AddControllerYawInput`/`AddControllerPitchInput`
3. Настроить Input Action (Enhanced Input):
   - Forward/Back (Mouse Y или Gamepad LeftY)
   - Left/Right (Mouse X или Gamepad LeftX)
   - Up/Down (Mouse Wheel или Gamepad RightY)
4. Сделать полёт отзывчивым, но с небольшой инерцией (для ощущения масштаба)
5. Добавить `bIsFlying` — флаг состояния

**Ограничения:**
- Без физики ветра, без столкновений — только базовое управление

---

## PROMPT 4 — Camera System

**Цель:** Камера от третьего лица, следующая за комаром.

**Что сделать:**
1. В `MosquitoCharacter` или `MosquitoPlayerController`:
   - Создать `USpringArmComponent` (длина ~15-20 см в игровых единицах)
   - Создать `UCameraComponent` на SpringArm
   - Настроить `bUsePawnControlRotation = true`
2. Камера должна:
   - Следовать за комаром плавно (SpringArm)
   - Позволять вращать обзор мышью
   - Быть достаточно близко, чтобы видеть окружение в масштабе
3. Настроить `MosquitoPlayerController` для обработки ввода камеры
4. Добавить настройку FOV (например, 70-90 для ощущения маленького существа)

**Ограничения:**
- Без эффектов (туман, расстояние прорисовки) — это позже

---

## PROMPT 5 — Input Setup (Enhanced Input)

**Цель:** Полная настройка Enhanced Input для комара.

**Что сделать:**
1. Создать `InputSettings` (или настроить в коде):
   - Action Maps: `AM_Flight`
   - Actions:
     - `IA_FlyForward` (Mouse Y / Gamepad LeftY)
     - `IA_FlyRight` (Mouse X / Gamepad LeftX)
     - `IA_FlyUp` (Mouse Wheel / Gamepad RightY)
     - `IA_LookYaw` (Mouse X / Gamepad RightX)
     - `IA_LookPitch` (Mouse Y / Gamepad RightY)
     - `IA_Bite` (Left Click / Gamepad A)
     - `IA_Sense` (Right Click / Gamepad X — заглушка)
2. Подключить в `MosquitoPlayerController`:
   - `SetupInputComponent()` — binding на actions
3. Связать actions с методами `MosquitoCharacter`

**Ограничения:**
- Логика Bite и Sense — заглушки, реальная реализация позже

---

## PROMPT 6 — World Blockout (Level 0.1)

**Цель:** Создать базовый уровень — территория поселка.

**Что сделать:**
1. Создать уровень `/Game/Maps/MainLevel`
2. Добавить:
   - `UGroundPlane` — трава (большая плоскость, ~20000x20000 units)
   - `USimpleMesh` — дом (коробка ~500x400x300 units)
   - `USimpleMesh` — 2-3 стола (на улице)
   - `USimpleMesh` — несколько деревьев/кустов (цилиндры/сферы)
   - `USimpleMesh` — лужа/ведро с водой
   - `UDirectionalLight` — солнце
   - `UExponentialHeightFog` — атмосфера
   - `UPostProcessVolume` — базовый tone mapping
3. Установить `MainLevel` как Default Map в `DefaultEngine.ini`
4. Расставить объекты так, чтобы было интересно летать

**Ограничения:**
- Всё из примитивов (Box, Sphere, Cylinder) — без статик мешей
- Цель — протестировать масштаб и полёт

---

## PROMPT 7 — Human NPC Character (Base)

**Цель:** Создать базовый класс человека-NPC.

**Что сделать:**
1. `HumanCharacter.h/.cpp` — наследник ACharacter (или AActor)
2. Параметры:
   - `IrritationLevel` (int, 0-4)
   - `CurrentState` (enum: Calm, Noticed, Irritated, Angry, Chase)
   - `AttackCooldown` (float)
   - `DetectionRadius` (float)
3. Базовая анимация/поведение:
   - Calm: стоит на месте или медленно ходит
   - Noticed: поворачивается в сторону комара
   - Irritated: начинает чесаться / махать рукой
   - Angry: ищет комара
   - Chase: бежит за комаром
4. `UAttackComponent` или метод `PerformAttack()` — хлопок/попытка убить
5. Поставить 2-3 экземпляра на уровень

**Ограничения:**
- Без AI Behavior Tree — простое состояние в самом классе
- Без анимаций — использовать простые меш/скелет или capsule

---

## PROMPT 8 — Day/Night Cycle

**Цель:** Система смены дня и ночи.

**Что сделать:**
1. `DayNightSystem.h/.cpp` — GameInstance или Actor-система
2. Параметры:
   - `DayLengthSeconds` (float, например 120 = 2 минуты = 1 день)
   - `CurrentTime` (float, 0-24)
   - `SunRotationSpeed` (float)
3. Логика:
   - Вращать `UDirectionalLight` (солнце)
   - Менять интенсивность/цвет света в зависимости от времени
   - Менять `UExponentialHeightFog` цвет/интенсивность
   - Можно менять `PostProcessVolume` Exposure
4. Экспорт переменных для Blueprint (чтобы дизайнер мог настраивать)
5. Начать с рассвета (например, 6:00)

**Ограничения:**
- Без погоды, без дождя — только свет и время

---

> ⚠️ **РЕВИЗИЯ 2026-09-05.** Промпты 9–13 ниже — исходная редакция, написанная
> ДО начала реализации. Фактически большая часть старого Prompt 10 (реакция,
> атака, CHASE, спад раздражения) уже сделана в Prompt 7, а «настоящий уровень»
> (Prompt 6) осознанно отложен — работает блокаут. АКТУАЛЬНЫЙ план остатка MVP —
> в конце файла: «ПЛАН v0.1 (РЕВИЗИЯ)». Следовать ревизии.

## PROMPT 9 — Bite System & Blood

**Цель:** Механика посадки на человека и укуса.

**Что сделать:**
1. В `MosquitoCharacter`:
   - `DetectNearbyHumans()` — LineTrace или SphereOverlap вперёд/вокруг
   - `ApproachAndLandOnHuman(AActor* Target)` — логика подхода
   - `PerformBite()` — укус, добавление крови
2. В `HumanCharacter`:
   - `OnBitten(float BloodAmount)` — получение укуса
   - Увеличение `IrritationLevel`
   - Переключение состояния
3. Условия укуса:
   - Комар должен быть рядом с человеком (< 50 units)
   - Комар должен «сесть» (нажать Bite)
   - Короткая задержка (0.5-1 сек) во время укуса
4. Результат:
   - `CurrentBlood += BloodAmount`
   - `IrritationLevel++`
   - Человек может атаковать

**Ограничения:**
- Без визуальных эффектов укуса
- Без UI отображения крови

---

## PROMPT 10 — Human Reaction & Chase Mode

**Цель:** Реакция человека и режим погони.

**Что сделать:**
1. В `HumanCharacter`:
   - При `IrritationLevel >= 1`:
     - `bIsAwareOfMosquito = true`
     - Поворачиваться в сторону комара
   - При `IrritationLevel >= 2`:
     - Периодически махать рукой (Attack)
   - При `IrritationLevel >= 4`:
     - **CHASE MODE**
     - Двигаться к позиции комара
     - Скорость выше обычной
     - Периодические атаки
2. Логика атаки человека:
   - `PerformAttack()` — SphereOverlap вокруг руки
   - Если комар в зоне → `MosquitoCharacter->TakeDamage()`
   - Воздушная волна (оттолкнуть комара)
3. Снижение раздражения:
   - Если комар улетел далеко > 300 units:
     - `IrritationLevel -= 1` каждые N секунд
   - Доходит до Calm
4. Очки за погоню (заглушка):
   - Таймер погони
   - При завершении: `Score += Duration * Multiplier`

**Ограничения:**
- Без AI навигации — человек просто движется к позиции комара
- Без Pathfinding — прямой путь

---

## PROMPT 11 — UI Basics (HUD)

**Цель:** Минимальный HUD для отображения состояния комара.

**Что сделать:**
1. `MosquitoHUD.h/.cpp` — наследник AHUD
2. `MosquitoWidget.h/.cpp` — UMG Widget:
   - Полоска Health
   - Полоска Blood
   - Полоска Energy
   - Индикатор состояния текущего человека (если рядом)
   - Время суток (цифры или иконка)
3. Подключить HUD в `MosquitoPlayerController`:
   - `BeginPlay()` → Create HUD widget
   - `ShowHUD()`
4. Обновление полосок в `Tick()` или при изменении значений

**Ограничения:**
- Простые UProgressBar / UBorder — без красивой графики
- Цвета: зелёный (норма), жёлтый (низко), красный (критично)

---

## PROMPT 12 — Wing Damage & Mosquito Death

**Цель:** Система повреждений крыльев и смерти.

**Что сделать:**
1. В `MosquitoCharacter`:
   - `WingCondition` (float, 0-100)
   - `TakeDamage(float Amount)` — от атак человека
   - При получении урона:
     - `WingCondition -= Amount`
     - `CurrentHealth -= Amount`
     - Если `WingCondition < 50` → `FlightSpeed *= 0.75`
     - Если `WingCondition < 25` → `FlightSpeed *= 0.5`
     - Если `CurrentHealth <= 0` → `Die()`
2. `Die()`:
     - Показать сообщение "Mosquito Died"
     - Respawn через 3 секунды на стартовой позиции
     - Сбросить Blood/Energy до базовых
3. Визуальная обратная связь:
     - Красная вспышка при получении урона
     - Тряска камеры при повреждении крыльев

**Ограничения:**
- Без системы поколений — просто respawn
- Без частиц — простая красная вспышка через PostProcess или Material

---

## PROMPT 13 — Polish & Integration

**Цель:** Собрать всё вместе, проверить полный цикл.

**Что сделать:**
1. Убедиться, что полный цикл работает:
   - Запуск → управление комаром → поиск человека → укус → реакция → погоня → смерть/выживание
2. Балансировка:
   - Скорости
   - Радиусы обнаружения
   - Урон атак
   - Скорость снижения раздражения
3. Проверить:
   - Комар ощущается маленьким?
   - Человек ощущается большим?
   - Полёт отзывчивый?
   - Камера удобная?
4. Исправить баги
5. Убедиться, что проект компилируется и запускается

---

## Итого: 13 промтов для MVP

Каждый промт — один шаг. После каждого:
1. Описать что сделано
2. Показать файлы
3. Проверить компиляцию
4. Ждать решения: переходить дальше или исправлять

После Prompt 12 (Polish) — MVP v0.1 готов.

---

# ПЛАН v0.1 — РЕВИЗИЯ 2026-09-05 (актуальные промпты 9–13)

> Промпты 9–13 выше — исходная редакция (писались до реализации). Факт:
> большая часть старого Prompt 10 уже готова (сделана в Prompt 7), «настоящий
> уровень» отложен. Источник истины по состоянию проекта — `ai/PROGRESS.md`.

## Правила процесса (для любого агента/ИИ)
1. Одна сессия = один промпт; handoff между сессиями — через `ai/PROGRESS.md`.
2. Definition of Done каждого промпта: сборка Succeeded → headless-прогон чист
   (0 fatal/ensure + ожидаемые лог-маркеры) → чек-лист человеку в PIE (3–5 мин)
   → `ai/PROGRESS.md` обновлён.
3. Детали API НЕ верить промптам — проверять по исходникам движка
   (`E:\UE_5.8\Engine\Source`); известные ловушки 5.8 — в PROGRESS.md п.11.
4. Лог-конвенция: `[Mosquito]`, `[Human]`, `[DayNight]` (+ новые префиксы в том
   же стиле) — по маркерам работает headless-верификация.
5. Баланс — через EditAnywhere-параметры прямо в PIE, не пересборкой.
6. «Второй слой» GDD (генетика, цивилизация, фонд, одежда) — запрещён до гейта v0.1.

## PROMPT 9 (rev) — Bite & Blood — ядро игры
1. `MosquitoCharacter`:
   - `DetectNearbyHumans()` — ближайший человек (дистанция до капсулы < ~80 см);
   - посадка: `bIsLanded` — у поверхности человека скорость гасится (гравитация
     остаётся 0); взлёт — любая клавиша полёта снимает посадку;
   - `PerformBite()`: сидя, ЛКМ → прогресс 0.5–1 c → `Human->OnBitten(BloodGain)`
     и `CurrentBlood += BloodGain` (лимит MaxBlood).
2. Тик статов (закрывает «Известные проблемы» №3): Hunger растёт (быстрее в
   полёте), Energy падает (полёт дороже сидения), Energy = 0 → скорость ×0.5,
   Hunger > 80 → кровь медленно тратится.
3. Лог-маркеры: `[Mosquito] Landed on Human`, `[Mosquito] Bite #n (+X)`,
   `[Mosquito] Take off`.
DoD: руками — сесть, укусить 2–3 раза (раздражение растёт, SWAT реально опасен);
headless — прогон чист (без ввода укуса не будет — достаточно отсутствия ошибок).

## PROMPT 10 (rev) — Debug HUD (C++ AHUD, ноль ассетов)
1. `MosquitoHUD : AHUD`: DrawRect-бары Health/Blood/Energy/Hunger/Wings + текст
   состояния ближайшего человека (Calm…CHASE) + часы (время из DayNightSystem).
2. `AMosquitoSimulatorPlayerController`: HUDClass в конструкторе.
3. Заложить `FlashDamage()` (красная вспышка урона) — пригодится Prompt 12.
DoD: в PIE бары живут при полёте/укусах, состояние человека видно без логов.
Ограничение: БЕЗ UMG-ассетов (сохраняем «ноль ручных шагов в редакторе»);
UMG-скин — после v0.1.

## PROMPT 11 (rev) — Chase Score (остаток старого Prompt 10)
1. Вход в Chase → таймер; выход из Chase (Irritation < 4) →
   `Score += Seconds * Multiplier`, лог `[Score] Chase survived %.1fs +%.0f`.
2. Score хранить в PlayerController (заглушка; вывод в HUD позже).
DoD: спровоцировать Chase и пережить/уйти → строка очков в логе.
(Поворот/махание/погоня/спад раздражения — УЖЕ готовы, см. Prompt 7.)

## PROMPT 12 (rev) — Wing Damage & Death/Respawn (остаток старого Prompt 12)
1. Урон уже идёт через `ApplySwatHit`. Добавить штрафы: Wings < 50 → скорость
   ×0.75, Wings < 25 → ×0.5 (пересчёт MaxFlySpeed в тике статов из Prompt 9).
2. `Die()` при Health <= 0: лог `[Mosquito] Died`, блок ввода, 3 c → респавн на
   PlayerStart, статы сброшены.
3. Вспышка урона — через `FlashDamage()` из HUD (Prompt 10).
DoD: намеренно дать себя нашлёпать → смерть и респавн; headless чист.
Без поколений — просто respawn (генетика — v0.3 по GDD).

## PROMPT 13 (rev) — Баланс + ПЛЕЙТЕСТ-ГЕЙТ v0.1
1. Прогнать полный цикл: охота → укус → раздражение → SWAT → погоня → выжил/умер.
2. Баланс (скорости, радиусы, урон, кулдауны) — EditAnywhere в PIE.
3. Сессия 10–15 мин «как игрок»: маленькость, отзывчивость полёта, камера,
   страх потеряться, читаемость состояния человека.
4. ГЕЙТ: если цикл ВЕСЁЛЫЙ на серых примитивах — MVP v0.1 готов, открываем v0.2
   (паук, сохранение). Если нет — чинить feel, а НЕ добавлять контент.

## Отложено (осознанно, после гейта v0.1)
- Рукотворный уровень (старый Prompt 6), геймпад (хвост Prompt 5),
  генетика/поколения (v0.3), паук/стрекозы/птицы (v0.2), UMG-скин HUD, звук.
