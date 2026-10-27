// Settings page, built by Clay (https://github.com/pebble-dev/clay).

var TEAMS = require('./teams');

module.exports = [
  { type: 'heading', defaultValue: 'MASL Scores' },
  {
    type: 'section',
    capabilities: ['COLOR'],
    items: [
      { type: 'heading', defaultValue: 'Appearance' },
      {
        type: 'color',
        messageKey: 'HighlightColor',
        label: 'Selected row color',
        defaultValue: '55AAFF',
        sunlight: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Game times' },
      {
        type: 'select',
        messageKey: 'TimeZone',
        label: 'Show start times in',
        defaultValue: 'eastern',
        options: [
          { label: 'Eastern (as listed)', value: 'eastern' },
          { label: 'My time zone', value: 'local' }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Teams' },
      {
        type: 'checkboxgroup',
        messageKey: 'FilterTeams',
        label: 'Only show games for',
        defaultValue: TEAMS.map(function () { return false; }),
        options: TEAMS
      },
      { type: 'text', defaultValue: 'Leave every team unchecked to show all games.' }
    ]
  },
  { type: 'submit', defaultValue: 'Save' }
];
