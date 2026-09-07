# HANDOFF.md — Mosquito Simulator (точка входа для нового агента)

**Движок:** UE 5.8.2 (`e:\ue_5.8`), проект: `d:\UnrealProjects\MosquitoSimulator 5.8`
**Обновлён:** 2026-09-07 (collision hotfix v2 — positional depenetration)

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

### ТЕКУЩАЯ COLLISION-КОНЦЕПЦИЯ (не отменять!)
**Human НЕ воспринимает Mosquito как препятствие.** Фикс подбрасывания человека (v2, kinematic):
- Капсула комара: `ECC_Pawn = ECR_Overlap` (`MosquitoCharacter.cpp` ctor) → физической блокировки
  между комаром и человеком НЕТ; человек продолжает Wander/Chase, не получает impulse/Launch,
  не меняет Z. Комар НЕ переведён на APawn/AActor, физика не используется.
- `AMosquitoCharacter::ResolvePawnPenetration()` (Tick, только в полёте `!bIsLanded`): point-sweep
  капсулы комара из центра человека наружу; при проникновении корректируется ТОЛЬКО позиция
  комара (`SetActorLocation`, velocity не трогается — «дополнительного ускорения нет»).
  Лог — Verbose (`[Mosquito::PawnPenetration]`). Query только при NearestHumanDistance
  ≤ `PawnPenetrationQueryRadius` (150 см).
- Посадка/укус идут отдельным путём (`StickToLandedHuman`, комар_adjacent к поверхности) —
  коррекция в посадке выключена, Landing/Bite не мешает.
**Старую физическую Block-схему не возвращать.** История: Ignore (комар сквозь человека) →
Block (трамплин/подбрасывание) → **Overlap + ручная коррекция (текущая)**.

### KNOWN ISSUES
- Камера: SpringArm `bDoCollisionTest=false` (может входить в стены вблизи).
- Геймпад не подключён (Prompt 5 частично).
- `GameMode.ServerTravel` заглушка (`DefaultMap=None`) — не удалить случайно.
- Шум LogPython (плагин VibeUE) — не наш код, игнорировать.
- TEMP-диагностика `[Mosquito::CameraShake]` печатается Warning'ом раз в 1 с при Wings<25 →
  понизить до Verbose при чистке логов.
- Headless `-nullrhi` может зависнуть — всегда с таймаутом + kill.

### NEXT (MVP 0.2 — по решению владельца, план согласован 2026-09-07)
P0: гигиена git (MVP 0.1 не закоммичен!) → уже сделан collision hotfix v2.
P1: паук + паутина; простая прокачка за Score. P2: настоящий уровень вместо блокаута.
P3: ночной режим людей (хуки `IsNight()` готовы); эскалация Chase. P4: Mosquito Sense (ПКМ-заглушка).
P5: UMG-HUD/меню; геймпад. P6: брюшко от крови; чистка логов.

### PLANNED / IDEAS (0.3+ — запрещено без явного «ок»)
Пауки-генерация, поколения/генетика, вода/кладка яиц, посёлок, расписания, погода, сейвы.

## Бытовые команды
- Сборка: `& 'e:\ue_5.8\Engine\Build\BatchFiles\Build.bat' MosquitoSimulatorEditor Win64 Development -Project="d:\UnrealProjects\MosquitoSimulator 5.8\MosquitoSimulator.uproject" -WaitMutex` (редактор должен быть закрыт).
- Headless QA: рецепт в `ai/PROGRESS.md`; критерии: Game class правильный, Blockout ready, 0 fatal/ensure.
- Карты: `/Game/Game/Maps/MainLevel`. Логи QA: `ai/logs/` (не удалять).
