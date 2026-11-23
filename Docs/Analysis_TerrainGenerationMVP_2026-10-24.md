# ChillFarmGame — аналіз гілки `TerrainGenerationMVP`

Дата: 2026-10-24. Основа: брифінг `ChillFarmGame_Analysis_Brief.md`.

**Що зроблено:** прочитано весь C++ обох модулів (~3.4k рядків), `DefaultEngine.ini`, і там, де висновок залежить від поведінки рушія, звірено з сорсами UE 5.7 (`W:\Binary Unreal Engine\UE_5.7`). Нічого **не запускалось і не збиралось**: «Confirmed in code» означає «видно з коду (і коду рушія)», а не «відтворено».

**Головне одним абзацом.** Брифінг частково застарів: пункти 6.1, 6.2, 6.6 (основна частина), 6.7 і 6.8 у поточному коді вже виправлені. Натомість є нові серйозні речі: (1) **Game-таргет не збирається** (два незалежні build breakers, і він жодного разу не збирався — в `Intermediate/Build/Win64` є лише editor-таргети); (2) **`GameMode::BeginPlay` повністю перегенеровує світ**, тож усе, що згенеровано в редакторі й збережено в мапі, на старті гри викидається; (3) регенерація створює HISMC з тими самими іменами, і UE **перебудовує старий об'єкт на тій самій адресі**, тому застарілий `InstanceToTile` дає неправильні координати; (4) у вибору біомів не один баг, а два, і разом вони означають, що майже завжди обирається *другий за якістю* біом; (5) кілька гарантованих null-deref-ів у шляху генерації, який тепер виконується і в грі.

---

## Зміст

1. [Верифікація розділу 6 брифінгу](#1-верифікація-розділу-6-брифінгу)
2. [Нові дефекти](#2-нові-дефекти)
3. [Відповіді на питання розділу 8](#3-відповіді-на-питання-розділу-8)
4. [Оцінка дизайн-думок із розділу 7](#4-оцінка-дизайн-думок-із-розділу-7)
5. [Пріоритети](#5-пріоритети)

---

## 1. Верифікація розділу 6 брифінгу

Коротка таблиця, далі деталі лише для пунктів, які підтвердились або змінились.

| # | Пункт брифінгу | Статус | Коротко |
|---|---|---|---|
| 6.1 | `Apply` пише не ті поля | **Refuted** (виправлено) | Всі 7 гілок пишуть правильні поля. Лишились дрібниці (див. 1.1). |
| 6.2 | `RecalculateNeighbors` бере збережену маску | **Refuted** (виправлено) | Береться `GetNeighborMask(X, Y)`, і виклик уже *після* пост-процесора. |
| 6.3 | Corner/InnerCorner недосяжні | **Confirmed**, гірше, ніж описано | InnerCorner теж недосяжний. |
| 6.4 | Математика вибору біома | **Confirmed**, уточнено | `DistanceToRange` не відрізняє «всередині» від «зовні»; 0/0 не крашить, а дає alpha = 1. |
| 6.5 | `ApplyTileSmoothing` нічого не робить | **Confirmed** | |
| 6.6 | Registry не оновлюється | **Refuted** (виправлено), але | Off-by-one в лічильнику; `Initialize` ніхто не викликає, тож це мертвий код. |
| 6.7 | `GetHistory` без return | **Refuted** (виправлено) | Повертається `static const` порожній масив. |
| 6.8 | `Editor.h` у runtime | **Refuted**, але | Є **два інших** build breakers для Game-таргета (D-01). |
| 6.9 | Навігація | **Changed** | `RuntimeGeneration=Dynamic` уже стоїть. Справжній новий фактор: регенерація на `BeginPlay` (D-05) + `ShortPressThreshold` (D-17). |
| 6.10 | `InstanceToTile` lifetime | **Confirmed**, гірше | Ще й гарантовано неправильні координати після регенерації (D-03). |
| minor | Невикористані параметри шуму | **Confirmed** | + багато мертвого коду (D-22). |
| minor | `TileTypeVisuals[...]` assert | **Confirmed** | Входить у D-02. |
| minor | `HeightPlusSecondary` | **Confirmed** | Систематичний зсув вниз на 0.075–0.1. |
| minor | `Coordinates` не задаються | **Confirmed** | Усі тайли мають (0,0). |
| minor | Детекція змін за прапорцем | **Confirmed** | |
| obs. | Клік «протікає» з Inspector | **Confirmed**, знайдено механізм | Кешована позиція курсора в `FSceneViewport` (D-10). |
| obs. | `GWorld` vs editor world, extent без scale, unbind | **Confirmed** / **Confirmed** (зараз не проявляється) / **Refuted** (unbind є) | Але є ризик з `FSlateApplication::Get()` (D-11). |

### 1.1 (6.1) `FTileMutationHelper` — залишкові проблеми

- **Severity:** Low · **Status:** Confirmed in code
- **Location:** `TileMutationHistory/TileMutationHelper.h:13-25, 37-39`
- **Evidence:**
  - `UPROPERTY()` всередині `#define` UHT не бачить: у `TileMutationHelper.gen.cpp` **0** `NewProp_`. USTRUCT не має жодної рефлектованої властивості. Зараз це нешкідливо (хелпер живе на стеку), але створює хибне враження, що, наприклад, `NewBiomeTypeData` видно GC чи серіалізації.
  - `Moisture`/`Fertility` у хелпері мають тип `int32`, у тайлі `uint8`. `Apply` неявно звужує без clamp, тож `SetFertility(300)` мовчки дасть 44.
  - `uint8 NewNeighborMask = INDEX_NONE` → 255. Нешкідливо, бо без прапорця значення не читається.
- **Fix:** типи в хелпері ті самі, що в тайлі, або явний clamp у сеттері. Прибрати `UPROPERTY()` з макроса або прибрати `USTRUCT`, бо рефлексія тут нічого не дає.
- **Alternatives:** якщо хелпер колись стане payload'ом для сейва чи мережі, макрос не підійде. Тоді краще `TOptional<T>` на поле або bitmask змінених полів + значення (див. 3.1).

### 1.2 (6.3) `FTileNeighborResolver` — Corner і InnerCorner недосяжні

- **Severity:** High · **Status:** Confirmed in code
- **Location:** `TileNeighborResolver.cpp:17-36`
- **Evidence:** після перевірок «всі 4 відсутні → Isolated» і «всі 4 є → Center» будь-яка комбінація має хоча б одну відсутню сторону, і її ловить один із `if (!N)…if (!W) return Edge`. Тому:
  - **Corner** недосяжний (рядки 28-31).
  - **InnerCorner** (рядок 33) потребує `N && E`, а якщо при цьому відсутні S чи W, раніше спрацьовує Edge; якщо всі 4 є, раніше спрацьовує Center. Отже теж недосяжний.
  - «Півострів» (3 сторони відсутні) і «коридор» (N+S або E+W) теж стають Edge з поворотом за першою відсутньою стороною. Візуально це неправильно.
  - InnerCorner перевіряє лише діагональ NE, інші три не враховано.
- **Fix:** спочатку порахувати кількість відсутніх кардинальних сторін і розгалузитися: 0 → перевірка діагоналей на InnerCorner (усі 4 діагоналі, кожна дає свій поворот); 1 → Edge; 2 суміжні → Corner; 2 протилежні → окремий тип (Corridor/Strip) або явний fallback; 3 → End/Peninsula; 4 → Isolated. Ще простіше — таблиця на 256 записів (mask → key), згенерована один раз; її легко покрити тестом «кожна маска дає валідний ключ».
- **Alternatives:** dual-grid прибирає цей клас проблем (див. розділ 4).

### 1.3 (6.4) Вибір біома — два незалежні баги, які підсилюють один одного

- **Severity:** High · **Status:** Confirmed in code; реальний ефект залежить від значень на розміщеному актері та в `DA_Biomes_*` → **Needs runtime verification**
- **Location:** `BiomeTypeDataSet.h:29-35` (`DistanceToRange`), `:74-78` (`BlendAlpha`); `TileWorldGenerator.cpp:200-219`
- **Evidence:**
  1. `DistanceToRange = min(|v - lo|, |hi - v|)` дає однакову форму результату і для значення всередині діапазону, і для значення поза ним. Приклад: `Height = 0.5`, біом A `[0, 1]` → 0.5; біом B `[0.9, 0.95]` → 0.4. **B виграє, хоча значення в його діапазон не потрапляє.** Отже, «центр діапазону гірший за край» — лише частина проблеми: широкі діапазони систематично програють вузьким.
  2. `BlendAlpha = Second / (Best + Second)` ≥ 0.5 завжди (бо Second ≥ Best). `GetSecondaryNoise` повертає `[0, SecondaryNoiseStrength]`, за замовчуванням `[0, 0.25]`. Умова `BlendNoise < BlendAlpha` тоді **завжди істинна**, тож за дефолтних значень завжди обирається **другий за рахунком** біом, якщо він існує.
  3. 0/0 (обидва рахунки 0) дає NaN; `FMath::Clamp(NaN, 0, 1)` повертає 1 (обидва порівняння false), тож краша немає, обирається Secondary.
  4. Secondary-шум не має сід-офсету: однаковий для всіх сідів. І він рахується двічі (`GenerateTileNoise:195` і `GenerateBiomeData:207`).
  5. **Новий нюанс:** water-біом бере участь у `ResolveBiomeBlend` для *суші*. Тайл над `WaterLevel` може отримати water-біом (як Primary, а з багом №2 і як Secondary) і через його rule set стати `Water`. Це вода вище рівня води, а пост-процесор потім робить навколо неї пісок. Перевірити: порахувати тайли з `Height > WaterLevel` і `BiomeType == Water`.
- **Fix:**
  - Рахунок біома: 0 всередині діапазону, відстань до найближчого краю зовні. Нормалізувати на ширину діапазону, якщо ширини дуже різні.
  - Alpha має прямувати до 0, коли кращий біом явно кращий, і до 0.5 на межі, наприклад `Best / (Best + Second)`. Порівнювати з шумом у `[0, 1]`, а не `[0, Strength]`.
  - Окремий сідований шум для блендингу. Strength — окремий множник ширини перехідної зони, а не діапазону шуму.
  - Water-біом виключити з вибору для суші (або фільтрувати біоми за `bAllowWater`/висотою).
- **Alternatives:** див. розділ 4 (Whittaker-lookup). Мій погляд: прямий lookup простіший і краще читається дизайнером, але виправлена range-модель теж працює. Різниця вирішальна лише тоді, коли біомів стане багато.

### 1.4 (6.5) `ApplyTileSmoothing` — no-op

- **Severity:** Medium · **Status:** Confirmed in code
- **Location:** `TileWorldPostProcessor.cpp:145`
- **Evidence:** умова вимагає `Tile->GetSmoothingOverride() != ETileType_MAX`, а `SetSmoothingOverride` ніде не викликається (grep). Водночас `UBiomeTypeData::ResolveSmoothingOverride` / `DefaultSmoothingOverride`, які, судячи з усього, і мали давати це значення, теж ніде не використовуються.
- **Fix:** у самому проході брати override з біома (`Tile->GetBiomeData()->ResolveSmoothingOverride(Tile->GetType())`). Зберігати його в тайлі не потрібно: це конфігурація біома, а не стан тайла. Поле `SmoothingOverride` з `FTileData` можна прибрати.
- **Додатково:** `HasNeighborOfType` (shore pass) перевіряє 4-сусідів, а маска автотайлу 8-сусідів, тож діагональний контакт із водою не стає берегом. Це питання узгодженості, а не баг; вирішити свідомо.

### 1.5 (6.6) Snapshot registry

- **Severity:** Low · **Status:** Refuted (основне виправлено); нові дрібниці Confirmed in code
- **Location:** `TileDebugSnapshot.h:15`, `TileDebugSnapshotsRegistry.cpp:7-15`
- **Evidence:** `auto& Snapshot = …FindOrAdd(InHistoryEntry->Coordinates)`, координати у записі заповнюються, тож оригінальна проблема зникла. Але:
  - `uint32 MutationCount = INDEX_NONE` → перший `++` дає 0. Лічильник завжди на 1 менший.
  - `UTileDebugSnapshotsRegistry::Initialize` ніде не викликається, тобто registry ніде не створюється. Це мертвий код.
  - `check(MutationHistorySubsystem)` спрацює для будь-якого світу, де сабсистеми немає.
- **Fix:** `MutationCount = 0`; або підключити registry, або видалити до моменту, коли він знадобиться. Замість `check` використати м'який early-out.

### 1.6 (6.9) Навігація — причина інша

- **Severity:** High · **Status:** Needs runtime verification (гіпотези впорядковано за ймовірністю)
- **Evidence:**
  - `FinalizeNavigation` справді ніде не викликається. Підтверджено, але це не головне.
  - `Config/DefaultEngine.ini:149` → `RuntimeGeneration=Dynamic` **уже стоїть**. Пункт брифінгу «треба виставити Dynamic» неактуальний. Але інстанс `RecastNavMesh-Default` у мапі може мати збережене значення, яке перекриває конфіг; це варто перевірити в Details.
  - **Новий фактор:** `ChillFarmGameGameMode.cpp:37-41` викликає `GenerateWorld()` на `BeginPlay`. Усі HISMC знищуються і створюються заново в момент старту гри, тож навмеш, побудований у редакторі, стає недійсним і перебудовується асинхронно (тайли по 1000uu, 128×128×100uu ≈ 13×13 = 169 тайлів). Гравець спавниться одразу (`RestartPlayer` синхронно в тому самому кадрі), і перші кліки можуть припадати на ще не перебудований навмеш.
  - `ShortPressThreshold` не ініціалізується в C++ (D-17). Якщо в BP він теж 0, `SimpleMoveToLocation` не викличеться ніколи, і буде лише лог «No MoveTo activated…». Цей лог уже є в коді, що натякає, що симптом траплявся.
  - Тайли розміщені в `(X*TileSize, Y*TileSize)` у world space, без урахування трансформу актора (D-08). `NavMeshBoundsVolume` має покривати 0..12800 по X/Y.
- **Як перевірити (порядок):** (1) у PIE натиснути `P`/`show Navigation`, щоб побачити, чи є навмеш узагалі і коли він з'являється; (2) лог «No MoveTo activated» при короткому кліку означає D-17; (3) Visual Logger на контролері: `SimpleMoveTo` → PathFollowing result; (4) тимчасово вимкнути регенерацію в `BeginPlay` і подивитись, чи MoveTo працює на навмеші, збудованому в редакторі.
- **Fix:** залежить від рішення D-05. Якщо світ генерується в рантаймі, після генерації потрібна явна точка «навігація готова» (`UNavigationSystemV1::OnNavigationGenerationFinishedDelegate`, або перевірка `IsNavigationBuildInProgress()`), і спавн гравця / дозвіл на input має чекати на неї, а не на `OnTerrainGenerated`.

### 1.7 (6.10) `InstanceToTile` — підтверджено, і гірше

Див. **D-03**.

### 1.8 Minor — `HeightPlusSecondary`

- **Severity:** Medium · **Status:** Confirmed in code
- **Location:** `TilesGenerationRuleSet.cpp:47-48`
- **Evidence:** `(Secondary - 0.5) * 0.2` при `Secondary ∈ [0, 0.25]` дає зсув у `[-0.1, -0.05]`. Висота завжди трохи занижена, і правило ніколи не «підвищує» тайл. Ширше: усі правила з `NoiseSource = Secondary` мають діапазони `0..1` (ClampMin/Max на `FTileGenerationRule`), а реальний шум `0..0.25`, тож усе, що вище 0.25, ніколи не спрацює.
- **Fix:** нормалізувати всі джерела шуму до `[0, 1]` на виході `GenerateTileNoise`; амплітуду впливу задавати в правилі, а не в діапазоні шуму.

### 1.9 Minor — `FTileData::Coordinates` не задаються

- **Severity:** Low · **Status:** Confirmed in code
- **Location:** `TileData.h:27`, `TileDataGrid.cpp:9-16`
- **Evidence:** сеттера в хелпері немає, `Initialize` робить лише `SetNum`. Усі тайли мають `(0,0)`. Зараз ніхто це поле не читає, тому наслідків немає.
- **Fix:** або заповнювати в `Initialize` (індекс → X,Y), або прибрати поле. Координати однозначно виводяться з індексу, а дублювання в кожному тайлі коштує 8 байт × N.

### 1.10 Minor — детекція змін за прапорцем

- **Severity:** Medium (для історії й сейву, не для коректності) · **Status:** Confirmed in code
- **Evidence:** `RecalculateNeighbors` щоразу пише в історію 16 384 записи, навіть якщо маска не змінилась.
- **Fix:** у `MutateTile` порівнювати Before/After після `Apply` і не записувати історію, якщо нічого не змінилось. Для `FTileData` потрібен `operator==`.

---

## 2. Нові дефекти

### D-01 — Game-таргет не збирається (два незалежні breakers)

- **Severity:** Critical · **Status:** Confirmed in code (+ сорси рушія). Збірку не запускав.
- **Location:**
  - `TilesGenerationRuleSet.h:19`, `TilesVisualDataSet.h:19` (+ відповідні `.cpp`)
  - `Persistent/ChillFarmGameGameMode.cpp:3`
- **Evidence:**
  - `UObject::IsDataValid` у UE 5.7 оголошено всередині `#if WITH_EDITOR` (`Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h:1090-1112`). Override без такого ж guard'а в Game/Shipping дасть помилку «method with override specifier did not override any base class methods».
  - `#include "BlendSpaceAnalysis.h"` лежить у модулі **Persona** (`Engine/Source/Editor/Persona/Public/`). В editor-збірці include path «протікає» через `Engine.Build.cs` (`if (Target.bCompileAgainstEditor) PublicIncludePathModuleNames.Add("AnimationBlueprintEditor")`), у Game-таргеті файл не знайдеться. Include ще й не використовується.
  - `Intermediate/Build/Win64/` містить лише `UnrealEditor` / `ChillFarmGameEditor`. Game-таргет, судячи з усього, жодного разу не збирався, тому це й не помітили.
- **Fix:** обгорнути оголошення та визначення `IsDataValid` у `#if WITH_EDITOR`. Видалити зайві include'и в GameMode (`BlendSpaceAnalysis.h`, `Components/WidgetComponent.h`, `Blueprint/AIBlueprintHelperLibrary.h` не використовуються). Додати в робочий цикл регулярну збірку `ChillFarmGame Development` (Game), бо це найдешевший захист від повторення.

### D-02 — Гарантовані null-deref у шляху генерації (тепер і в грі)

- **Severity:** Critical (бо `GenerateWorld` викликається з `BeginPlay`) · **Status:** Confirmed in code
- **Location:** `TileWorldGenerator.cpp`:
  - `:41-45` → `TileGrid->Initialize` без перевірки, якщо в рівні немає `ATileVisualGrid`
  - `:176`, `:202` → `BiomeDataSet->` без перевірки
  - `:124` перевіряє `FinalBiome` на null, але `:128` → `FinalBiome->BaseFertility` без перевірки
  - `:125` → `RulesSet->` (null, якщо і в біома, і `DefaultTileGenerationRuleSet` порожньо)
  - `TileVisualGrid.cpp:32-33, 68-69` → `TilesVisualSet->`, `TileTypeVisuals[...]` (assert на відсутній ключ), `TileData->`, `Variant->Mesh` (null, якщо для позиції немає варіанту, наприклад якщо колись запрацюють Corner/InnerCorner, а варіантів для них нема)
- **Fix:** одна валідація на вході `GenerateWorld` (є grid, є dataset, є хоча б один land-біом і water-біом, є дефолтний rule set, є visual set) з `UE_LOG(Error)` + ранній вихід. У `CreateChunk` використовувати `Find` замість `operator[]` і fallback-меш (або skip з логом). Узагалі краще тримати конфіг генерації в одному `UPrimaryDataAsset` (див. 3.3) і валідувати його через `IsDataValid`.

### D-03 — `InstanceToTile` дає неправильні координати після будь-якої регенерації

- **Severity:** High · **Status:** Confirmed in code + сорси рушія; прояв **Needs runtime verification** (очікую відтворення з першої спроби)
- **Location:** `TileVisualGrid.h:57`, `TileVisualGrid.cpp:38-50, 77-80, 87-97, 157-164`
- **Evidence:**
  1. `ClearAllChunks` не чистить `InstanceToTile`.
  2. Імена HISMC детерміновані: `Chunk_X%d_Y%d_<Type>_<Pos>_Rot%d`. Після `DestroyComponent` старий об'єкт живе до GC (у редакторі зазвичай кілька десятків секунд).
  3. `NewObject` з тим самим `Outer`+`Name`+класом **перебудовує існуючий об'єкт на тій самій адресі** (`UObjectGlobals.cpp`, `StaticAllocateObject`: *«Replace an existing object without affecting the original's address or index»*).
  4. Отже `InstanceToTile.FindOrAdd(FoundHISMC)` знаходить **старий** масив координат, і `Append` дописує нові координати в кінець. Instance `i` нового компонента мапиться на `old[i]`. З іншим сидом чи налаштуваннями координати неправильні, з тим самим сидом випадково правильні.
  5. Плюс оригінальні проблеми: map не UPROPERTY, не серіалізується, після перевідкриття мапи порожня → `(0,0)`. `(0,0)` як значення «не знайдено» не відрізнити від валідного тайла. Ключі — сирі вказівники, які GC не обнуляє (deref немає, тому краша немає, лише застарілі записи).
  6. Побічно: та сама перебудова на місці відбувається і з компонентами в PIE, бо `BeginPlay` регенерує світ.
- **Fix (мінімальний):** чистити `InstanceToTile` в `ClearAllChunks`; не давати компонентам фіксованих імен (`NAME_None` або `MakeUniqueObjectName`); повертати `TOptional<FIntPoint>` / `bool` + out-параметр.
- **Fix (правильний):** прибрати мапу взагалі. Варіанти:
  - **(a) World position → grid coordinate** (рекомендую). Координата виводиться з точки удару (або з позиції інстанса) через ту саму функцію, якою генератор розміщує тайли. Нуль стану, нічого не серіалізується, рендерер можна замінити. Потребує спершу виправити D-08.
  - **(b) Per-instance custom data** (`NumCustomDataFloats = 2`, X і Y). Живе разом з інстансом, серіалізується з HISMC. Але прив'язує інспектор до ISM-рендерингу і потребує float → int.
  - **(c) USTRUCT-обгортка** для `TArray<FIntPoint>` + UPROPERTY. Найменша зміна, але зберігає зв'язок «індекс інстанса ↔ тайл», який ламається при будь-якому видаленні інстанса (наприклад, при runtime-зміні тайла через `RemoveInstance`, бо ISM переставляє останній на місце видаленого).

### D-04 — Історія не очищується між генераціями

- **Severity:** High (дані інспектора неправильні) · **Status:** Confirmed in code
- **Location:** `TileMutationHistorySubsystem.cpp:41`; `TileWorldGenerator.cpp:29-77`
- **Evidence:** `TileHistory` очищується лише в `Initialize`/`Deinitialize`. Кожен клік Generate дописує ~3 записи на тайл (generation + neighbors + shore/smoothing), і історія попередніх генерацій змішується з новою. На 128² це ~50k `TSharedPtr`-алокацій за клік, на 1024² вже ~3M і сотні МБ.
- **Fix:** на початку `GenerateWorld` скидати історію (подія «generation started» від гриду, а сабсистема реагує сама; генератор про історію знати не повинен). У записах історії потрібен ще `GenerationId`/порядковий номер, щоб відрізняти прогони.

### D-05 — Подвійне джерело правди: editor-генерація серіалізується, а гра її викидає

- **Severity:** High (архітектурне рішення, яке блокує інші) · **Status:** Confirmed in code; вплив на розмір мапи **Needs runtime verification**
- **Location:** `ChillFarmGameGameMode.cpp:33-42`; `TileWorldGenerator.h:126-133`; `TileDataGrid.h:53-63`; `TileVisualGrid.h:54-55`
- **Evidence:**
  - `DataGrid` — default subobject з `UPROPERTY() TArray<FTileData> Tiles`, тож 16 384 тайли з hard-ref'ами на біоми зберігаються в `.umap`.
  - `Chunks` (UPROPERTY) тримає посилання на runtime-створені HISMC, які, найімовірніше, зберігаються в мапі разом з усіма інстансами. Це пояснює, чому `GenerationTest.umap` змінюється після кожної генерації.
  - Потім `BeginPlay` усе це знищує і генерує заново з того самого сиду.
  - Історія (editor world) і реальний світ гри (PIE world) — різні світи, тож інспектор у PIE нічого не покаже.
- **Fix:** вирішити, хто є правдою. Для цілі «процедурний світ + гравець обирає ділянку + сейв = сид + дельти» правда — **сид + конфіг + версія генератора**, а все згенероване має бути `Transient` (`UPROPERTY(Transient)` на `Tiles` і `Chunks`, HISMC з `RF_Transient`). Тоді editor-генерація — це лише прев'ю, мапа не роздувається і не змінюється в git на кожен клік. Якщо колись знадобиться «запечений» світ (наприклад, для швидкого старту), робити це окремим явним кроком.

### D-06 — `RecordHistory` крашить поза світом

- **Severity:** High (блокує тести) · **Status:** Confirmed in code + сорси рушія
- **Location:** `TileDataGrid.cpp:136`
- **Evidence:** `GetWorld()->GetSubsystem<…>()` без null-перевірки. `UObject::GetWorld()` (`Obj.cpp:1145`) іде по ланцюжку Outer і повертає `nullptr`, якщо світу немає (наприклад, `NewObject<UTileDataGrid>()` в автотесті, Outer = transient package). Перший же `MutateTile` поза актором крашить.
- **Fix:** прибрати залежність гриду від світу (див. 3.2): грид кидає подію, а хто хоче, той підписується. Як мінімум `if (UWorld* World = GetWorld())`.

### D-07 — Inspector: deref `TWeakObjectPtr` без перевірки

- **Severity:** High · **Status:** Confirmed in code
- **Location:** `Widgets/STileInspectorWindow.cpp:171` (`Item->Instigator->GetName()`)
- **Evidence:** Instigator — `TWeakObjectPtr`. Після знищення інстигатора (наприклад, пересоздання `PostProcessor`, перезавантаження рівня, коли історія ще жива, або майбутні мутації з `Instigator = nullptr`) це null deref.
- **Fix:** `GetNameSafe(Item->Instigator.Get())`.

### D-08 — Конвенція координат неузгоджена (центр vs кут, трансформи акторів ігноруються)

- **Severity:** Medium (стане High, коли інспектор і геймплей перейдуть на world→grid) · **Status:** Confirmed in code; фактичний зсув залежить від півоту меша → **Needs runtime verification**
- **Location:** `TileWorldGenerator.cpp:152-156`; `TileDataGrid.cpp:104-119`; `TileVisualGrid.cpp:55-60`
- **Evidence:**
  - Візуал ставить інстанс у `(X - ChunkX) * TileSize`, тобто в кут клітинки.
  - `UTileDataGrid::TileToWorld` повертає центр (`+ 0.5 * TileSize`), а `WorldToTile` робить floor.
  - Якщо півот меша по центру (стандартний plane), тайл `(x, y)` візуально займає `[x*100 - 50, x*100 + 50]`, а `WorldToTile` вважає, що `[x*100, x*100 + 100]`. Половину кожного тайла буде віднесено до сусіда.
  - `HISMC->SetWorldLocation(ChunkOrigin)` задає world space і ігнорує положення `ATileVisualGrid`; позиція генератора теж ніде не враховується. `WorldToTile` не знає origin гриду.
- **Fix:** одна функція «grid → world» з явно заданим origin (трансформ актора гриду) і явною конвенцією (центр). Нею користуються і візуал, і `WorldToTile`, і майбутній інспектор. Півот тайлових мешів задокументувати (або компенсувати в `FTileTypeVisualVariant` offset'ом). HISMC ставити через `SetRelativeLocation`.

### D-09 — Highlight subsystem: null-deref і дублікати акторів

- **Severity:** High (краш) / Medium (дублікати) · **Status:** null-deref Confirmed in code; дублікати **Needs runtime verification**
- **Location:** `TileDebugRender/TileDebugHighlightSubsystem.cpp:8-40`
- **Evidence:**
  - `RenderComponent` створюється лише в `HandleOnMapOpened`. `FEditorDelegates::OnMapOpened` не стріляє для File → New Level та деяких інших шляхів створення світу. `UpdateTiles`/`SetSelectedTile` розіменовують `RenderComponent` без перевірки, тож виходить краш при першому кліку по тайлу в такому світі. Зараз від цього частково рятує те, що селекшн-сабсистема теж тримає `TileVisualGrid` лише з `OnMapOpened`.
  - `ShouldCreateSubsystem` → `IsEditorWorld()`, і в UE 5.7 це `Editor || EditorPreview || PIE` (`World.cpp:9182`). Сабсистема створюється і для preview-світів асет-редакторів (Static Mesh Editor тощо). **Кожна** з них підписується на `OnMapOpened`, ніколи не відписується (немає `Deinitialize`), і кожна спавнить `DummyActor` у `GEditor->GetEditorWorldContext().World()`, а не у свій світ.
  - `SetFlags(RF_Transient)` ставиться *після* спавну; правильно передавати `ObjectFlags` через `FActorSpawnParameters`. Актор видно в Outliner, і його можна виділити або видалити.
- **Fix:** обмежити `ShouldCreateSubsystem` до `WorldType == Editor`. Створювати компонент ліниво при першому запиті (або в `OnWorldBeginPlay` / `PostInitialize`), спавнити в `GetWorld()` сабсистеми. Відписуватись у `Deinitialize`.
- **Alternatives:** див. D-10. Якщо інструмент перейде на `UEdMode`, рендер підсвітки робиться в `Render(const FSceneView*, FViewport*, FPrimitiveDrawInterface*)` моду і цей субсистем із dummy-актором не потрібен узагалі.

### D-10 — Клік із Inspector-вкладки (та інших вікон) вибирає тайл: механізм

- **Severity:** Medium · **Status:** Confirmed in code + сорси рушія
- **Location:** `TileDebugSelection/ViewportMouseClickNotifier.cpp:13-55`; рушій: `SceneViewport.cpp:197-209, 732-742`
- **Evidence:**
  - `FSceneViewport::GetMousePos` (local) повертає `CachedCursorPos`, який оновлюється лише тоді, коли курсор рухається *над віджетом вʼюпорта*. На `OnMouseLeave` він скидається в `(-1,-1)` **лише для game viewport** (`IsCurrentlyGameViewport()`). Для editor-вʼюпорта лишається остання позиція всередині, тож `IsInsideTheViewport` повертає true, виконується трейс і тайл виділяється.
  - `IsInsideTheViewport` порівнює *локальну* позицію з `GetInitialPositionXY()` (позиція вікна при створенні) — це семантично неправильно і працює лише тому, що зазвичай там 0.
  - Препроцесор глобальний для всіх вікон Slate: клік у Material Editor теж «вибере» тайл в активному level-вʼюпорті.
  - Спрацьовує на кожен LMB up, зокрема після орбітування камери (LMB-drag) і після перетягування гізмо.
  - Повертає `false`, тож редактор паралельно виділяє `ATileVisualGrid` у Details.
  - `static_cast<FEditorViewportClient*>` без перевірки типу (див. брифінг): підтверджено, але в поточних умовах (`IsPlayInEditor` → return) ризик низький.
- **Fix (мінімальний):** перевіряти, що віджет під курсором (`SlateApp.LocateWindowUnderMouse` / `FindWidgetUnderCursor`, або порівняння `MouseEvent.GetScreenSpacePosition()` з `CachedGeometry` віджета вʼюпорта) — це саме вʼюпорт активного level-клієнта; ігнорувати клік, якщо між down і up була суттєва дельта (drag).
- **Fix (правильний, рекомендую):** окремий **editor mode** (`UEdMode` + `UEdModeToolkit` або простіше `FEdMode` для MVP) «Tile Inspect»:
  - `HandleClick(FEditorViewportClient*, HHitProxy*, const FViewportClick&)` отримує рівно ті кліки, що були у вʼюпорті, вже без drag'ів, і може їх поглинути.
  - Hit proxy для ISM (`HInstancedStaticMeshInstance`) дає компонент + індекс інстанса без трейсу. Або просто трейс від `Click.GetOrigin()/GetDirection()`.
  - `Render(...)` дає PDI для підсвітки (замінює D-09).
  - Мод вмикається явно, тож поза ним тайли не «ловлять» кліки.
  - Компроміс: більше boilerplate (реєстрація моду, toolkit), і виділення в інших режимах не працює. Для debug-інструмента це скоріше плюс.

### D-11 — `FSlateApplication::Get()` без guard'а в editor subsystem

- **Severity:** Medium · **Status:** **Needs runtime verification** (ймовірний краш у commandlet'ах)
- **Location:** `TileDebugSelectionEditorSubsystem.cpp:24, 37`
- **Evidence:** `FSlateApplication::Get()` робить `check(CurrentApplication.IsValid())` (`SlateApplication.h:319-324`). Editor subsystems ініціалізуються і в commandlet-режимі редактора (cook, `-run=...`, частина headless-сценаріїв автотестів), де Slate не ініціалізовано. У `Deinitialize` до того ж `UnregisterInputPreProcessor` викликається і з null `ClickProcessor`.
- **Fix:** `if (FSlateApplication::IsInitialized())` в обох місцях; реєструвати препроцесор лише `if (!IsRunningCommandlet())`. Перевірити запуском cook'а або `UnrealEditor-Cmd.exe … -run=…`.

### D-12 — `IsDataValid` нічого не валідує

- **Severity:** Medium (блокує заплановані asset-тести) · **Status:** Confirmed in code
- **Location:** `TilesGenerationRuleSet.cpp:8-20`, `TilesVisualDataSet.cpp:9-25`
- **Evidence:**
  - Rule set пише лише `UE_LOG(Warning)` і повертає `Super::IsDataValid` (`NotValidated`). Валідатор асетів ніколи не покаже помилку.
  - Visual set пише в `Context.AddWarning`, але потім `Result = Valid` **перезаписує** можливий `Invalid` від `Super`.
  - Чи має rule set містити *кожен* `ETileType`, — питання. Правила — це «перший збіг», а відсутній тип — це нормальний конфіг (у пустелі може не бути трави), тож попередження на кожен тип, ймовірно, просто шум.
- **Fix:** вирішити, що є помилкою (наприклад, порожній `Rules`, `MinNoise >= MaxNoise`, null меш у варіанті, відсутній варіант для позиції, яку резолвер може повернути) і що попередженням. Писати в `Context.AddError/AddWarning` і комбінувати результат з `Super` через `CombineDataValidationResults`. Попередження «нема правила для типу X» прибрати.

### D-13 — Дані, яких бракує, і дані, яким тут не місце (`FTileData`)

- **Severity:** Medium · **Status:** Confirmed in code
- **Location:** `TileData.h`, `TileWorldGenerator.cpp:102-134`
- **Evidence:**
  - **Висота і сирі значення шуму ніде не зберігаються**: `Height` використовується для рішення «вода чи ні» і викидається. Саме тому інспектор не може показати «noise values», елевацію не буде з чого будувати, а вибір ділянки для ферми (рельєф, рівнинність) не матиме даних.
  - `NeighborMask` — похідні дані рендерингу (конкретної схеми автотайлу) в логічному тайлі, ще й з історією мутацій. З переходом на dual-grid поле втратить сенс.
  - `BiomeData` — сирий вказівник на асет. Для сейву «сид + дельти» потрібен стабільний ID (`EBiomeType` або `FPrimaryAssetId`), а не вказівник.
  - `SmoothingOverride` у тайлі — це конфіг біома (див. 1.4).
- **Fix:** розділити на (1) авторитетний стан тайла (Type, Biome ID, Fertility, Moisture, Occupied, далі Height level, Tilled тощо), (2) шари генерації (float Height/Moisture/…, distance fields) як окремі масиви в контексті генерації (SoA), (3) похідні дані рендеру (маски / corner-ключі), які обчислює й тримає візуальний шар.

### D-14 — Візуалізація живе в генераторі; доступу до даних ззовні немає

- **Severity:** Medium · **Status:** Confirmed in code
- **Location:** `TileWorldGenerator.cpp:136-172` (`VisualizeChunk`), `TileWorldGenerator.h:126-127`; `STileInspectorWindow.cpp:137`
- **Evidence:**
  - Батчинг за `FTileVisualKey` і виклик `FTileNeighborResolver` знаходяться в `ATileWorldGenerator`. Генератор знає про автотайли, а візуальний грид отримує готові батчі. Тобто розділення data/visual часткове: заміна рендерера зачепить генератор.
  - `DataGrid` — private, без геттера. Інспектор фізично не може дістати тайл, і `<todo>` у полі «Tile Type» — архітектурна проблема, а не UI-задача.
  - `GetTileChecked` повертає `nullptr` замість `check`, тобто назва вводить в оману; геттери не `const`.
- **Fix:** `ATileVisualGrid::Build(const UTileDataGrid&)` / `RebuildChunk(const UTileDataGrid&, ChunkIndex)`, а генератор лише каже «дані готові» (або візуал сам підписаний на подію гриду). Публічний `const UTileDataGrid* GetDataGrid() const` (або грид як окремий world subsystem — див. 3.4). Перейменувати на `FindTile` (nullable) / `GetTileChecked` (з `check`), зробити `const`.

### D-15 — `FTileVisualKey`: Rotation є в ключі, але не в ідентичності

- **Severity:** Low · **Status:** Confirmed in code
- **Location:** `TileVisualKey.h:50-61`, `TileTypeVisualData.h:41-51`, `TileVisualGrid.cpp:38-45`
- **Evidence:** `operator==` і `GetTypeHash` ігнорують `Rotation`, тож у мапі «виграє» перша зустрінута ротація (вона ж потрапляє в ім'я компонента). Працює лише тому, що поворот кладеться в per-instance transform. `FindVisualVariant` теж ігнорує ротацію, а `FTileTypeVisualVariant::Rotation` ніде не читається.
- **Fix:** прибрати `Rotation` з ключа (ключ = «який меш», поворот = властивість інстанса) і з варіанту.

### D-16 — Inspector прив'язаний до editor world і не реагує на зміни

- **Severity:** Medium · **Status:** Confirmed in code
- **Location:** `STileInspectorWindow.cpp:142-156`
- **Evidence:** історія береться з `GEditor->GetEditorWorldContext().World()`. У PIE (де світ генерується заново, D-05) історія пишеться в PIE-світ, і інспектор її не бачить. Підписки на `OnTileMutated` немає, тож після регенерації список не оновлюється до наступного вибору тайла. `SelectedTileCoordinates` за замовчуванням `(0,0)`, а в селекшн-сабсистемі `NoneValue`.
- **Fix:** вибирати світ явно (editor / активний PIE); підписуватись на подію мутації (з тротлінгом, бо генерація дає тисячі подій за кадр). Невибраний стан показувати як порожній.

### D-17 — `ShortPressThreshold` не ініціалізований у C++

- **Severity:** Low (High, якщо в BP теж 0) · **Status:** Confirmed in code; значення в `BP_CFG_PlayerController` **Needs runtime verification**
- **Location:** `ChillFarmGamePlayerController.h:24`, `.cpp:17-23, 77`
- **Evidence:** UObject-пам'ять обнуляється перед конструктором, тож дефолт = 0. `FollowTime` після першого `Triggered` > 0, тому `FollowTime <= 0` false, і `SimpleMoveToLocation` не викликається ніколи. `bMoveToMouseCursor` не ініціалізований і не використовується.
- **Fix:** дефолт у конструкторі (у шаблоні Top Down це 0.3), прибрати мертве поле.

### D-18 — Жорсткі дрібниці в шапках / UHT

- **Severity:** Low · **Status:** Confirmed in code; збірка без unity **Needs verification**
- **Evidence:**
  - `TileMutationContext.h:31` використовує `UBiomeTypeData*` без forward declaration і без include'а, тож шапка не self-contained і компілюється лише завдяки порядку include'ів. Під non-unity / IWYU-збіркою може впасти.
  - `TileData.h` не включає `CoreMinimal.h`.
  - `FTileMutationContext::Flags`: дефолт члена 0, дефолт параметра в `Make` — `INDEX_NONE` (0xFFFFFFFF). Не використовується, але значення суперечать одне одному.
- **Fix:** forward declare; include `CoreMinimal.h`; узгодити або прибрати `Flags`.

### D-19 — Runtime-створені HISMC: undo, Details, Modify

- **Severity:** Medium · **Status:** **Needs runtime verification**
- **Location:** `TileVisualGrid.cpp:47-72`
- **Evidence:** компоненти створюються `NewObject` + `RegisterComponent` без `AddInstanceComponent`, без `RF_Transactional`, без `Modify()` на актор/грид. Імовірні наслідки: їх немає в Details, Ctrl+Z після Generate поводиться непередбачувано, мапа може не позначатись dirty. При перезапуску construction script (рух актора в редакторі) поведінка залежить від `CreationMethod`. `SetMobility(Static)` викликається після `RegisterComponent`, що дає зайве перестворення render state.
- **Fix:** якщо приймається D-05 (transient), проблема undo/serialize значною мірою зникає: компоненти явно позначаються `RF_Transient` і не беруть участі в транзакціях. Налаштовувати властивості до `RegisterComponent`.

### D-20 — Ігровий цикл старту

- **Severity:** Low · **Status:** Confirmed in code
- **Evidence:** `Config/DefaultEngine.ini:2` → `GameDefaultMap=/Game/TopDown/Maps/TopDownMap` (мапа шаблону; запакована гра стартує не з генерації). `RestartPlayer(GetWorld()->GetFirstPlayerController())` спавнить лише першого гравця. `CropBase`: `Tick` увімкнений і порожній, `CropMesh` ніде не створюється; `IInteractableInterface::Interact(const FInteractionPayloadBase*)` приймає сирий вказівник на USTRUCT, тож його не реалізувати і не викликати з BP, а поліморфізм payload'ів через USTRUCT-вказівник небезпечний (немає RTTI).
- **Fix:** дрібні. Для інтерфейсу: `BlueprintNativeEvent` + `FInstancedStruct` (StructUtils) як payload, якщо потрібна гнучкість.

### D-21 — `bConstrainToPlane` у персонажа

- **Severity:** Low (стане актуальним з елевацією) · **Status:** Confirmed in code
- **Evidence:** `ChillFarmGameCharacter.cpp:31-32`. Зараз нормаль площини нульова, тому обмеження фактично не діє, але це «мина» з шаблону: якщо колись виставити нормаль, персонаж не зможе ходити терасами.
- **Fix:** прибрати.

### D-22 — Мертвий код і невикористані параметри

- **Severity:** Low · **Status:** Confirmed in code (grep)
- **Evidence:** `HeightNoiseScale`, `MoistureNoiseScale`, `BiomeBorderWidth/NoiseScale/Strength` (до того ж `EditDefaultsOnly` без категорії); `ATileWorldGenerator::DistanceToRange` оголошено, але не визначено; `FBiomeGenerationRule`, `FTileChunkData`, `UBiomeTypeData::IsShore / ResolveBorderTile / FillRuntimeTileData / ResolveSmoothingOverride`, `bAllowWater`, `ShoreHeight/Width`; `ATileVisualGrid::RebuildChunk` (тіло закоментоване) і `FinalizeNavigation`; `UTileTypeVisualData::bCanPlant / MovementCost / bAffectsNavigation` (геймплейні дані у візуальному асеті, як і зазначено в TODO); `UTileDebugSnapshotsRegistry`; `ApplyBiomeSmoothing`.
- **Fix:** видалити або позначити явно як «наступний крок». Невикористані параметри в Details найбільше заважають тюнінгу: їх крутиш, а нічого не змінюється.

---

## 3. Відповіді на питання розділу 8

### 3.1 (8.2) Чи годиться мутаційна система як основа для гравцевих змін і сейву «сид + дельти»?

**Що добре:** одна точка входу (`MutateTile`), контекст із джерелом/причиною, неможливість змінити поле в обхід (protected + friend). Це правильний скелет.

**Чого бракує:**

| Аспект | Стан | Що потрібно |
|---|---|---|
| Інвалідація похідних даних | Немає | Зміна типу тайла має інвалідувати маски 8 сусідів і позначити чанк(и) брудними. Зараз `RebuildChunk` закоментований, і немає події «змінились тайли X». |
| Батчинг | Немає | `BeginBatch/EndBatch` (або `MutateTiles(TArrayView<…>)`): одна подія і один rebuild на чанк, а не на тайл. Генерація має йти повз per-tile історію (див. 3.2). |
| Детекція no-op | За прапорцем | Порівнювати Before/After (1.10). |
| Сейв | Історія editor-only, з повними снапшотами | Для сейву потрібна **не історія, а дельта-стор**: `TMap<FIntPoint, FTileDelta>` (bitmask змінених авторитетних полів + значення), last-write-wins, лише для `Source == Gameplay`. Без похідних полів (`NeighborMask`) і без вказівників (біом → ID). Лоад = генерація з сиду → застосування дельт через той самий `MutateTile` з `Source = Load`. |
| Версіонування | Немає | Сейв має зберігати версію генератора + хеш конфігу. Інакше будь-який тюнінг DA (навіть зміна порядку біомів у `DA_BiomesSet`, бо від нього залежить tie-break) мовчки змінює світ під дельтами. |
| Undo | Немає | Для гравця undo навряд чи потрібен. Для редактора (якщо колись буде ручне редагування тайлів) — `FScopedTransaction` + `Modify()` на гриді, а не власна історія. Повний Before як undo-запис небезпечний: відкат перезаписує поля, змінені пізніше іншими мутаціями. |
| Потокобезпечність | Немає, і не треба | Мутації лише з game thread. Паралельна генерація пише напряму в масиви шарів (без `MutateTile`), а наприкінці публікує одну подію. |
| Пам'ять історії | ~150-200 Б на запис (2×`FTileData` + 2×`FName` + weak ptr + shared-ptr control block) | На 128² × 3 записи це ~7-10 МБ. На 1024² уже сотні МБ, тож історія має бути opt-in (CVar) і за замовчуванням не писати генераційні фази per-tile. Як альтернатива — зберігати лише змінені поля (той самий bitmask, що й для дельт). |
| Ідентичність запису | Немає порядку | `GenerationId` + монотонний `Sequence` у записі: інакше неможливо впорядкувати мутації між тайлами і відрізнити прогони (D-04). |

**Висновок:** як основа — так, але роль «історії» і роль «дельт для сейву» треба розвести. Це два різні споживачі однієї події.

### 3.2 (8.3) Як тримати editor-only історію поза shipping без `#if` по всьому коду

Рекомендую **observer + окремий модуль**:

1. `UTileDataGrid` експонує нативну подію, наприклад `FOnTilesMutated(const FTileMutationContext&, TArrayView<const FTileMutationRecord>)`, і **нічого не знає** про історію і про світ (заодно знімає D-06). Before-копію робити лише `if (OnTilesMutated.IsBound())`, тоді в shipping, де ніхто не підписаний, ціна практично нульова.
2. `UTileMutationHistorySubsystem` переїздить в editor-модуль (або в окремий модуль `ChillFarmGameDebug` з `Type = "DeveloperTool"`, який не потрапляє в shipping). Він підписується на грид, коли грид з'являється (через world subsystem гриду або подію «grid created»). PIE-світи editor-модуль бачить так само, тож історія в PIE теж працюватиме.
3. CVar `cfg.TileHistory.Enable` / `cfg.TileHistory.RecordGeneration` дає змогу вимкнути запис для великих мап.
4. Дельта-стор для сейву (3.1) — **runtime**-підписник тієї ж події.

Альтернатива «один `#if WITH_TILE_HISTORY` у тілі `RecordHistory`» теж нормальна і простіша, але лишає залежність runtime → history-код і не дає підключити другого споживача (дельти) без змін у гриді.

### 3.3 (8.4) Чи готовий пайплайн до елевації, доріг, похідних полів? Формалізувати проходи?

Зараз пайплайн захардкоджений: `GenerateWorld` викликає фіксовану послідовність, `UTileWorldPostProcessor` теж. Кожен новий етап означає правку двох класів, а дані між етапами передаються лише через `FTileData` (у якому немає навіть висоти, D-13).

**Рекомендую формалізувати, але легко:**

- `FTileGenContext` = розміри + **шари** (`TArray<float> Height`, `Moisture`, …; `TArray<uint8> HeightLevel`; distance fields) + фінальний грид + `FRandomStream` *на прохід*, з сидом `HashCombine(WorldSeed, GetTypeHash(PassName))`. Тоді додавання проходу не зсуває RNG інших проходів, тобто старі сиди не «ламаються».
- `UTileGenPass` (abstract, `EditInlineNew`) з `Execute(FTileGenContext&)`; на генераторі (а краще в окремому `UTileWorldGenConfig : UPrimaryDataAsset`) лежить `UPROPERTY(EditAnywhere, Instanced) TArray<TObjectPtr<UTileGenPass>> Passes`. Порядок задається масивом.
- Явні input/output декларації і граф залежностей — **поки ні**. Достатньо, щоб прохід у `Validate(const FTileGenContext&)` перевіряв наявність потрібних шарів. Граф вартий того, коли проходів стане >10 або з'явиться паралелізм між ними.
- Бонус для тестів і дебагу: прогін до N-го проходу + дамп шару в текстуру (heatmap) дає і debug-оверлей, і основу для UI вибору ділянки (як і зазначено в брифінгу).

Поточні частини відображаються так: Noise → Biome → TileType → Shore → Smoothing → [Elevation levels → Cliffs/Ramps → Water bodies → Roads (A*) → Distance fields → Site scoring → Validation] → Neighbor/visual keys (це вже не прохід генерації, а візуальний шар).

### 3.4 (8.5) Рендеринг: що і коли

- **Зараз (плоский світ, MVP):** HISMC лишити. Розглянути звичайний `UInstancedStaticMeshComponent`: в UE5 з GPU Scene ISM зазвичай не гірший, а перебудова дерева HISMC при runtime-змінах (тілінг!) дорожча. Це *inferred*, варто профайлити.
- **Момент перемикання:** **перед** реалізацією візуалу елевації, не раніше. З терасами кількість комбінацій «тип × автотайл-позиція × конфігурація кручі» вибухає, а тайловий підхід погано стикується з асет-паком (це пак оточення, не тайлсет).
- **Ціль:** per-chunk згенерований меш поверхні (`UDynamicMeshComponent` + Geometry Script для простоти; `ProceduralMeshComponent` теж підійде на 64×64 чанках; `RealtimeMeshComponent` — якщо профайлінг покаже потребу), побудований з dual-grid кутових висот (тераси + кручі одним алгоритмом). Матеріал поверхні за типом тайла — через vertex color / UV-канал з індексом у texture array або через per-chunk маску-текстуру. Пропси з асет-паку (дерева, каміння, паркани) лишаються ISM.
- **Runtime-модифікація:** тілінг/полив/посіви робити **не** перебудовою меша, а оверлеєм: per-chunk маска (render target або CPU-текстура) у матеріалі ґрунту + ISM/декалі для грядок. Перебудова меша лише при зміні висоти (будівництво/тераформінг), а це рідко.
- Навігація: один меш на чанк дає чистішу і дешевшу геометрію для Recast, ніж тисячі інстансів. При виборі висоти рівня врахувати `AgentMaxStepHeight=35`: якщо рівень вищий за 35uu, кручі автоматично стають непрохідними, а рампи працюють як nav-зв'язки. Це якраз бажана поведінка.

### 3.5 (8.6) Детермінізм

**В межах однієї збірки/платформи — так:** немає часу, потоків, `FMath::Rand`; `FRandomStream` (LCG) і `FMath::PerlinNoise2D` чисті функції; post-process спочатку збирає, потім застосовує, тож порядок не впливає.

**Джерела ризику:**

1. **`FMath::PerlinNoise2D` — реалізація рушія.** Будь-яка зміна в рушії (таблиця пермутацій, `Grad2`, `SmoothCurve`) змінює всі світи. Для «сид + дельти» це критично на горизонті кількох оновлень UE. Рекомендую власний (або вендорений, наприклад FastNoiseLite) шум з фіксованою версією.
2. **Float між компіляторами/платформами:** UE не гарантує бітову відтворюваність (FMA-контракції, fast-math, інший порядок операцій). На межах порогів (`WaterLevel`, правила) тайл може «переключитись». Для сейвів на одній платформі це неактуально; для крос-платформних сейвів — або integer/fixed-point шум, або зберігати дельти разом з авторитетними значеннями, а не лише «змінені поля».
3. **Конфіг як частина сиду:** властивості розміщеного актора + усі DA (включно з порядком масиву біомів, бо він визначає tie-break) = вхід генератора. Сейв має зберігати версію/хеш конфігу.
4. **`(float)Location.X` у Perlin** (є `LWC_TODO` у рушії) + період 256 шумового простору: на 128×0.025 це неактуально; на дуже великих мапах або великих офсетах точність деградує.
5. Secondary-шум не сідований: детермінізму це не шкодить, але різні сиди дають однаковий «візерунок» країв біомів.

### 3.6 (8.7) Масштабування понад 128×128

**Що блокує зараз:**

- `MutateTile` на кожен тайл: subsystem lookup + `MakeShared` + broadcast (game-thread-only, слухачі в Slate). На 1024² це мільйони алокацій.
- Синхронна генерація на game thread з `CallInEditor` і `BeginPlay`.
- Глобальний `UTileDataGrid` з одним масивом; візуалізація всього світу за раз; `InstanceToTile` росте з кількістю інстансів.
- Пост-процеси з сусідами потребують «фартуха» (apron) у 1 тайл на межах чанків. Глобальні проходи (A* доріг, BFS-поля) на чанки не ріжуться.

**Що вже добре:** шум — чиста функція від `(X, Y)`, тобто будь-який чанк можна генерувати незалежно і паралельно.

**Шлях:** (1) прибрати per-tile події з генерації (3.2); (2) `ParallelFor` по чанках для локальних проходів, які пишуть у свої зрізи шарів; (3) дворівнева генерація: глобальна грубa карта (наприклад, 1/8 розміру: біоми, ріки, дороги, вибір ділянок), яка рахується одразу, + детальні чанки на вимогу; (4) стрімінг чанків за дистанцією від камери — лише якщо фінальний розмір цього вимагатиме. Для «Stardew у 3D» 256-512² тайлів, найімовірніше, вистачить без стрімінгу, тож я б зафіксував розмір світу раніше, ніж будувати стрімінг. (5) `UE::Tasks` / `Async` для генерації в рантаймі з екраном завантаження. UObject-вказівники на DA читати з воркерів безпечно, якщо DA гарантовано живі (а вони живуть, поки на них посилається генератор).

### 3.7 (8.8) Lifetime / ownership — зведення

| Місце | Проблема | Ризик |
|---|---|---|
| `ATileVisualGrid::InstanceToTile` | Сирі ключі, не UPROPERTY, не чиститься, адресу перевикористовує `NewObject` | D-03 (High) |
| HISMC через `NewObject` з фіксованими іменами | In-place перебудова знищеного компонента | D-03, D-19 |
| `UTileDebugHighlightSubsystem` | `OnMapOpened` без відписки; `DummyActor` спавниться в чужий світ; `RenderComponent` може бути null | D-09 |
| `UTileDebugSelectionEditorSubsystem` | `FSlateApplication::Get()` без guard'а; `TileVisualGrid` лише з `OnMapOpened` (краще брати `Hit.GetActor()`) | D-11 |
| `UTileDebugSnapshotsRegistry` | Підписка на world subsystem конкретного світу, переживає світ | мертвий код |
| `FTileMutationHistoryEntry` в `TSharedPtr` | Містить `UBiomeTypeData*` (через `FTileData`), невидимий для GC | Low: DA тримаються датасетом; але якщо біом видалять із сету під час сесії, вказівник висне |
| `ATileVisualGrid::NavMeshBoundsVolume` | Лише в `OnConstruction`; у cooked-грі для розміщених акторів construction script не перезапускається | Low (зараз не використовується) |
| `DataGrid` / `PostProcessor` | `CreateDefaultSubobject` + fallback `NewObject` з тим самим ім'ям; все серіалізується в мапу | D-05 |
| `STileInspectorWindow` | `AddSP` слабкий — OK; deref weak ptr | D-07 |
| `AChillFarmGameGameMode` | `AddUObject` без відписки | OK (лайфтайм збігається) |

### 3.8 (8.9) Тестованість

**Що заважає зараз:** уся генерація — protected-методи `AActor`, яким потрібні світ, `ATileVisualGrid` у рівні (інакше краш, D-02), DA і `GetWorld()` у гриді (D-06). Результат не можна отримати без побічних ефектів (HISMC, історія).

**Що змінити:**

1. Виділити чисте ядро: `FTileWorldGenerator::Generate(const FTileGenSettings&, const UTileWorldGenConfig&) → FTileGenResult` (або UObject без світу). Актор стає тонкою обгорткою: прочитати налаштування → згенерувати → віддати візуалу.
2. Грид без залежності від світу (подія замість subsystem lookup).
3. DA в тестах створювати через `NewObject` у transient package і заповнювати в коді: тести не залежать від контенту.

**Що покрити (Automation Spec / `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, або Low Level Tests для чистого ядра):**

- Детермінізм: той самий сид → той самий хеш типів тайлів; різні сиди → різні хеші; «golden hash» для 2-3 сидів (ловить зміни рушія з 3.5).
- Інваріанти: немає води вище `WaterLevel` (ловить 1.3 п.5); кожен суходіл поруч з водою → shore-тип; `NeighborMask` збігається з перерахованою; немає тайлів `ETileType_MAX`.
- Резолвер: усі 256 масок дають очікуваний ключ (таблиця) — одразу ловить 1.2.
- Біоми: значення всередині діапазону обирає цей біом (ловить 1.3 п.1); за відсутності другого біома — первинний.
- Мутації: кожен сеттер змінює рівно своє поле; no-op не пише запис; історія впорядкована.
- Asset validation: `IsDataValid` на реальних DA як окремий тест + Data Validation у CI (`-run=DataValidation`), після виправлення D-12.

---

## 4. Оцінка дизайн-думок із розділу 7

| Думка | Моя оцінка |
|---|---|
| **Розділення data/visual — найсильніше рішення** | Згоден з принципом, але зараз воно часткове: генератор сам батчить за візуальними ключами і викликає резолвер (D-14), а `NeighborMask` (дані рендеру) лежить у логічному тайлі (D-13). Поки що це скоріше намір, ніж факт. |
| **HISMC прийнятний для плаского світу** | Згоден. Додам: перевірити ISM vs HISMC профайлером, а «момент перемикання» прив'язати до елевації (3.4). |
| **Елевація: цілі рівні, тераси, кручі, рампи, сусіди ±1** | Згоден. Додам зв'язок з навігацією: висота рівня > `AgentMaxStepHeight` (35uu) робить кручі непрохідними «безкоштовно». Правило «сусіди ±1» краще забезпечувати окремим проходом-«ерозією» після квантування, а не вимагати від шуму. |
| **Dual-grid автотайлінг** | Згоден, і для круч він навіть важливіший, ніж для ґрунту. Нюанси: (1) з >2 типами ґрунту потрібне шарування (кожен тип — шар у порядку пріоритету) або блендинг у шейдері; (2) візуальні тайли зміщені на пів клітинки, тож клік → тайл **мусить** іти через world→grid (ще один аргумент за D-03a і D-08). |
| **Whittaker-lookup замість range-scoring** | Згоден з напрямком. Як мінімум виправити range-scoring (1.3) і перевірити, чи вистачає domain warp: зараз `WarpStrength 0.1` у просторі шуму ≈ 4 тайли, це слабко для «нерівних» меж. |
| **Inspector: world→grid замість instance index** | Згоден, з передумовою D-08. |
| **Вибір ділянки, дороги A*, BFS-поля, валідація** | Згоден. Додам: BFS на 128-512² дешевий (мілісекунди), тож поля можна рахувати і в рантаймі при кожній генерації, а не зберігати. Валідацію «є ≥ N придатних ділянок» робити з детермінованим retry (сид + лічильник спроб), інакше втрачається відтворюваність. |
| **Порядок: дані → Inspector UI → навігація/ворнінги → елевація** | Частково не згоден, див. розділ 5. Build breakers і краші коштують хвилини, тому вони першими. Рішення D-05 (де правда — сид чи мапа) треба ухвалити **до** роботи над Inspector UI, бо воно визначає, в якому світі живе історія і чи серіалізуються дані. |

**Про ворнінги «saved with empty engine version»** (5.4 брифінгу): у коді причин немає, це питання асетів. Зазвичай лікується ресейвом (Resave Packages / `-run=ResavePackages -ProjectOnly`). Але якщо приймається D-05, частина цих асетів (сама мапа з тайлами) і так зміниться.

---

## 5. Пріоритети

1. **D-01 — збірка Game-таргета** (хвилини). Без неї не можна зробити жоден пакет, і кожен новий `IsDataValid` повторить проблему. Додати Game-збірку в звичний цикл.
2. **D-02, D-06, D-07, D-09 (null-deref'и)** (до години). Генерація тепер виконується з `BeginPlay`, тож це краші гри, а не лише інструмента; D-06 до того ж блокує тести.
3. **D-05 — рішення «правда = сид + конфіг», все згенероване Transient.** Невелика зміна в коді, але вона визначає все далі: розмір і стабільність мапи в git, де живе історія, як працює навігація на старті, модель сейву.
4. **Коректність генерації: 1.3 (біоми), 1.2 (резолвер), 1.8 (нормалізація шумів), 1.4 (smoothing).** Без цього все, що інспектор показує і що тестуватиметься, — неправильний світ. Паралельно — перші тести інваріантів (3.8), вони одразу підтвердять виправлення.
5. **D-03 + D-08 — координати.** Спершу одна конвенція grid↔world, потім прибрати `InstanceToTile` на користь world→grid. Знімає краш-клас, неправильні координати і готує ґрунт для dual-grid.
6. **Навігація (1.6, D-17)** — пройти чекліст діагностики. Ймовірно, вирішиться пунктами 3 і 5 + дефолтом `ShortPressThreshold`.
7. **Інструмент:** D-10 (editor mode замість препроцесора) разом з D-09; D-04/3.2 (історія як підписник, очищення, CVar); D-14 (доступ до гриду) і лише тоді UI інспектора (diff Before/After).
8. **Архітектура під елевацію:** D-13 (шари генерації, висота), 3.3 (проходи), далі елевація + dual-grid + рішення про меш-рендеринг (3.4).

Дрібниці (D-15, D-18, D-20…D-22, 1.1, 1.5, 1.9) — разом з будь-яким із пунктів вище, коли зачіпається той самий файл.
