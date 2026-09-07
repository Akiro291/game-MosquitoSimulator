# HANDOFF.md — Mosquito Simulator (точка входа для нового агента)

**Движок:** UE 5.8.2 (`e:\ue_5.8`), проект: `d:\UnrealProjects\MosquitoSimulator 5.8`
**Обновлён:** 2026-09-07 (старт MVP 0.2: план зафиксирован, Phase 0 гигиена)

## Что это за игра

3D Survival/Stealth/Simulator от лица комара (≈1–2 см) в дачном посёлке. Для человека
комар — мелкая раздражающая дрянь; для комара человек — гигантский опасный босс.
Чистый C++, ноль ассетов (примитивы /Engine/BasicShapes, runtime-материалы,
процедурный звук). Масштаб: **1 uu = 1 см**.

Дизайн: `ai/DESIGN.md` (GDD v1.1), правила работы: `Agent.md`, `ai/WORKFLOW.md`.

## Статус MVP 0.1: ЗАВЕРШЁН (плейтест-гейт пройден в PIE 2026-09-07)

### IMPLEMENTED
- Полёт (`MOVE_Flying`), камера (SpringArm 18 см, FOV 90, near clip 0.5 в INI), Enhanced Input в ctor.
- Статы-тик: голод/энергия/кровь, Exhausted → скорость ×0.5, голод >80 жжёт кровь.
- Посадка (ЛКМ ≤85 см, прилип `LandedOffset` к человеку) + укус (0.6 с → кровь, раздражение +1).
- Wing Damage (штрафы ×0.7/×0.4 + тряска), Death/Respawn (2.5 с, телепорт к PlayerStart).
- 3 Human NPC: FSM Calm→Noticed→Irritated→Angry→Chase по IrritationLevel 0–4, Wander,
  PerformAttack (SWAT: урон крыльям + кик камеры, БЕЗ импульса), Chase Score (10/50/150/500).
- День/ночь (120 с сутки), `ADayNightSystem` c хуками `GetDayFactor()/IsNight()`.
- HUD: C++ Canvas (5 баров, состояние человека, часы, прогресс укуса, Score, DEAD-overlay).
- Блокаут-посёлок спавнится GameMode (10 примитивов), runtime-MID цвета, процедурный аудио
  (buzz/clap/bite, `MosquitoAudio`), GameMode ставит `HUDClass=AMosquitoHUD` в ctor.

### VERIFIED (headless + PIE)
- Headless: EXIT=0, 0 fatal/ensure, Game class/Blockout/3×Spawn/HUD-маркеры, 19× readback=OK цветов.
- PIE-гейт 2026-09-07: человек бежит в Chase, тряска безопасна, баланс принят владельцем.

### ТЕКУЩАЯ COLLISION-КОНЦЕПЦИЯ (v5b, односторонняя — не отменять!)
**Human физически полностью игнорирует комара; комар блокируется человеком И миром.**
- **Канал:** в DefaultEngine.ini добавлен object channel **"MosquitoBody"** (ECC_GameTraceChannel1) с **DefaultResponse=ECR_Block** — критично: мир (профиль BlockAll, пустые CustomResponses) берёт ответ на новый канал из DefaultResponse; с Ignore (ошибка v5) комар пролетал пол/стены/деревья.
- **Комар** (`MosquitoCharacter.cpp` ctor): object type = MosquitoBody, response на `ECC_Pawn` = **Block** → собственный swept-move движка (CMC → SafeMoveUpdatedComponent → ResolvePenetration) останавливает/скользит комара по капсуле человека штатным механизмом; односторонне — движется только комар. WorldStatic/WorldDynamic = Block (из Pawn-профиля + DefaultResponse канала).
- **Human** (`HumanCharacter.cpp` ctor): **runtime-override** response на MosquitoBody = **Ignore** → движение человека никогда не блокируется/де-пенетрируется/толкается комаром (лечит «подбрасывание»: human — object type ECC_Pawn с Block на Pawn-канал, его CMC выталкивал его из капсулы комара). Human-vs-human (ECC_Pawn) коллизии не изменены.
- **`ResolvePawnPenetration(DeltaTime)`** (Tick, только `!bIsLanded`): закрывает случай «человек прошёл над зависшим комаром» — комар выталкивается аналитической математикой capsule-vs-capsule по минимальному вектору разделения (только позиция, velocity не тронут, sweep=true от мира). В v4 направление push было инвертировано (тянуло к центру — протаскивало сквозь тело) — исправлено.
- **Диагностика (TEMP):** `[ Mosquito::HumanCollision ]` type=BLOCKED/DEPENETRATED + loc/normal/penetration/velocity, throttle 0.5 с (`LogHumanCollision`) — удалить после PIE-верификации владельцем.
- Посадка/укус: отдельный путь `StickToLandedHuman`, депенетрация скипается при `bIsLanded`.
**Возвращать двустороннюю Block-схему или Overlap-варианты запрещено.** История: Block (трамплин) → Overlap+ручная коррекция (инвертированный push + human всё ещё блокировал Pawn-канал) → **v5/v5b односторонняя (текущая)**.

### KNOWN ISSUES
- Камера: SpringArm `bDoCollisionTest=false` (может входить в стены вблизи).
- Геймпад не подключён (Prompt 5 частично).
- `GameMode.ServerTravel` заглушка (`DefaultMap=None`) — не удалить случайно.
- Шум LogPython (плагин VibeUE) — не наш код, игнорировать.
- TEMP-диагностика `[Mosquito::CameraShake]` печатается Warning'ом раз в 1 с при Wings<25 →
  понизить до Verbose при чистке логов.
- Headless `-nullrhi` может зависнуть — всегда с таймаутом + kill.

### NEXT (MVP 0.2 — ФАЗЫ 0–6 РЕАЛИЗОВАНЫ, headless-гейт пройден 2026-09-07)
План-источник: `.kilo/plans/1788765577251-mvp-0-2-plan.md` (фазы 0–6) — всё в дереве (коммиты Ph0–Ph6, headless-логи в ai/logs).
**Score конвертируется в XP** (run-уровни → level-up points на Tab-панель; без generic RPG), lifetime Score — рекорды + species-очки (наследование, `UMosquitoSimulatorGameInstance` + `Saved/Config/MosquitoSave.ini`). Паук: trapped/struggle lunge killable — в игре.
Осталось: PIE-гейт владельца (чек-лист: ai/PROGRESS.md «Следующий шаг»). Дальше — только явное «ок» (кандидаты 0.3: mating/partner, второй паук/патрули).
P2: настоящий уровень вместо блокаута.
P3: ночной режим людей (хуки `IsNight()` готовы); эскалация Chase. P4: Mosquito Sense (ПКМ-заглушка).
P5: UMG-HUD/меню; геймпад — ОТЛОЖЕНО владельцем (0.2 остаётся на Canvas HUD). P6: брюшко от крови; чистка логов.

### PLANNED / IDEAS (0.3+ — запрещено без явного «ок»)
Пауки-генерация, поколения/генетика, вода/кладка яиц, посёлок, расписания, погода, сейвы.

## Бытовые команды
- Сборка: `& 'e:\ue_5.8\Engine\Build\BatchFiles\Build.bat' MosquitoSimulatorEditor Win64 Development -Project="d:\UnrealProjects\MosquitoSimulator 5.8\MosquitoSimulator.uproject" -WaitMutex` (редактор должен быть закрыт).
- Headless QA: рецепт в `ai/PROGRESS.md`; критерии: Game class правильный, Blockout ready, 0 fatal/ensure.
- Карты: `/Game/Game/Maps/MainLevel`. Логи QA: `ai/logs/` (не удалять).
