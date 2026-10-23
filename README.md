# MASL Scores

A Pebble watch app that shows Major Arena Soccer League (MASL) games and scores
in a scrollable list.

The phone-side PebbleKit JS component pulls the score ticker used by
[maslsoccer.com](https://www.maslsoccer.com) (served by DigitalShift), parses
it, and sends a compact list of games to the watch.

## Features

- Scrollable list of up to 30 games: matchup, score when available, date, and
  status
- Live games are marked `LIVE`
- Auto-refresh every minute while a game is live, every 10 minutes otherwise
- Press **Select** to refresh manually

## Supported platforms

`basalt`, `chalk`, `diorite`, `emery`, `flint` (SDK 3)

## Project layout

| File           | Purpose                                                  |
| -------------- | -------------------------------------------------------- |
| `main.c`       | Watch app: menu UI and AppMessage handling               |
| `index.js`     | Phone side: fetches and parses the ticker, sends games   |
| `package.json` | Pebble app metadata (UUID, platforms, message keys)      |
| `wscript`      | Pebble SDK build script                                  |

`wscript` builds from the standard Pebble layout, so the sources need to live
at `src/c/main.c` and `src/pkjs/index.js` for `pebble build` to pick them up.

## Building

With the Pebble SDK installed:

```bash
pebble build
pebble install --emulator basalt
```

## Testing the parser

`index.js` exports `parseTicker` when run outside the watch environment, so
you can feed it ticker HTML from Node:

```js
const { parseTicker } = require('./index.js');
console.log(parseTicker(html));
```

## Troubleshooting

If the app shows `Error 401` or `Error 403`, the public site ticket embedded
in `index.js` has probably rotated. Open maslsoccer.com with DevTools, find the
`score-ticker` request, copy the `ticket` value from its `Authorization`
header, and update `TICKET` in `index.js`.

## Disclaimer

Unofficial fan project. Not affiliated with or endorsed by the MASL or
DigitalShift.
