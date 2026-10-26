// MASL Scores - phone-side component (PebbleKit JS)
// Fetches the MASL score ticker from DigitalShift, parses the HTML partial,
// and sends a compact list of games to the watch. Also fetches the division
// standings on request and runs the settings page.

var TEAMS = require('./teams');

var TICKER_URL = 'https://web.api.digitalshift.ca/partials/score-ticker';
// Standings table for the MASL division, as shown at
// maslsoccer.com/stats#/192/standings?render=division&division_id=50066
var STANDINGS_URL = 'https://web.api.digitalshift.ca/partials/stats/standings/table' +
                    '?division_id=50066&league_toggle=division';
// Public site token embedded in maslsoccer.com. If the app starts showing
// "Error 401/403", grab a fresh ticket from DevTools and paste it here.
var TICKET = 'sBo7AT5RH6DLyYeFtGiJvpXa_JsGl9LIj8dSnDZTEwj5gPgukowAIc9Swg-t9T6dgm0IUwtnCacatO0TAb05PNqb';

var MAX_GAMES = 30;
var LIVE_REFRESH_MS = 60 * 1000;      // while a game is in progress
var IDLE_REFRESH_MS = 10 * 60 * 1000; // otherwise

var MONTHS = {
  January: 'Jan', February: 'Feb', March: 'Mar', April: 'Apr', May: 'May',
  June: 'Jun', July: 'Jul', August: 'Aug', September: 'Sep', October: 'Oct',
  November: 'Nov', December: 'Dec'
};

// localStorage key for the team filter: a JSON array of team names.
var FILTER_KEY = 'teamFilter';

var refreshTimer = null;

function loadTeamFilter() {
  try {
    return JSON.parse(localStorage.getItem(FILTER_KEY)) || [];
  } catch (e) {
    return [];
  }
}

// Keep games involving any of the given teams. An empty filter keeps all.
function filterGames(games, teams) {
  if (!teams.length) return games;
  return games.filter(function (g) {
    return teams.indexOf(g.away) !== -1 || teams.indexOf(g.home) !== -1;
  });
}

function decodeEntities(s) {
  return s.replace(/&amp;/g, '&').replace(/&#39;/g, "'").replace(/&quot;/g, '"')
          .replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&nbsp;/g, ' ');
}

function stripTags(s) {
  return decodeEntities(s.replace(/<[^>]*>/g, ' ')).replace(/\s+/g, ' ').trim();
}

// Remove characters used as separators in the watch message.
function clean(s) {
  return (s || '').replace(/[|\n]/g, ' ').trim();
}

function parseTeam(chunk) {
  var nameMatch = chunk.match(/<img[^>]*>([^<]*)/);
  var name = nameMatch ? decodeEntities(nameMatch[1]).trim() : '';
  if (!name) name = stripTags(chunk).split(' ')[0] || '?';
  // Scores aren't in the feed yet (preseason). Look for any element whose
  // class contains "score" followed by a number inside this team's block.
  var scoreMatch = chunk.match(/class="[^"]*score[^"]*"[^>]*>\s*(?:<[^>]*>\s*)*(\d+)/);
  return { name: name, score: scoreMatch ? scoreMatch[1] : null };
}

function parseTicker(html) {
  var games = [];
  var items = html.split('<li class="item"');
  for (var i = 1; i < items.length && games.length < MAX_GAMES; i++) {
    var c = items[i];

    var label = (c.match(/aria-label="([^"]*)"/) || [])[1] || '';
    var dm = label.match(/^(\w+) (\d+), \d{4} at /);
    var date = dm ? (MONTHS[dm[1]] || dm[1].slice(0, 3)) + ' ' + dm[2] : '';

    // Split on the exact class "team" (not "team-area" / "teams").
    var teamChunks = c.split('<div class="team"').slice(1, 3);
    if (teamChunks.length < 2) continue;
    // Each chunk should stop before the status block.
    teamChunks[1] = teamChunks[1].split('<div class="status')[0];
    var away = parseTeam(teamChunks[0]);
    var home = parseTeam(teamChunks[1]);

    var statusMatch = c.match(/<div class="status[^"]*"[^>]*>([\s\S]*?)<\/div>/);
    var status = statusMatch ? stripTags(statusMatch[1]) : '';
    var scheduled = /^\d{1,2}:\d{2}\s*[ap]m/i.test(status);
    var isFinal = /final/i.test(status);
    var live = !scheduled && !isFinal && status.length > 0;

    var subtitle = (live ? 'LIVE ' : '') + (date ? date + ' ' : '') + status;

    games.push({
      away: clean(away.name), awayScore: away.score || '',
      home: clean(home.name), homeScore: home.score || '',
      subtitle: clean(subtitle), live: live
    });
  }
  return games;
}

// The partial renders the table twice (screen and print); use the first.
// Columns are found by header text, so reordering on the site doesn't matter.
function parseStandings(html) {
  var table = (html.split('<table')[1] || '').split('</table>')[0];
  var headers = (table.match(/<th[^>]*>[\s\S]*?<\/th>/g) || []).map(stripTags);
  var col = function (name) { return headers.indexOf(name); };
  var rankCol = col('Rk'), teamCol = col('Team'), wCol = col('W'), lCol = col('L'),
      otCol = col('OT'), ptsCol = col('Pts'), diffCol = col('Diff'),
      streakCol = col('Streak');
  if (teamCol === -1 || ptsCol === -1) return [];

  var body = table.split('<tbody')[1] || '';
  var rows = [];
  body.split('<tr').slice(1).forEach(function (tr) {
    var cells = tr.split('<td').slice(1).map(function (c) {
      return c.replace(/^[^>]*>/, '');
    });
    if (cells.length <= ptsCol) return;
    // The team cell has the full name ("St. Louis Ambush") for wide screens
    // and the short name ("Ambush") in span.p; use the short one.
    var shortName = cells[teamCol].match(/<span class="p">([\s\S]*?)<\/span>/);
    var text = function (i) { return i === -1 ? '' : clean(stripTags(cells[i])); };
    rows.push({
      rank: rankCol === -1 ? String(rows.length + 1) : text(rankCol),
      team: clean(stripTags(shortName ? shortName[1] : cells[teamCol])),
      w: text(wCol), l: text(lCol), ot: text(otCol), pts: text(ptsCol),
      diff: text(diffCol), streak: text(streakCol)
    });
  });
  return rows;
}

function sendStatus(text) {
  Pebble.sendAppMessage({ Status: text }, null, function () {
    console.log('Failed to send status');
  });
}

function scheduleRefresh(anyLive) {
  if (refreshTimer) clearTimeout(refreshTimer);
  refreshTimer = setTimeout(fetchScores, anyLive ? LIVE_REFRESH_MS : IDLE_REFRESH_MS);
}

// GET a DigitalShift partial and pass its HTML content to onContent, or a
// short error message for the watch to onError.
function getPartial(url, onContent, onError) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.setRequestHeader('Accept', 'application/json, text/plain, */*');
  xhr.setRequestHeader('Authorization', 'ticket="' + TICKET + '"');
  // Some environments ignore these; harmless if so.
  try {
    xhr.setRequestHeader('Origin', 'https://www.maslsoccer.com');
    xhr.setRequestHeader('Referer', 'https://www.maslsoccer.com/');
  } catch (e) { /* ignored */ }
  xhr.timeout = 15000;

  xhr.onload = function () {
    if (xhr.status !== 200) {
      onError('Error ' + xhr.status);
      return;
    }
    var content;
    try {
      content = JSON.parse(xhr.responseText).content || '';
    } catch (e) {
      console.log('Bad response from ' + url + ': ' + e);
      onError('Could not read data');
      return;
    }
    onContent(content);
  };
  xhr.onerror = function () { onError('No connection'); };
  xhr.ontimeout = function () { onError('Timed out'); };
  xhr.send();
}

function fetchScores() {
  getPartial(TICKER_URL, function (html) {
    var games;
    try {
      games = parseTicker(html);
    } catch (e) {
      console.log('Parse error: ' + e);
      sendStatus('Could not read scores');
      scheduleRefresh(false);
      return;
    }
    if (games.length === 0) {
      sendStatus('No games listed');
      scheduleRefresh(false);
      return;
    }
    games = filterGames(games, loadTeamFilter());
    if (games.length === 0) {
      sendStatus('No games for your teams');
      scheduleRefresh(false);
      return;
    }
    // One game per line: away|awayScore|home|homeScore|subtitle
    var payload = games.map(function (g) {
      return [g.away, g.awayScore, g.home, g.homeScore, g.subtitle].join('|');
    }).join('\n');
    Pebble.sendAppMessage({ Games: payload }, null, function () {
      console.log('Failed to send games');
    });
    scheduleRefresh(games.some(function (g) { return g.live; }));
  }, function (message) {
    sendStatus(message);
    scheduleRefresh(false);
  });
}

function sendStandingsStatus(text) {
  Pebble.sendAppMessage({ StandingsStatus: text }, null, function () {
    console.log('Failed to send standings status');
  });
}

// Fetched only when the watch opens the standings screen.
function fetchStandings() {
  getPartial(STANDINGS_URL, function (html) {
    var rows;
    try {
      rows = parseStandings(html);
    } catch (e) {
      console.log('Standings parse error: ' + e);
      sendStandingsStatus('Could not read standings');
      return;
    }
    if (rows.length === 0) {
      sendStandingsStatus('No standings listed');
      return;
    }
    // One team per line: rank|team|w|l|ot|pts|diff|streak
    var payload = rows.map(function (r) {
      return [r.rank, r.team, r.w, r.l, r.ot, r.pts, r.diff, r.streak].join('|');
    }).join('\n');
    Pebble.sendAppMessage({ Standings: payload }, null, function () {
      console.log('Failed to send standings');
    });
  }, sendStandingsStatus);
}

if (typeof Pebble !== 'undefined') {
  var Clay = require('@rebble/clay');
  // Events are handled here so the team filter can stay on the phone; only
  // the highlight color goes to the watch.
  var clay = new Clay(require('./config'), null, { autoHandleEvents: false });

  Pebble.addEventListener('ready', fetchScores);
  Pebble.addEventListener('appmessage', function (e) {
    if (e.payload && e.payload.Refresh) fetchScores();
    if (e.payload && e.payload.GetStandings) fetchStandings();
  });

  Pebble.addEventListener('showConfiguration', function () {
    Pebble.openURL(clay.generateUrl());
  });

  Pebble.addEventListener('webviewclosed', function (e) {
    if (!e || !e.response) return;
    var settings = clay.getSettings(e.response, false);

    var checked = (settings.FilterTeams && settings.FilterTeams.value) || [];
    var teams = TEAMS.filter(function (t, i) { return checked[i]; });
    localStorage.setItem(FILTER_KEY, JSON.stringify(teams));

    if (settings.HighlightColor) {
      Pebble.sendAppMessage({ HighlightColor: settings.HighlightColor.value }, null, function () {
        console.log('Failed to send highlight color');
      });
    }
    fetchScores();
  });
}

// Exported only so the parser can be tested outside the watch environment.
if (typeof module !== 'undefined' && typeof Pebble === 'undefined') {
  module.exports = { parseTicker: parseTicker, filterGames: filterGames,
                     parseStandings: parseStandings };
}
