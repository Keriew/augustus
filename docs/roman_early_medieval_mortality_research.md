# Roman and Early Medieval Mortality Research for `defines.xml`

## Why this note exists

This is a research memo for tuning the `mortality_table` in `Mods/Vespasian/defines.xml` so it feels closer to Roman and early post-Roman demographic conditions instead of modern or near-modern survival.

## What the game code is doing right now

- `src/city/population.cpp` applies each mortality-table entry as a **yearly death percentage** to one ten-year age cohort.
- The cohort index is `age / 10`, so the ten columns are effectively `0-9`, `10-19`, `20-29`, ... `90-99`.
- However, deaths are processed **before** births are added for the year. That means the first cohort behaves more like **ages 1-9**, not true `0-9` including newborns.
- `src/city/population.cpp` uses `game_defines_mortality_percentage(city_data.health.value / 10, decennium)`, so health `0-100` collapses into **11 buckets** (`0-10`).
- `src/city/health.c` makes very high health fairly reachable for advanced housing because house level, clinic/hospital, bathhouse, barber, water/latrine, mausolea, and multiple foods can all stack toward `100`.

## Immediate simulation consequences

- The current high-health rows are far too generous for a Roman-world setting.
- Bucket `10` currently has `0,0,0,0,0,0,0,2,5,10`, which means **no annual mortality at all through the sixties**.
- Bucket `8` still has no deaths until the fifties.
- Roughly speaking, those rows imply survival curves closer to modern or elite early-modern populations than to Roman imperial cities.
- At the low end, some rows are already extremely lethal. Bucket `0` is not "ordinary poverty"; it is closer to **catastrophic deprivation**.

## Historical anchors from the literature

### Roman world

- Walter Scheidel's demographic survey in *The Cambridge Economic History of the Greco-Roman World* says the broad consensus for ancient populations is **life expectancy at birth around 20-30 years**, with Roman Egypt reconstructions broadly consistent with **about 22-25 years**.
- Isabelle Seguy's open-access review of Roman demography says Roman populations should be modeled as **preindustrial**, with **high infant mortality, high fertility**, and major local variation by environment, status, sex, and urbanization.
- The same review notes that Roman mortality estimates are usually model-based because direct infant-mortality evidence is weak, but the consensus still sits in that **high-mortality premodern range**.
- Oxford material summarizing Beryl Rawson reports that in ancient Rome **about one in ten died in very early infancy** and **about one-third died before age one**.

### Urban penalty

- Roman cities, especially very large ones, were not "safe" just because they had baths, fountains, or physicians. Dense settlement still meant more infection pressure, contaminated food chains, and repeated epidemic risk.
- Seguy explicitly notes that exceptional urbanization in Rome could alter mortality and fertility, and that epidemics, malaria, and crowding mattered.
- Later medieval evidence points the same way: a PubMed study of medieval England found that **urban adults had elevated mortality risk and lower survivorship** than rural adults.

### Early dark ages / early medieval

- Seguy's review cites archaeological work from Normandy indicating **Merovingian mortality was less favorable than Gallo-Roman mortality**, with **higher mortality especially at young ages**.
- A bioarchaeological study of early medieval Central Europe (9th-10th century) shows highly variable child feeding and stress patterns, reinforcing that child survival remained fragile well after the western Roman period.
- In other words: moving from Rome to the early dark ages should not mean lower mortality. If anything, in many places it should mean **equal or worse juvenile survival** and continued heavy background mortality.

## What this means for your table

### 1. The high-health end is the real problem

If you simply "raise all mortality rows," you risk making the bottom of the table absurdly lethal while still leaving the core problem untouched. The table is currently too forgiving mainly in buckets `6-10`, especially for cohorts under `60`.

### 2. Best Roman health should still be harsh by modern intuition

Even with baths, doctors, barbers, clean water, latrines, varied diet, and decent housing:

- children still died
- infections still killed adults
- childbirth still killed women, which your non-sexed model can only approximate through general adult mortality
- injury, gastrointestinal disease, malaria, and periodic epidemic shocks still existed

So the top bucket should represent **the best survivorship available in a Roman city**, not "quasi-modern longevity."

### 3. You currently have no true infant mortality pass

Because births are added after deaths, your first mortality cohort does **not** capture death before age one. That matters a lot.

Implication:

- If you try to recreate Roman infant mortality only by inflating the `0-9` cohort, you will overkill ages `1-9`.
- If historical fidelity matters a lot, the best long-term fix is a code change that applies a separate mortality pass to `age 0` or to newborns before they enter the next year.

### 4. You currently have no sex-specific maternal mortality

Roman and early medieval populations paid a real mortality cost in the reproductive years. Since the sim does not split male and female mortality, the only way to represent some of that cost inside the current design is to avoid zeroes in the `20-39` cohorts.

## Recommended tuning approach

### Short version

- Do **not** increase the whole table evenly.
- Leave bucket `0` and maybe bucket `1` as crisis-level mortality unless you want deprivation to be less catastrophic.
- Focus most of the rebalance on buckets `5-10`.
- Remove all zeroes from the top half of the table.
- Add a small but real background mortality in `10-39`.
- Start the old-age ramp earlier, with visible deterioration already in `50-59`.

### Practical first pass

Treat the top row as your anchor. A reasonable first-pass target for **bucket `10`** would be something in this neighborhood:

`3,1,1,2,3,5,9,16,28,45`

Why this is a good starting point:

- it stops "perfect health" from being immortal until old age
- it keeps childhood and early adulthood better than lower buckets, but not safe
- it bakes in some reproductive-age and infection risk
- it produces a much more Roman-looking late-life ramp

If you want an even harsher Roman-urban ceiling, push the first six entries up slightly. If you want an elite or unusually healthy provincial ceiling, keep this row but do not go much lower.

### How to derive the rows below it

Instead of handwaving every row separately, use the top row as the historical ceiling and then worsen it by bucket:

- buckets `8-9`: add about `+1` to childhood and young-adult cohorts, `+1 to +2` to middle age, `+2 to +5` to old age
- buckets `6-7`: add another `+1 to +2` across the board, with bigger penalties after `40`
- buckets `3-5`: make mortality clearly premodern-poor, especially in `0-9`, `20-39`, and `50+`
- buckets `0-2`: decide whether these mean "ordinary poverty" or "collapse conditions"; right now they read closer to collapse

### Shape to aim for

For a Roman or early medieval table, every row should generally follow this pattern:

- highest mortality in the youngest cohort
- lower but nonzero mortality in teens
- low but nonzero mortality in `20-39`
- noticeable acceleration in `40-59`
- steep acceleration in `60+`

That shape matters more than chasing a fake exact number.

## If you want the strongest historical result

The most accurate path is:

1. Add a separate newborn/infant mortality step in code.
2. Keep the mortality table for ages `1+`.
3. Retune the highest health buckets downward.
4. Use `20-39` mortality to stand in partly for maternal mortality and infectious-disease burden.

If you do **not** want code changes yet, then the best compromise is:

1. Retune buckets `6-10` aggressively.
2. Retune buckets `3-5` moderately.
3. Leave buckets `0-2` mostly alone unless you think deprivation is too punishing.

## Bottom line

The table should not make Roman "perfect health" people functionally immortal until age 60. The more historically grounded approach is to:

- compress the gap between good and excellent health
- keep mortality nonzero at every age
- preserve strong child mortality pressure
- preserve real adult mortality in the reproductive years
- make old age dangerous well before `70`

## Sources

- Walter Scheidel, "Demography," *The Cambridge Economic History of the Greco-Roman World*:
  [https://www.cambridge.org/core/services/aop-cambridge-core/content/view/4D4C2E2D1669317B44CAF7A1E46CED14](https://www.cambridge.org/core/services/aop-cambridge-core/content/view/4D4C2E2D1669317B44CAF7A1E46CED14)
- Isabelle Seguy, "Current Trends in Roman Demography and Empirical Approaches to the Dynamics of the Limes Populations" (open access):
  [https://link.springer.com/chapter/10.1007/978-3-030-04576-0_2](https://link.springer.com/chapter/10.1007/978-3-030-04576-0_2)
- Oxford extract summarizing Beryl Rawson on Roman infant mortality:
  [https://academic.oup.com/book/6954/chapter/151218649](https://academic.oup.com/book/6954/chapter/151218649)
- Brittany S. Walter and Sharon N. DeWitte, "Urban and rural mortality and survival in Medieval England":
  [https://pubmed.ncbi.nlm.nih.gov/28006969/](https://pubmed.ncbi.nlm.nih.gov/28006969/)
- Sylva Kaupova et al., "Urban and rural infant-feeding practices and health in early medieval Central Europe (9th-10th Century, Czech Republic)":
  [https://pubmed.ncbi.nlm.nih.gov/25256815/](https://pubmed.ncbi.nlm.nih.gov/25256815/)
