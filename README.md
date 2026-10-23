# MASL Scores

A Pebble watch app that shows Major Arena Soccer League (MASL) games and scores
in a scrollable list.

The phone-side PebbleKit JS component pulls the score ticker used by
[maslsoccer.com](https://www.maslsoccer.com) (served by DigitalShift), parses
it, and sends a compact list of games to the watch.

## Features

- Scrollable list of up to 30 games. Each row shows the away team, then the
  home team, then the date and status.
- Team logos next to each team name on color watches (names only on
  black-and-white watches)
- Press **Select** to refresh manually
- Auto-refresh every 10 minutes
- Planned, not yet verified: scores, a `LIVE` marker on in-progress games, and
  refreshing every minute while a game is live (see [Status](#status))

## Status

Early and untested on hardware. The ticker parser has only been tested in
Node, against a preseason response in which every game was upcoming.

Known issues and open questions:

- **Not yet built or run on a device.** The first `pebble build` and
  emulator/watch test are still to do. If the SDK doesn't recognize `emery` or
  `flint`, remove them from `targetPlatforms`. Aplite is left out on purpose
  because of memory limits.
- **The live request hasn't been tested from a phone.** On Oct 23, 2026 the
  feed returned 200 from a desktop without the `Origin`/`Referer` headers the
  MASL site sends, so PebbleKit JS shouldn't need them. If the phone still gets
  `Error 401`/`403` with a fresh ticket, the fallback is a small proxy that adds
  the headers, parses the HTML, and returns clean JSON.
- **Score and live/final parsing are guesses.** No live or final games have
  appeared in the feed yet. Once the season starts (first game Nov 27, 2026),
  capture a real response and fix the parsing in `parseTicker`.
- **Away/home order is inferred.** The first team listed is treated as away
  and the second as home, based on ticket links.
- **Logos cover the 9 teams in the feed as of Oct 2026.** A team that isn't in
  `s_logos` just shows its name. See [Adding a team logo](#adding-a-team-logo).
- **Some logos are hard to see on the selected row.** Dark logos such as the
  Blast wordmark blend into the dark green highlight.

Ideas:

- A section header for each date
- An option to show only a favorite team

## Supported platforms

`basalt`, `chalk`, `diorite`, `emery`, `flint` (SDK 3)

## Project layout

| File                     | Purpose                                                |
| ------------------------ | ------------------------------------------------------ |
| `src/c/main.c`           | Watch app: menu UI, logo lookup, AppMessage handling   |
| `src/pkjs/index.js`      | Phone side: fetches and parses the ticker, sends games |
| `resources/images/`      | Team logos (24×20, Pebble 64-color)                    |
| `tools/convert_logos.py` | Converts downloaded logos into Pebble resources        |
| `package.json`           | Pebble app metadata (UUID, platforms, message keys)    |
| `wscript`                | Pebble SDK build script                                |

The phone sends the watch one line per game:
`away|awayScore|home|homeScore|subtitle`. Scores are empty until a game starts.

## Building

With the Pebble SDK installed:

```bash
pebble build
pebble install --emulator basalt
```

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

## Troubleshooting

If the app shows `Error 401` or `Error 403`, the public site ticket embedded
in `src/pkjs/index.js` has probably rotated. Open maslsoccer.com with
DevTools, find the `score-ticker` request, copy the `ticket` value from its
`Authorization` header, and update `TICKET` in `src/pkjs/index.js`.

## Disclaimer

Unofficial fan project for personal use. Not affiliated with or endorsed by
the MASL or DigitalShift. The score feed isn't an official public API, so check
the MASL site's terms before distributing the app.
