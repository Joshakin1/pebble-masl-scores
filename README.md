# MASL Scores

A Pebble watch app that shows Major Arena Soccer League (MASL) games, scores
and division standings.

The phone-side PebbleKit JS component pulls the score ticker and standings
table used by [maslsoccer.com](https://www.maslsoccer.com) (served by
DigitalShift), parses them, and sends compact versions to the watch.

## Features

- Scrollable list of up to 30 games. Each row shows the away team, then the
  home team, then the date and status.
- Team logos next to each team name on color watches (names only on
  black-and-white watches)
- **Standings** screen for the MASL division. Each team shows rank, logo, name
  and points, with games played (GP), W-L-OT, regulation wins (RW) and goal
  differential (GD) underneath. GP shows who has games in hand; RW and GD are
  the main tiebreakers after points (head-to-head isn't shown). The header is
  a key for the numbers, and the last row lists the full tiebreak order:
  Pts > RW > W > H2H > GD > GF > GA. It loads each time it opens.
- Controls:
  - **Select** switches between the schedule and the standings.
  - **Long-press Select** refreshes the current screen.
  - **Back** from the standings also returns to the schedule.
- Auto-refresh of the schedule every 10 minutes
- Settings page in the Pebble phone app (gear icon next to the app):
  - **Selected row color** (color watches only). The text switches between
    black and white automatically to stay readable.
  - **Team filter:** only show games involving the teams you check. Leave every
    team unchecked to show all games.
- Planned, not yet verified: scores, a `LIVE` marker on in-progress games, and
  refreshing every minute while a game is live (see [Status](#status))

## Status

Builds for all five platforms and runs on a watch (confirmed Oct 26, 2026),
including the live request from the phone. All data so far is preseason:
every game is upcoming and every standings stat is 0.

Known issues and open questions:

- **The standings screen has only been checked in the emulator**, and the division ID
  (`50066`) is hard-coded in `STANDINGS_URL`. If MASL changes divisions for a
  new season, take the new ID from the standings page URL on maslsoccer.com.
- **Score and live/final parsing are guesses.** No live or final games have
  appeared in the feed yet. Once the season starts (first game Nov 27, 2026),
  capture a real response and fix the parsing in `parseTicker`.
- **Away/home order is inferred.** The first team listed is treated as away
  and the second as home, based on ticket links.
- **Logos cover the 9 teams in the feed as of Oct 2026.** A team that isn't in
  `s_logos` just shows its name. See [Adding a team logo](#adding-a-team-logo).

Ideas:

- A section header for each date
- An option to show only a favorite team

## Supported platforms

`basalt`, `chalk`, `diorite`, `emery`, `flint` (SDK 3)

## Project layout

| File                     | Purpose                                                |
| ------------------------ | ------------------------------------------------------ |
| `src/c/main.c`           | Watch app: menu UI, logo lookup, AppMessage handling   |
| `src/pkjs/index.js`      | Phone side: fetches and parses the ticker and standings |
| `src/pkjs/config.js`     | Settings page layout ([Clay](https://github.com/pebble-dev/clay)) |
| `src/pkjs/teams.js`      | Team names offered in the settings team filter         |
| `resources/images/`      | Team logos (24×20, Pebble 64-color)                    |
| `tools/convert_logos.py` | Converts downloaded logos into Pebble resources        |
| `package.json`           | Pebble app metadata (UUID, platforms, message keys)    |
| `wscript`                | Pebble SDK build script                                |

The phone sends the watch one line per game:
`away|awayScore|home|homeScore|subtitle`. Scores are empty until a game starts.
Standings are sent the same way, one line per team:
`rank|team|gp|w|l|ot|pts|rw|diff`.
The team filter is applied on the phone before sending and saved in the phone's
`localStorage`. The highlight color is sent to the watch and saved there.

## Building

With the Pebble SDK installed:

```bash
pebble build
pebble install --emulator basalt
```

The settings page needs the `@rebble/clay` package, listed under
`dependencies` in `package.json`. The SDK installs it during the build. In
CloudPebble, it should appear under **Dependencies**; if it doesn't, add
`@rebble/clay` with version `^1.1.0` there. Use this maintained fork of Clay,
not the original `pebble-clay`, which doesn't support `flint` and fails the
build.

## Testing the parser

`src/pkjs/index.js` exports `parseTicker` when run outside the watch
environment, so you can feed it ticker HTML from Node:

```js
const { parseTicker } = require('./src/pkjs/index.js');
console.log(parseTicker(html));
```

## Adding a team logo

1. Save the team's logo as a PNG named after its short name in the ticker, in
   lowercase (for example `comets.png`), in a folder outside the repo. The
   ticker HTML has an `<img>` link to each logo. SVG logos need to be rendered
   to PNG first.
2. Run the converter (requires Pillow):

   ```bash
   python tools/convert_logos.py path/to/logos
   ```

3. Add a `bitmap` entry for `images/logo_<team>.png` to `resources.media` in
   `package.json`, limited to the color platforms like the others.
4. Add the team to `s_logos` in `src/c/main.c`, using the exact name the
   ticker shows.
5. Add the same name to `src/pkjs/teams.js` so it appears in the team filter,
   and update the count in the `FilterTeams[9]` message key in `package.json`
   to match the number of teams.

## Troubleshooting

If the app shows `Error 401` or `Error 403`, the public site ticket embedded
in `src/pkjs/index.js` has probably rotated. Open maslsoccer.com with
DevTools, find the `score-ticker` request, copy the `ticket` value from its
`Authorization` header, and update `TICKET` in `src/pkjs/index.js`.

## Disclaimer

Unofficial fan project for personal use. Not affiliated with or endorsed by
the MASL or DigitalShift. The score feed isn't an official public API, so check
the MASL site's terms before distributing the app.
