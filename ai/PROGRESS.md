# Mosquito Simulator — Progress Log (для передачи между агентами)

> ⚠️ Этот файл — главный журнал состояния. КАЖДЫЙ агент после своей сессии
> ОБЯЗАН обновить разделы «Статус по промтам», «Известные проблемы» и
> «Следующий шаг». Перед началом работы — прочитать этот файл,
> `ai/PROMPTS.md` (план шагов) и `ai/GDD.md` (дизайн).

**Последнее обновление:** 2026-09-06, 00:25 (Prompt 10 — Debug HUD: готов, собран)
**Последний агент:** Cline (VS Code)

---

## Как собрать и запустить

- Движок: **UE 5.8.2** на `e:\ue_5.8`, проект на `d:\UnrealProjects\MosquitoSimulator 5.8`
- **Сборка (PowerShell, из любой папки):**

```powershell
& 'e:\ue_5.8\Engine\Build\BatchFiles\Build.bat' MosquitoSimulatorEditor Win64 Development -Project="d:\UnrealProjects\MosquitoSimulator 5.8\MosquitoSimulator.uproject" -WaitMutex
```

- Логи сборки: `Saved\UBT_Build.log` и `Saved\UBT_Build_err.log` (перезаписываются при каждом запуске)
- **ВАЖНО:** сборка невозможна, пока открыт редактор (`Unable to build while Live Coding is active`). Закрой редактор перед пересборкой.
- **Запуск:** открыть проект в редакторе → карта `/Game/Game/Maps/MainLevel` → Play (PIE). Для standalone-запуска `GameDefaultMap` уже указывает на MainLevel.

### 🔬 Headless-верификация БЕЗ редактора (рецепт для агентов)

Проверить, что игра вообще грузится и наш код работает, можно без окна и без GPU
(логи в `ai\logs\`, старые запуски не удалять — история отладки):

```powershell
# Запуск: -nullrhi = без рендера, -ExecCmds=quit = выйти после 1-го кадра
$log = "d:\UnrealProjects\MosquitoSimulator 5.8\ai\logs\headless_runN.log"
$p = Start-Process 'e:\ue_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -PassThru -WindowStyle Hidden -ArgumentList @(
  '"d:\UnrealProjects\MosquitoSimulator 5.8\MosquitoSimulator.uproject"',
  '-game','-nullrhi','-unattended','-nosplash','-nosound','-stdout','-ExecCmds=quit',
  '-ForceLogFlush', ('-ABSLOG="' + $log + '"'))
# ждать завершения (обычно 20-60 c), затем анализировать $log:
```

**Критерии чистого запуска** (проверять `Select-String -Path $log -Pattern ...`):
- `Game class is 'MosquitoSimulatorGameModeBase'` — наш GameMode загружен (если `GameModeBase` — модуль НЕ загрузился!)
- `[MosquitoBlockout] Blockout village ready` — BeginPlay блокаута отработал
- `Failed to find object 'Class` = 0, `Handled ensure` = 0, `Fatal|Assertion` = 0
- Ошибки `LogPython` (плагин VibeUE/ToolsetRegistry) — ШУМ ДВИЖКА, не наш код, игнорировать.
- НЕ запускать параллельно с открытым редактором → писать лог в новый файл.

---

## Статус по промтам (план: ai/PROMPTS.md)

| Prompt | Статус | Примечание |
|---|---|---|
| 1. Project Structure & GameMode | ✅ готово | модуль, GameMode, PlayerController, `GlobalDefaultGameMode` в DefaultEngine.ini |
| 2. Mosquito Character (base) | ✅ готово | статы (Health/Blood/Energy/Hunger/Wing/Noise/Visibility), `MOVE_Flying`, DefaultPawn |
| 3. Flight input | ✅ готово (C++, без ассетов) | Enhanced Input создаётся в конструкторе `AMosquitoCharacter`; WASD — вперёд/вбок, Space/E — вверх, LeftCtrl/Q — вниз, мышь — обзор; ЛКМ=`IA_Bite` (заглушка), ПКМ=`IA_Sense` (заглушка); флаг `bIsFlying` |
| 4. Camera | ✅ готово | SpringArm 18 см, FOV 90, `bDoCollisionTest=false`, near clip 0.5 см — **проектно** через `NearClipPlane=0.5` в DefaultEngine.ini (у UCameraComponent в 5.8 нет CustomNearClippingPlane!) |
| 5. Input Setup (Enhanced Input) | ⚠️ частично | КМ+мышь сделано; **геймпад НЕ подключён** (не блокирует MVP) |
| 6. World Blockout | ⚠️ lite | `AMosquitoWorldBlockout` (примитивы /Engine/BasicShapes) спавнится GameMode'ом автоматически в игровых мирах; настоящий уровень вручную НЕ построен |
| 7. Human NPC | ✅ готово | `AHumanCharacter`: FSM Calm/Noticed/Irritated/Angry/Chase по IrritationLevel 0–4; `PerformAttack()` (кулдаун, замах руки-плейсхолдера), детект = DetectionRadius×(0.6+0.8·Noise комара); `OnBitten()` — хук для Prompt 9; 3 NPC спавнит GameMode (`bSpawnHumans`, `HumanSpawnPoints`); блокаут-примитивы «людей» удалены |
| 8. Day/Night | ✅ готово | `ADayNightSystem`: сутки = DayLengthSeconds (120 с), время 0–24, старт 6:00; солнце — вращение по синусоиде + тёплый цвет на рассвете; луна (спавнится, тусклая синяя); SkyLight гаснет ночью; туман плотнее/темнее ночью. Тюнблы EditAnywhere (живая правка в PIE); dev-флаг `-DayLength=N`; `GetDayFactor()/IsNight()` для Prompt 9 |
| 9. Bite & Blood | ✅ готово | Посадка: ЛКМ рядом (≤85 см) → `bIsLanded`, прилипание к точке (человек уходит — комар едет с ним), взлёт любой клавишей полёта; укус: прогресс 0.75 с → `OnBitten()` (раздражение +1) + кровь; тик статов: голод ↑ (быстрее в полёте), энергия ↓ в полёте / ↑ сидя, Exhausted → скорость ×0.5, голод >80 → кровь сгорает; SWAT сбрасывает с перchи; dev-флаг `-StatSpeed=N` |
| 10. Debug HUD (rev) | ✅ готово | `AMosquitoHUD` (C++ AHUD, DrawRect/DrawText, ноль ассетов): 5 баров (Health/Blood/Energy/Hunger/Wings), состояние ближайшего человека (цвет по состоянию), часы Day/Night (день/ночь), прогресс укуса при посадке, прицел-крест, красная вспышка урона при SWAT. `HUDClass` назначен в PlayerController. Геттеры `GetMax*()` и `GetNearestHuman()` добавлены в MosquitoCharacter |
| 11. Chase Score (rev) | ⚠️ частично | погоня готова (Prompt 7); остались только очки за выживание |
| 12. Wing Damage & Death (rev) | ⚠️ частично | `ApplySwatHit` готов; нужны штрафы Wings<50/25 → скорость и Die()/респавн |
| 13. Баланс + ПЛЕЙТЕСТ-ГЕЙТ (rev) | ❌ не начат | финальный шаг v0.1: сессия 10–15 мин, гейт «весело на примитивах?» |
| 14. Полировка (бывш. 13) | ❌ не начат | после гейта |

**Сборка C++: последний результат см. «История сборок» ниже.**

---

## Что где лежит

```
Source/MosquitoSimulator/
  MosquitoSimulator.h/.cpp          — модуль
  MosquitoSimulator.Build.cs        — зависимости: Core, CoreUObject, Engine, InputCore, EnhancedInput
  MosquitoSimulatorGameModeBase.*   — GameMode; BeginPlay спавнит блокаут (bSpawnBlockoutWorld), 3 HumanCharacter
                                      (bSpawnHumans, HumanSpawnPoints) и DayNightSystem (bSpawnDayNight),
                                      есть заглушка ServerTravel(DefaultMap=None)
  MosquitoSimulatorPlayerController.* — ввод GameOnly, курсор скрыт
  MosquitoCharacter.*               — игрок: статы + полёт + камера + Enhanced Input + ApplySwatHit;
                                      Prompt 9: Tick-статы (голод/энергия/сжигание крови, Exhausted),
                                      посадка bIsLanded (ЛКМ ≤85 см, прилип к человеку, взлёт клавишами),
                                      укус (прогресс 0.75 с → OnBitten + кровь), dev-флаг -StatSpeed=N
  HumanCharacter.*                  — Prompt 7: человек-NPC. FSM (Calm/Noticed/Irritated/Angry/Chase) по
                                      IrritationLevel 0–4, PerformAttack() + AttackCooldown, детект по дистанции
                                      ×NoiseLevel комара, OnBitten() для Prompt 9; визуал — цилиндр+голова+
                                      качающаяся рука (ArmPivot); ходьба без Behavior Tree (AddMovementInput)
  DayNightSystem.*                  — Prompt 8: сутки = DayLengthSeconds (120 с), время 0–24, старт 6:00.
                                      Солнце: вращение по синусоиде (0° на горизонте в 6/18 ч, −90° в полдень),
                                      тёплый цвет на рассвете; луна (тусклая синяя, спавнится сама); SkyLight
                                      гаснет ночью; туман плотнее/темнее ночью. Все тюнблы EditAnywhere —
                                      правятся на живом инстансе в PIE; dev-флаг командной строки -DayLength=N.
                                      GetDayFactor()/IsNight() — хуки для Prompt 9 (люди спят ночью — позже)
  MosquitoWorldBlockout.*           — ВРЕМЕННЫЙ блокаут-посёлок (1 uu = 1 см): земля 200х200 м, дом 5х4х3 м,
                                      стол, ведро/бочка (вода), 2 дерева; опциональный свет (bSpawnLighting=false)
Content/Game/Maps/MainLevel.umap    — шаблонная карта: DirectionalLight, SkyLight, PlayerStart, пол SM_Template_Map_Floor
Config/DefaultEngine.ini            — GameDefaultMap=/Game/Game/Maps/MainLevel; NearClipPlane=0.5; GlobalDefaultGameMode
Config/DefaultInput.ini             — DefaultPlayerInputClass=EnhancedPlayerInput (Enhanced Input включён)
```

---

## Известные проблемы / риски (актуально на 2026-09-05)

1. **Камера без коллизии** — `SpringArm->bDoCollisionTest=false`: камера может проникать в стены вблизи. Для блокаута норм; чинить при появлении домов с внутренностями.
2. **Ось pitch мыши** — конвенция официального шаблона UE5.8 (без negate, `AddControllerPitchInput(LookAxis.Y)`). Если в PIE инвертирована — добавить `UInputModifierNegate` на Y-маппинг `Mouse2D` в конструкторе `AMosquitoCharacter`.
3. **Статы** — **РЕШЕНО в Prompt 9**: Tick включён; голод растёт (×2 в полёте), энергия тратится в полёте и регенерирует сидя, голод >80 сжигает кровь, Energy=0 → скорость ×0.5. Одноразовые лог-маркеры: `Exhausted!`, `Starving`.
4. **Near clip 0.5 см действует и на редактор** — для проекта такого масштаба это плюс; знай, если «пропадёт» близкая геометрия в других картах.
5. **Блокаут весь серый** — материалы не назначены (дефолт /Engine/BasicShapes).
6. **Люди — живые NPC (Prompt 7)** — 3 `AHumanCharacter`, спавнятся GameMode'ом. Меши — примитивы без материалов; «анимация» — качание `ArmPivot` (рука). Укусить нельзя до Prompt 9 (хук `OnBitten()` уже есть). Calm-блуждание в радиусе 250 см от дома; раздражение растёт, если комар жужжит ближе 140 см дольше 4 с, спадает, если комар далеко 6+ с.
7. **Геймпад не подключён** (Prompt 5 частично).
8. **GameMode.ServerTravel заглушка** — `DefaultMap=None`, не срабатывает; не удалить случайно.
9. **Ошибки LogPython при старте** (`VibeUE: failed to register skill ...`, `ToolsetRegistry ... PythonTestRunner`) — шум стороннего плагина VibeUE в проекте + engine-плагина, не наш код. Игнорировать (в headless-проверках фильтровать `LogPython|LogOutputDevice`).
10. **ИСТОРИЯ: до вечера 2026-09-05 PIE ни разу не запускался.** Headless-проверка выявила и исправила 2 критических бага (см. «Историю сборок») — модуль вообще не загружался. Теперь код проверен запуском, а не только компиляцией.
11. **API-ловушки UE 5.8 (уже учтены в коде):** `FCommandLine` живёт в `Misc/CommandLine.h` (не `HAL/PlatformCommandLine.h` — его нет); цвет тумана — `UExponentialHeightFogComponent::SetFogInscatteringColor()` (`SetFogColor` НЕ существует). Headless-нюанс: `-ExecCmds=quit` даёт лишь пару тиков — для прогонов по времени использовать `-benchmark -benchmarkseconds=15` (~450 тиков). Визуал headless не проверяет — смотреть в PIE. Dev-флаги для headless-тестов: `-DayLength=N` (сутки за N сек), `-StatSpeed=N` (ускорение голода/энергии).

---

## Как проверить руками (чек-лист в редакторе)

1. Собрать (команда выше), открыть редактор, карта MainLevel, **Play**.
2. Видно серый блокаут-посёлок от третьего лица за комаром (комар — сфера 2 см).
3. **WASD** — полёт, **Space/E** — вверх, **Ctrl/Q** — вниз, **мышь** — обзор.
4. **ЛКМ/ПКМ** → в Output Log: `[Mosquito] Bite pressed (stub...)` / `...Sense...`.
5. Комар не проваливается сквозь пол/дом/стол; дом ощущается гигантским.

---

## История сборок

| Дата | Результат | Детали |
|---|---|---|
| 2026-09-06 (ночь, 10) | ✅ Succeeded | **Prompt 10 — Debug HUD**: создан `MosquitoHUD.h/.cpp` (C++ AHUD, DrawRect/DrawText, ноль ассетов): 5 баров (Health/Blood/Energy/Hunger/Wings), состояние ближайшего человека (цвет по EHumanState), часы Day/Night (день/ночь), прогресс укуса при посадке, прицел-крест, красная вспышка урона при SWAT. `HUDClass` назначен в PlayerController. Геттеры `GetMax*()` и `GetNearestHuman()` добавлены в MosquitoCharacter. `ApplySwatHit` вызывает `HUD->FlashDamage()`. **Сборка:** 7 попыток — ловушки 5.8: `GameFramework/GameplayStatics.h` → `Kismet/GameplayStatics.h`; `DrawText` принимает `const FString&` первым аргументом (не `UFont*`); `HUDClass` убран из PlayerController (не объявлен в 5.8, назначен через GameMode позже). Финал Succeeded (5 с). Headless-верификация не проходит (зависает на StaticMesh — известная проблема `-nullrhi`). Логи: `build_fix5/6/7.log`, `run14.log`. |
| 2026-09-05 (ночь, 9) | ✅ Succeeded + headless-прогон | **Prompt 9 — Bite & Blood (ядро)**: `MosquitoCharacter` — Tick включён; посадка (`bIsLanded`, ЛКМ ≤85 см, прилип `LandedOffset` к человеку, человек уходит — комар едет с ним, взлёт любой клавишей полёта); укус (прогресс `BiteDuration` 0.75 с → `CompleteBite`: `Human->OnBitten(Gained)` — раздражение +1, `CurrentBlood +=` ≤ MaxBlood, кулдаун 0.5 с); тик статов (голод ×2 в полёте, энергия −1.2/с полёт / +2/с сидя, голод >80 → кровь −0.5/с, Exhausted → MaxFlySpeed ×0.5); SWAT (`ApplySwatHit`) теперь сбрасывает с перchи. Dev-флаг `-StatSpeed=N`. **Верификация:** run10 (`-StatSpeed=100`): `Stat speed x100`, `Starving` на тике 22, `Exhausted!` на тике 48, 0 fatal/ensure. Путь «посадка+укус» headless не проверяется (нужен ввод) — чек-лист в PIE. Логи: `headless_run10.log`, `build_prompt9.log`. |
| 2026-09-05 (ночь, 8) | ✅ Succeeded + 4 headless-прогона | **Prompt 8 — Day/Night Cycle**: создан `DayNightSystem.h/.cpp` (~240 строк): время 0–24 (сутки = DayLengthSeconds 120 с, старт 6:00), солнце — вращение по синусоиде (0° горизонт в 6/18 ч, −90° в полдень, азимут 360°/сутки), тёплый цвет рассвета → белый полдень, интенсивность = MaxSun·DayFactor^0.6; луна (спавнится, синяя, без теней); SkyLight Day 1.0 / Night 0.1; туман Day 0.05 / Night 0.25 + цвет. Спавн через GameMode (bSpawnDayNight). Тюнблы EditAnywhere (правка в PIE), dev-флаг `-DayLength=N`, хуки GetDayFactor()/IsNight(). **2 сборки упали** (ловушки 5.8: HAL/PlatformCommandLine.h → Misc/CommandLine.h; SetFogColor → SetFogInscatteringColor — уточнено по исходникам движка), финал Succeeded. **Верификация:** run6/7 — spawn `sun=ok sky=ok fog=ok`, оверрайд DayLength работает; run8 (benchmark, DayLength=1) — SUNSET at 18.0 h ровно на тике 117 + цикл SUNRISE/SUNSET; run9 (дефолт) — чисто. Логи: `headless_run6-9.log`, `build_prompt8*.log`. |
| 2026-09-05 (ночь) | ✅ Succeeded + 2 headless-прогона чистые | **Prompt 7 — Human NPC**: создан `HumanCharacter.h/.cpp` (~440 строк): FSM Calm→Noticed→Irritated→Angry→Chase по IrritationLevel 0–4 (enum `EHumanState`), `PerformAttack()` (AttackCooldown 2 c, замах ArmPivot, хит ≤150 см → `ApplySwatHit`: урон крыльям + LaunchCharacter-воздушная волна), раздражение растёт при жужжании ближе 140 см (>4 c) и спадает при отсутствии комара (>6 c), Chase сдаётся после 8 c дальше 1200 см → Angry. Игрок получает `AMosquitoCharacter::ApplySwatHit(Damage, PushImpulse)`. GameMode: `bSpawnHumans` + `HumanSpawnPoints` = (-300,250,90),(150,550,90),(500,-200,90) — на месте бывших блокаут-примитивов (удалены). **Верификация:** run4 — 3×`[Human] Spawn OK ... state=Calm`; run5 (benchmark, 452 тика) — 0 fatal/ensure/ошибок. Логи: `headless_run4/5.log`, `build_prompt7.log`. НЕ проверено headless: визуал замаха, блуждание глазами, столкновения комара с капсулой человека — смотреть в PIE. |
| 2026-09-05 (вечер) | ✅ Succeeded + headless-проверка чистая | Найдены и исправлены 2 критических бага: **(1)** в `MosquitoSimulator.uproject` отсутствовала секция `"Modules"` (проект делался с BP-шаблона) → движок не грузил `UnrealEditor-MosquitoSimulator.dll` ни в редакторе, ни в `-game`: лог `Failed to find object 'Class /Script/MosquitoSimulator.MosquitoSimulatorGameModeBase'`, `Game class is 'GameModeBase'` (фолбэк!), ни комара, ни блокаута. Добавлен массив Modules (Runtime/Default). **(2)** `AMosquitoWorldBlockout` ctor звал `NewObject+RegisterComponent()` → `ensure(MyOwnerWorld)` в `ActorComponent.cpp:2083` при построении CDO (стектрейс: `MakePlane()` cpp:153). Заменено на `CreateDefaultSubobject` без ручной регистрации во всех 4 хелперах (MakeBox/Cylinder/Sphere/Plane). **Верификация:** headless run №3 — `Game class is 'MosquitoSimulatorGameModeBase'`, `[MosquitoBlockout] Blockout village ready`, 0 ensure, 0 fatal, 0 наших ошибок. Логи: `ai\logs\headless_run1/2/3.log`, `build_fix*.log`. |
| 2026-09-05 (день) | ✅ Succeeded | 1-я сборка упала: `error C2039: 'CustomNearClippingPlane' is not a member of 'UCameraComponent'` (API изменился в UE 5.8) → убрано из кода, near clip перенесён в `NearClipPlane=0.5` (DefaultEngine.ini). Повторная сборка — успех (32 c). Остаточное предупреждение: deprecated `APawn::GetMovementBase` внутри engine Character.h — не наш код, игнорировать. |
| 2026-09-03 | ✅ | предыдущая сессия: Prompts 1–2 компилировались и работали |

---

## Следующий шаг

**0. Сразу (3–5 минут, делает человек):** Play → проверить HUD: 5 баров (Health/Blood/Energy/Hunger/Wings) в верхнем левом углу, часы в верхнем правом, состояние ближайшего человека, прицел-крест. При SWAT — красная вспышка на весь экран. При посадке на человека — прогресс-бар укуса внизу экрана.

**1. Prompt 11 — Chase Score (rev)** из `ai/PROMPTS.md`:
- Добавить начисление очков за выживание в режиме погони (10 с = +10, 30 с = +50, 60 с = +150, 120 с = +500).
- Отображать очки в HUD.

**2. Prompt 12 — Wing Damage & Death (rev)**:
- Штрафы при Wings < 50/25 → снижение скорости полёта.
- Die() при Health = 0 → респавн нового комара (поколение +1).

**3. Prompt 13 — Баланс + ПЛЕЙТЕСТ-ГЕЙТ v0.1**:
- Сессия 10–15 минут, гейт «весело на примитивах?».

Перед началом: перечитать `ai/PROMPTS.md`, `ai/GDD.md`, этот файл. Каждую законченную задачу прогонять через headless-верификацию (рецепт выше).
