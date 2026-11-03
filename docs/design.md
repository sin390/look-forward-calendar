# Calendar Design Document

English | [中文](design_CN.md)

LookForward Calendar (LFCalendar), a terminal calendar built with FTXUI: month view + schedule management (events / fixed schedules / long-term tasks) + Chinese lunar calendar + Chinese and Japanese holidays.

## 1. Tech stack

| Item | Choice |
|---|---|
| Language | C++23 (date arithmetic with `<chrono>`) |
| UI | FTXUI v7.0.3 |
| JSON | nlohmann/json (header-only, via FetchContent) |
| Build | CMake + Ninja, GCC 15.1 (MinGW), statically linked |
| Platform | Windows only; no cross-platform support |

## 2. Data directory

The root is `data/` in **the exe's directory** (independent of the working directory at startup).

```
data/
  general/<year>/<month>.json    per-day properties and events of the month, e.g. general/2026/11.json
  task/<created>_<id>.json       one file per long-term task (operation log), e.g. task/20261102_a3f9.json
  fixed/<created>_<id>.json      one file per fixed schedule, e.g. fixed/20261103_b71c.json
  preference.json                settings: { "holiday_config": 1, "language": "EN", "lunar": true, "slogan": "Look Forward!" }
  holidays/config<N>_<year>.json holidays of holiday config N, e.g. holidays/config1_2027.json
```

Language files are not user data. They live in `language/` in the exe's directory (`EN.json`, `CN.json`, ...) and are copied there from the source `language/` by CMake after each build.

## 3. Data model

### 3.1 Day (one object per day)

`general/2026/11.json`: keyed by date; days without content are not written.

```json
{
  "2026-11-02": {
    "events": [
      { "title": "Group meeting", "start": "14:00", "end": "15:30", "note": "" }
    ]
  }
}
```

- Events are stored directly on the Day.
- Adding, editing or deleting an event rewrites the month file.

### 3.2 FixedSchedule (one file per fixed schedule)

```json
{
  "id": "b71c",
  "title": "Group meeting",
  "start": "09:00",
  "end": "10:00",
  "note": "Room A",
  "repeat": "weekly",
  "weekdays": [2, 4],
  "from": "2026-11-03",
  "until": ""
}
```

- `repeat`: `daily` / `weekly` (default) / `biweekly` / `monthly`. `weekdays` (1 = Monday … 7 = Sunday) is used only by weekly and biweekly.
- biweekly counts the week containing `from` as the first week; monthly repeats on the day of the month of `from`, skipping months without that day. An empty `until` means it repeats forever (`until` itself is included).
- All definitions are read at startup and on refresh; a month is expanded into per-day lists the first time it is shown and then cached (`AppState::FixedOn`); the cache is cleared when a schedule is added or deleted.
- Schedules can only be created and deleted; deleting removes the file, and with it every date.

### 3.3 Task (long-term task, one file per task)

The file stores an **operation log**; the task's current state and its daily workload are computed by replaying the log.

```json
{
  "id": "a3f9",
  "name": "Thesis",
  "log": [
    { "at": "2026-11-02T22:10", "op": "create",
      "hours_total": "40:00", "due": "2026-12-20",
      "weekdays": [1, 2, 3, 4, 5], "skip_holiday": true, "holiday_config": 1 },
    { "at": "2026-11-03T21:00", "op": "progress", "date": "2026-11-03", "minutes": 150 },
    { "at": "2026-11-10T09:00", "op": "modify", "fields": { "due": "2027-01-10" } },
    { "at": "2026-11-15T20:00", "op": "pause" },
    { "at": "2026-11-18T20:00", "op": "resume" },
    { "at": "2026-12-01T18:00", "op": "complete" }
  ]
}
```

| op | Meaning | Fields |
|---|---|---|
| `create` | create | total planned time, due date, workdays (1 = Monday … 7 = Sunday), whether to skip holidays, holiday config (`holiday_config`, 1 / 2; logs written before v1.5.0 have `country`: CN → 1, JP → 2) |
| `progress` | actual work on a day | `date`, `minutes`, `note` (a later entry for the same day overwrites the earlier one) |
| `modify` | change properties | `fields`: total time / due date / workdays / name, etc. |
| `pause` / `resume` | pause / resume | the due date does not change while paused |
| `complete` | finished (early or on time) | |
| `abort` | aborted | |

- Short id: 4 random hex digits generated at creation and stored as a property; regenerated if it collides with an existing task or fixed schedule.
- File name: `<created>_<id>.json`; the task name is stored only inside the file.

### 3.4 Task state machine

```
           pause             complete / abort
 Active ─────────▶ Paused ─────────────────▶ Ended
   ▲ ◀─────────── │
   │    resume    │
   └─ overdue, unfinished ─┘ (automatically paused, shown in red)
```

### 3.5 Daily workload

For a task and a date D (today or later):

```
R = total time − sum of actual work logged (before today)
N = number of workdays from today to the due date (including today)
workday = weekday is in weekdays, and (skip_holiday is false, or the day is not a day off in holiday config holiday_config)
daily amount = ceil(R / N), rounded up to 10 minutes
```

- The creation day counts.
- Work is assigned day by day: each day takes ceil(remaining / remaining workdays), rounded up to 10 minutes, and is then subtracted from the remainder. Later days may get slightly less; the total equals the remainder exactly.
- Today's plan uses only work logged **before today**; work logged today does not change today's target.
- A day with nothing logged counts as 0 minutes, and the following days get more.
- No work is assigned while paused; the due date stays, so each day gets more after resuming.
- Completed and aborted tasks get no more work.
- Overdue (or no usable workday left) with R > 0: the task is shown in red as a **computed paused state** (nothing is written to the log); it resumes automatically when the due date is extended.
- Make-up workdays are ignored (`off: false` in holiday data is only displayed).
- **Logging actual work**: the [Log] button in the sidebar; the value overwrites and can be changed any time; today and earlier dates can be logged (including catching up).
- **Past dates**: the calendar cell shows the actual work logged that day.
- **Refresh (r)**: re-reads every file on disk, updates "today" and recomputes all plans. Plans are also recomputed after every change.

## 4. Lunar calendar

- A built-in compressed lunar table for 1900–2100 (one integer per year encoding the leap month and long/short months); no network needed.
- Computes: lunar date, sexagenary cycle, zodiac animal, lunar festivals (Spring Festival, Lantern Festival, Dragon Boat, Qixi, Mid-Autumn, Double Ninth, New Year's Eve) and the 24 solar terms (including Qingming and the winter solstice).

## 5. Holidays

### 5.1 Holiday configs and the file format `holidays/config<N>_<year>.json`

```json
{
  "country": "中国",
  "year": 2027,
  "days": [
    { "date": "2027-02-06", "name": "春节", "off": true },
    { "date": "2027-02-14", "name": "春节调休补班", "off": false }
  ]
}
```

- `off: true` is a day off; `off: false` is a make-up workday (e.g. in China).
- There are two holiday configs (`kHolidayConfigCount`), fixed slots not tied to any country; the config and year come from the file name, not the content.
- `country` is just a property of the config: the name entered for the prompt. It is used only in the prompt and in the sidebar line "country: holiday".
- Files from before v1.5.0 (`CN_<year>.json`, `JP_<year>.json`) are renamed on load to `config1_…` / `config2_…`, with `country` CN / JP turned into 中国 / 日本.
- Only special dates are listed; other dates are ordinary workdays or weekends.

### 5.2 Import flow

1. In the program, press the key, choose a holiday config (1 / 2), enter a country and a year; a dialog shows the prompt and copies it to the clipboard.
2. The user sends the prompt to any AI and saves the returned JSON to a file.
3. The user enters the file path; the program validates the format (fields, valid dates, matching year).
4. If valid, it is written to `holidays/config<N>_<year>.json`, replacing existing data for the same config and year.

### 5.3 Prompt template (built in; `{COUNTRY}` / `{CONFIG}` / `{YEAR}` are replaced)

The prompt is itself a piece of UI text: this English version is the key in the source code; `CN.json` holds the Chinese translation used when the UI language is CN.

```
List all statutory public holidays of {COUNTRY} for the year {YEAR}, strictly in the JSON format below.
Put the complete JSON in a single ```json code block, and output nothing outside the code block.
If you can create files, also create a downloadable file named config{CONFIG}_{YEAR}.json with exactly the same JSON as the code block.

{
  "country": "{COUNTRY}",
  "year": {YEAR},
  "days": [
    { "date": "YYYY-MM-DD", "name": "Holiday name", "off": true }
  ]
}

Rules:
1. One entry per date; list every day of a multi-day holiday.
2. Days off (including substitute holidays for holidays that fall on a weekend) have "off": true.
3. Also list weekend days that become working days because of holiday adjustments, with "off": false and name "XX make-up workday".
4. Use the common name of each holiday in that country's language.
5. Only list dates within {YEAR}, sorted by date in ascending order.
6. Follow the schedule officially published by the government; if it has not been published yet, append "(forecast)" to the name.
```

## 6. Screen (month view)

```
┌ November 2026 ──────────────────────────────────┬ 11/2 Mon ──────────────┐
│  Su     Mo     Tu     We     Th     Fr     Sa   │ Lunar: 十月初二        │
│ ┌────┐                                          │                        │
│ │ 2  │  solar day / lunar day, festival or term │ [Events]               │
│ │初二│                                          │  14:00-15:30 Meeting   │
│ │●2  │  number of events                        │                        │
│ │▶3h │  total task hours that day (red if overdue)│ [Tasks]              │
│ └────┘                                          │  Thesis    Plan 2:00   │
│                                                 │            Actual 1:30 │
│                                                 │  [Log][Pause][Modify]  │
├─────────────────────────────────────────────────┴────────────────────────┤
│ Move / Actions / Settings key hints                                      │
└──────────────────────────────────────────────────────────────────────────┘
```

- Weeks start on Sunday (Su Mo … Sa); the weekday checkboxes in dialogs and texts such as "Weekly Su Mo We" use the same order. In the data, Monday is still 1.
- Days off and make-up workdays are shown in different colors.
- Sidebar: the day's events and fixed schedules; for each task, the plan and actual work for the day and its buttons.

## 7. Modules (actual structure)

```
src/
  core/        independent of the UI
    date.*         date and duration helpers (std::chrono)
    lunar.*        lunar calendar, solar terms, lunar festivals, sexagenary cycle and zodiac
    holiday.*      holiday format validation, prompt, HolidayCalendar
    i18n.*         multiple languages: Tr() / N_(), language file parsing
    json.h         alias for nlohmann::ordered_json
  model/
    day.*          Day / Event and JSON conversion
    task.*         Task log, replay, workload computation, validation
    fixed.*        FixedSchedule repeat rules, JSON conversion, validation
    preference.*   settings (holiday config, language, lunar calendar, slogan)
  store/
    fs_util.*      UTF-8 paths, atomic file writes
    repository.*   reads and writes general/ task/ fixed/ holidays/ preference.json
  app/
    app_state.*    application state: every change goes through here (write to disk + recompute + bump version)
  platform/
    platform.*     exe directory, clipboard, UTF-8 console (Windows API)
  ui/            FTXUI
    widgets.*      shared styles and widgets (buttons, inputs, panels, CJK-aware WrapText)
    logo.*         pixel-art logo (About dialog)
    month_view.*   month grid
    day_panel.*    sidebar (events, fixed schedules and tasks of the selected day)
    overview_screen.* / reminder_screen.*  full-screen task overview and reminder list
    dialogs.*      dialog base class + event / fixed schedule / task / progress / holiday import / about
    app.*          assembly, global keys, snapshot rendering
  main.cpp
tests/test_main.cpp  core / model unit tests
res/                 exe icon (app.rc + LookForward.ico)
tools/make_icon.py   generates res/LookForward.ico and docs/logo.png from the pixel data in logo.cpp
tools/i18n.py        scans the UI text in the source, generates language/EN.json, checks the other language files
language/            language files (EN.json is the template, CN.json is Chinese)
```

- The static library `calendar_core` contains everything except the UI; both `LookForward` (the program) and `calendar_tests` link it.
- UI extension points: a new panel is a `ComponentBase` subclass; a new dialog derives from `Dialog` (Esc to close, Tab focus order and error line built in); dynamic widgets are rebuilt lazily based on `AppState::version()`.
- Debugging: `LookForward --data <dir> --snapshot 150x40 "n,TAB,10"` feeds the keys and prints the screen as text.
- Release: `./build.sh package` builds the `release` preset (`build-release/`, linked with `-s`), runs the unit tests and packs `LookForward.exe`, `language/`, the READMEs and `docs/logo.png` into `dist/LookForward-v<version>-win64.zip` (the version is read from `src/version.h`). `data/` is not packed; it is created next to the exe on first use.

## 8. Decisions

1. Make-up workdays are ignored.
2. Today's plan uses only actual work logged before today.
3. Actual work overwrites, can be changed any time, and past dates can be filled in.
4. Overdue is only a computed paused state; extending the due date resumes the task.
5. (Changed in v1.5.0) A task that skips holidays may extend into years without holiday data; those years count as having no holidays. Importing holidays and refreshing (`r`) reload the holiday data and recompute every plan, so the plan corrects itself once the data is there. (Before v1.5.0, creating or modifying such a task was refused.)
6. (Superseded by 10) A fixed-width column on the right showed every task with a progress bar; calendar cells scale with the window width.
7. Keys are limited to those on small keyboards: arrows or `h j k l` to move, `[ ]` to change month, `t` for today, `Tab`/`Esc` to switch panes; `a` event, `n` task, `i` import holidays, `c` switch holiday config, `r` refresh, `q` quit.
8. The [Log] button of a task opens a dialog for the day's actual time and a note (`note` of a progress log entry); it overwrites and can be changed again; future dates are locked.
9. **Event reminders**: when creating/editing an event, tick "Remind" and enter a lead time (`30m` / `1:30` / `2` (hours) / `1d`, up to 30 days), stored in the event's `remind` field (minutes).
   The reminder period is [start − lead time, end); without an end time it ends at the start minute; an all-day event starts at 0:00 and lasts until the end of the day.
   Events in their reminder period go into the reminder list, sorted by time; the status bar shows how many. The program refreshes every 30 seconds and reloads when the date changes at midnight.
10. **Subscreens**: the right-hand overview column was replaced by full-screen subscreens. `o` task overview (`1` active by due date, `2` all by creation date, `3` completed by final due date), `m` reminder list (Enter jumps to the date). `Esc`/`q` go back.
11. Every place that shows a task also shows `(ID:xxxx)` (the task overview has a separate ID column instead, see 19). `v` opens the version information; the version is in `src/version.h` (currently v1.5.1, 2026-11-03; the About dialog shows the pixel logo and the model Claude Opus 5.5), and CMake's `project(VERSION)` matches it.
12. In the task overview, `Enter` shows the selected task's full log (in chronological order, made readable by `DescribeLogEntry`) and `d` deletes the task file (confirm with `y`). Subscreens handle their own back keys.
13. **v1.2.0**: the program is named LookForwardCalendar (LFCalendar for short, executable `LookForward.exe`). Since v1.5.0 the official name is written "LookForward Calendar", still LFCalendar for short.
    The holiday prompt asks the AI to put the JSON in a ```json code block and, if it can create files, to also create the data file; import ignores text around the code block.
    The sidebar, status bar messages and dialog hints wrap with `WrapText` (`ftxui::paragraph` only breaks at spaces, so long Chinese text never wrapped); the sidebar scrolls only vertically (`yframe`).
    Order of a task in the sidebar: name / ID and status / plan and actual / progress note / button row (Log, Pause or Resume, Modify, Abort, Done), with a blank line between tasks.
    The pixel logo is also the exe icon: `logo.cpp` is the single source of the pixel data, and `tools/make_icon.py` generates the icon from it (the result is committed, so building does not need Python).
14. **v1.3.0 fixed schedules**: `f` creates one (start date defaults to the selected day, repeat to weekly, the weekday to the start date's weekday).
    The third line of a calendar cell shows the count as a cyan `◆n`; the sidebar shows [Fixed schedules] between events and tasks, each with its time, work content, repeat rule, note and [Del].
    [Del] deletes the whole schedule and needs two presses (the first turns it into [Confirm delete] and shows a hint in the status bar); rebuilding the sidebar (changing the day or the data) cancels the pending confirmation.
    Fixed schedule ids and task ids share one generator and never collide. Editing, skipping holidays and reminders are not supported yet.
15. **Settings**: `data/preference.json` stores the holiday config shown on the calendar, the language and the lunar switch; it is read at startup and on refresh (`r`), and written as soon as `c` (holiday config) or `g` (language) changes it.
    Without the file the defaults are used (holiday config 1, English, lunar on); an invalid value falls back to the default with a hint in the status bar. `country` from before v1.5.0 is read as a config (CN → 1, JP → 2).
    The key hints in the status bar have three lines: "Move", "Actions", "Settings" (`c` holiday config, `i` import holidays, `g` language, `u` lunar calendar on/off, `e` slogan).
    Holiday configs are fixed slots, not countries (see 5.1): the UI calls them "Holiday config 1 / 2" everywhere (calendar title, status bar, the import and task dialogs, the task overview, task log descriptions and error messages; `HolidayConfigName` / `HolidayConfigShortName`). Tasks and `preference.json` store the config number; the country appears only in the prompt and in the sidebar's "country: holiday" line. The import dialog has a country field, pre-filled from the config's latest data (`HolidayCalendar::LatestCountry`).
16. **v1.4.0 multiple languages**: all fixed text in the program goes through `Tr()`, with the English text as the key; `language/<name>.json` is a flat `{ key: translation }` object.
    - The default language is EN; in `EN.json` every key equals its value. It is the translation template and is generated from the source by `tools/i18n.py` (which scans `Tr("…")` and `N_("…")`). `CN.json` is included.
    - Variable parts use named placeholders `{name}`, so translations can reorder them; missing keys show the English text.
    - The special key `" "` is the word separator (weekday abbreviations, durations), an empty string in Chinese; the translation of the key `"English"` is the language's own name.
    - Text in constant tables (weekdays, months, tab labels) is marked with `N_()` and passed to `Tr()` where used.
    - `g` cycles through the `*.json` files in `language/` in file-name order and writes the file name (e.g. `"CN"`) to `language` in `preference.json`; `"zh-CN"` from v1.3.0 maps to `CN`.
    - Buttons and labels get their text when constructed, so after a language switch `App` leaves the event loop and `RunApp` constructs the whole UI again; `AppState` (data, selected day) is kept.
    - A missing or malformed language file falls back to English with a hint in the status bar. Lunar names, solar terms and holiday names are not translated.
    - Unit tests check that keys equal values in `EN.json` and that `CN.json` covers every key with the same placeholders.
    - English is longer than Chinese: dialogs are wider, form labels are 16 columns, and the status and weekday columns of the task overview are wider.
    - README and the design document each have an English and a Chinese version: `README.md` / `README_CN.md`, `docs/design.md` / `docs/design_CN.md`.
17. **v1.5.0 lunar calendar on/off**: `u` toggles `lunar` in `preference.json` (default true).
    When off, calendar cells show only holiday names on the second line (no lunar dates, lunar festivals or solar terms), and the sidebar shows the slogan (see 18) where the lunar date and festival/solar term lines were.
18. **v1.5.0 slogan**: when the status bar has no message it shows the slogan (`slogan` in `preference.json`, default "Look Forward!", user text, not translated). At startup the "Refreshed" message is cleared so the slogan shows. `e` opens `SloganDialog`; an empty value restores the default.
19. **v1.5.1 task overview layout**: columns are ID / Task / Status / Progress / Done/Total / Created / Due / Weekly / Today's goal.
    - The ID has its own first column (dim, without brackets); the name column no longer contains `(ID:xxxx)`.
    - The name column is as wide as the longest listed name, at most 32 cells. Longer names end with a dim `…`; the selected row scrolls with `<` `>` (`,` `.`), 4 cells per press, reset when the selection or tab changes. Scrolling counts display cells, so full-width characters are never cut in half.
    - While the selected name overflows, a dim `━` thumb is drawn on the separator under the header, above the name column.
    - Progress shows only the percentage (no bar). The selected row has a dark grey background and bold text instead of inverted colours, so each column keeps its colour.
    - "Today" was renamed "Today's goal": it is today's planned quota, which does not drop when work is logged today (see the planning rules).

## 9. Possible future work

- More editing (copying events, editing fixed schedules, keeping task renames in sync)
- Replaying the plan of a past date (replay the log up to that day)
- Clicking a date with the mouse
