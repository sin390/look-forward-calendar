# LookForward Calendar (LFCalendar) v1.5.0

English | [中文](README_CN.md)

![LookForward logo](docs/logo.png)

> A sun rising over a calendar page, and one highlighted day in the date grid: looking forward to every day ahead. The same pixel data is used as the exe icon; press `v` in the program to see the terminal version.

LookForward Calendar (LFCalendar for short) is a terminal calendar for Windows built with [FTXUI](https://github.com/ArthurSonzogni/FTXUI). It shows a month view with the Chinese lunar calendar and solar terms, Chinese and Japanese public holidays, events with reminders, recurring fixed schedules, and long-term tasks whose remaining work is spread automatically over the workdays.

## Features

- **Month view**: weeks start on Sunday; shows lunar dates, lunar festivals, the 24 solar terms, and holidays (off / workday)
- **Events**: an appointment at a given time on one day, with an optional reminder
- **Fixed schedules**: work that repeats on a cycle (weekly by default) at a fixed time, such as a meeting every Tuesday and Thursday
- **Long-term tasks**: set the total hours and a due date; the remaining work is divided evenly over the workdays, you log what you actually did each day, and the plan adjusts itself
- **Holidays**: get a country's holidays for a year from an AI and import them as local data
- **Task overview and reminder list**: full-screen subscreens
- **Multiple languages**: all UI text comes from language files in `language/`; English by default, Chinese included, press `g` to switch

## Quick start (release package)

Download `LookForward-v<version>-win64.zip`, unzip it anywhere and double-click `LookForward.exe` (or run it from Windows Terminal / PowerShell / cmd). Nothing needs to be installed.

```
LookForward-v1.5.0-win64/
  LookForward.exe        the program (statically linked, no DLLs needed)
  language/              language files: EN.json (default), CN.json; must stay next to the exe
  README.md, README_CN.md
  docs/logo.png          the image used in the READMEs
```

- On first use a `data/` folder is created next to the exe for your events, tasks, holidays and settings. To upgrade, unzip the new version and copy your old `data/` folder next to the new exe.
- The UI starts in English; press `g` to switch to Chinese.
- Use a window at least 120 columns wide.

## Building and running

Requirements: GCC 13 or later (C++23), CMake 3.20 or later, Ninja, Git, and a network connection for the first build (to download FTXUI and nlohmann/json).

```bash
./build.sh          # configure + build + run unit tests
./build.sh run      # same, then start the program in a new console window
./build.sh clean    # delete build/ and rebuild
./build.sh package  # release build in build-release/, packed into dist/LookForward-v<version>-win64.zip
```

The result is `build/LookForward.exe`; the language files are copied to `build/language/` at the same time. When distributing, keep the `language/` folder next to the exe; `./build.sh package` produces a ready-to-use zip with the release build (no debug symbols, about 3 MB) laid out as shown in Quick start.

- Run it in **Windows Terminal, PowerShell or cmd**. The Git Bash terminal does not deliver key presses correctly, so `./build.sh run` opens a separate window.
- A window at least **120 columns** wide is recommended.
- The compiler path is set in `CMakePresets.json`; adjust it on another machine.

## Screen layout

```
┌ Calendar ──────────────────────────┐┌ Sidebar (selected day) ┐
│ Su  Mo  Tu  We  Th  Fr  Sa          ││ Lunar date, holidays   │
│ 3*                                  ││ [Events]   [Edit][Del] │
│ 廿五     ← lunar date/festival/term ││ [Tasks]                │
│ ●1 ▶3:40 ← events / work hours      ││ Task name              │
│                                     ││ (ID:xxxx)       Active │
│                                     ││  Plan 2:40 Actual 1:30 │
│                                     ││ [Log][Pause][Modify]…  │
└─────────────────────────────────────┘└────────────────────────┘
 Key hints (Move / Actions / Settings)    ⏰ Reminders: N (m to view)
```

Third line of a day cell: `●n` means n events that day; `◆n` means n fixed schedules; `▶h:mm` is the planned work for today and later; `✓h:mm` is the actual work on past days; a red `!` means a task is overdue.

## Keys

Only keys found on every keyboard are used.


| Where         | Key                     | Action                                                                                           |
| ------------- | ----------------------- | ------------------------------------------------------------------------------------------------ |
| Calendar      | arrows /`h` `j` `k` `l` | move the selected day                                                                            |
|               | `[` `]`                 | previous / next month                                                                            |
|               | `t`                     | back to today                                                                                    |
|               | `Tab`                   | go to the sidebar                                                                                |
|               | `a`                     | new event on the selected day                                                                    |
|               | `f`                     | new fixed schedule starting on the selected day                                                  |
|               | `n`                     | new long-term task                                                                               |
|               | `o`                     | task overview                                                                                    |
|               | `m`                     | reminder list                                                                                    |
|               | `i`                     | import holidays                                                                                  |
|               | `c`                     | switch the holidays shown on the calendar between holiday config 1 and 2; saved automatically    |
|               | `g`                     | switch the UI language to the next file in`language/` (e.g. CN → EN → CN); saved automatically |
|               | `u`                     | show / hide the lunar calendar; saved automatically (see below)                                  |
|               | `e`                     | edit the slogan shown in the status bar (see below)                                              |
|               | `r`                     | refresh (re-read the data files)                                                                 |
|               | `v`                     | version information                                                                              |
|               | `q`                     | quit                                                                                             |
| Sidebar       | arrows /`h` `j` `k` `l` | move between buttons                                                                             |
|               | `Enter`                 | press the button                                                                                 |
|               | `Tab` / `Esc`           | back to the calendar                                                                             |
| Dialogs       | `Tab`                   | next field                                                                                       |
|               | `Space` / `Enter`       | toggle a checkbox, press a button                                                                |
|               | `Esc`                   | close                                                                                            |
| Task overview | `1` / `2` / `3`         | Active (by due date) / All (by creation date) / Completed (by final due date)                    |
|               | `j` `k`                 | move up / down                                                                                   |
|               | `Enter`                 | show the full log of the selected task (in chronological order);`Esc` goes back                  |
|               | `d`                     | delete everything about the selected task (press`y` to confirm, any other key cancels)           |
|               | `Esc` / `q` / `o`       | back                                                                                             |
| Reminder list | `j` `k`                 | move up / down                                                                                   |
|               | `Enter`                 | jump to the event's date                                                                         |
|               | `Esc` / `q` / `m`       | back                                                                                             |

## Events and reminders

Press `a` on the calendar, or "+ New event" in the sidebar, and enter a title, start and end time (optional, `HH:MM`) and a note.

Tick "Remind" and enter a lead time: `30m` (30 minutes), `1:30` (1 hour 30 minutes), `2` (2 hours) or `1d` (1 day), up to 30 days.

- The reminder period starts at "start time − lead time" and lasts until the event ends; for an event without an end time, until its start minute.
- An all-day event (no time) starts at 0:00 that day, and its reminder lasts until the end of the day.
- Events in their reminder period appear in the reminder list (`m`), sorted by time. The program refreshes itself every 30 seconds.

## Fixed schedules

Work that repeats on a cycle at a fixed time, such as meetings, classes or routine work. Press `f` on the calendar, or "+ New fixed schedule" in the sidebar, and fill in:


| Field                 | Notes                                                                                 |
| --------------------- | ------------------------------------------------------------------------------------- |
| Work content          | required                                                                              |
| Start time / End time | required,`HH:MM`                                                                      |
| Repeat                | Daily / Weekly (default) / Biweekly / Monthly                                         |
| Days                  | for Weekly and Biweekly, which days of the week; defaults to the start date's weekday |
| Start date            | defaults to the selected day                                                          |
| End date              | optional; empty means it repeats forever                                              |
| Note                  | optional                                                                              |

- Biweekly: every other week, counted from the week of the start date.
- Monthly: on the start date's day of the month; months without that day (e.g. the 31st) are skipped.
- The days of a month that have fixed schedules are worked out only when the calendar shows that month.
- In the sidebar each fixed schedule shows its time, work content, repeat rule and note. `[Del]` deletes every date of the schedule, so it needs two presses: the first turns it into `[Confirm delete]`, the second deletes.

## Long-term tasks

Press `n` to create a task:


| Field           | Notes                                       |
| --------------- | ------------------------------------------- |
| Task name       | required                                    |
| Planned hours   | hours, e.g.`40` or `12:30`                  |
| Due date        | `YYYY-MM-DD`                                |
| Workdays        | which days of the week you work on it       |
| Skip holidays   | when ticked, no work is planned on days off |
| Holiday config  | whose holidays are skipped (config 1 / 2)   |

Each task gets a random ID (e.g. `ID:a3f9`). In the sidebar the first line of a task is its name and the second its ID and status, with a blank line between tasks. When the content is taller than the screen a scrollbar appears on the right; scroll with the arrow keys or the mouse wheel. In the task overview, `Enter` shows the task's full log and `d` deletes the task (its file is deleted and cannot be recovered).

**How work is planned**

- Remaining = planned hours − work actually done before today. The remaining work is divided evenly over the workdays from today to the due date, each day rounded up to 10 minutes.
- The creation day counts as a workday.
- Today's plan only uses work done before today, so what you log today does not change today's target, only later days.
- A day with nothing logged counts as 0, and the following days get more.
- A task past its due date and not finished is shown in red and treated as paused; it resumes when you move the due date.

**Task buttons in the sidebar** (on the last line of each task, left to right)


| Button                 | What it does                                                                                   |
| ---------------------- | ---------------------------------------------------------------------------------------------- |
| `[Log]`                | enter the actual time and a note for the day; can be changed any time. Future dates are locked |
| `[Pause]` / `[Resume]` | no work is planned while paused; the due date stays the same                                   |
| `[Modify]`             | change the hours, due date, workdays and so on; the plan is recalculated                       |
| `[Abort]` / `[Done]`   | end the task                                                                                   |

A task that skips holidays may run into a year whose holidays have not been imported yet; days in that year are planned as ordinary workdays. After you import that year (`i`) or refresh (`r`), the holidays are read again and every plan is recalculated.

## Importing holidays

1. Press `i`, choose the holiday config (1 / 2), enter the country and the year, and press "Generate & copy prompt"; the prompt is copied to the clipboard.
2. Paste the prompt to any AI (such as Claude). It returns the JSON in a code block; an AI that can create files also gives you `config<N>_<year>.json` to download. Otherwise save the code block to a file (keeping the text around the code block is fine).
3. Enter the file's path under "File path" (a quoted path works too) and press "Import".

The program checks the format and that the year matches your choice, then saves the data to `data/holidays/config<N>_<year>.json`, replacing any existing data for the same holiday config and year.

Holiday configs 1 and 2 are slots that are not tied to any country: you can import the holidays of any country into either one. The country is just a property of the config (the `country` field of the JSON, taken from what you entered for the prompt). It is used only by the prompt and by the sidebar, which shows a day's holidays as "country: holiday", e.g. "中国: 国庆节 (off)". When the dialog opens, the country field is filled with the country already stored for that config.

The prompt follows the UI language: English with EN, Chinese with CN.

## Lunar calendar on/off

`u` shows or hides the lunar calendar: lunar dates, lunar festivals and solar terms. When hidden, calendar cells show only holiday names on the second line, and the sidebar shows the slogan (see below) where the lunar date was. The setting is saved in `data/preference.json`; the lunar calendar is shown by default.

## Slogan

When there is no other message, the status bar shows a slogan, "Look Forward!" by default. Press `e` to change it; leave it empty to restore the default. The slogan is saved in `data/preference.json` and is not translated.

## Languages

All fixed UI text comes from the language files in `language/` next to the exe. The default language is English (EN); Chinese (CN) is included.

- `g` switches to the next language file in `language/` (in file-name order, e.g. CN → EN → CN). The change takes effect at once and is saved in `data/preference.json`, so the next start uses it too.
- A language file is a JSON object of the form `{ "English text": "translation" }`. In `EN.json` every key equals its value; it is the template for translations.
- **Adding a language**: copy `EN.json`, rename it to `<name>.json` (e.g. `JA.json`), translate every value, and put it in `language/`. No rebuild is needed.
- `{name}` in a text is a placeholder that is replaced with a value (a date, a name, ...). Keep placeholders in the translation; you may move them, e.g. `"Task created: {name}"` → `"已创建任务：{name}"`.
- The special key `" "` (a single space) is the separator between words, used for weekday abbreviations (`Mo We`), durations (`1 h 30 min`) and similar; Chinese translates it to an empty string.
- The value of the key `"English"` is the language's own name ("中文" in the Chinese file); it is shown in the status bar when switching.
- Entries missing from a language file are shown in English. Lunar months, lunar dates, solar terms and holiday names are not translated.

After changing UI text in the code, run `python tools/i18n.py` to regenerate `EN.json` and list the entries missing from the other language files; the unit tests also check that `CN.json` is complete.

## Data files

Data is stored in `data/` next to the exe; use `LookForward.exe --data <dir>` for another location. All files are UTF-8 JSON and can be viewed or edited in a text editor; press `r` in the program after editing.

```
data/
  general/<year>/<month>.json    events of each day, e.g. general/2026/11.json
  task/<created>_<ID>.json       one file per task, containing its operation log
  fixed/<created>_<ID>.json      one file per fixed schedule
  preference.json                settings: holiday config shown on the calendar (1 / 2), language (language file name, e.g. "CN") whether the lunar calendar is shown, and the slogan; read at startup
  holidays/config<N>_<year>.json imported holidays of a holiday config, e.g. holidays/config1_2027.json
```

A task file records every operation as a log: create, modify, progress, pause, resume, complete, abort, each with its time.

The language files are in `language/` next to the exe (not in `data/`): `EN.json`, `CN.json` and so on.

## Development

- Design document: [docs/design.md](docs/design.md) (Chinese: [docs/design_CN.md](docs/design_CN.md))
- UI text: `python tools/i18n.py` (generates `language/EN.json`, checks the other language files)
- Unit tests: `build/calendar_tests.exe` (run automatically by `build.sh`)
- UI snapshot (for debugging): `LookForward.exe --data <dir> --snapshot 150x40 "TAB,j,ENTER"` feeds these keys and prints the screen as text.

## About

v1.5.0 (2026-11-03). Written by Claude (Anthropic) based on requirements and design discussions with ZXHAN, using Claude Opus 5.5 (claude-opus-5-5).
