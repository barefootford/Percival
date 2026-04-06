var miniCompOptions = [
  {"label": "None", "value": 0},
  {"label": "Date", "value": 1},
  {"label": "Step Count", "value": 2},
  {"label": "Battery Life", "value": 3},
  {"label": "Day of Week", "value": 4},
  {"label": "Sunset", "value": 5},
  {"label": "Sunrise", "value": 6}
];

module.exports = [
  {
    "type": "heading",
    "defaultValue": "Percival Settings"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "color",
        "messageKey": "PrimaryColor",
        "label": "Primary Color",
        "defaultValue": "0x000000"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Top Bar Complications"
      },
      {
        "type": "select",
        "messageKey": "MiniCompLeft",
        "label": "Left",
        "defaultValue": 1,
        "options": miniCompOptions
      },
      {
        "type": "select",
        "messageKey": "MiniCompMiddle",
        "label": "Center",
        "defaultValue": 2,
        "options": miniCompOptions
      },
      {
        "type": "select",
        "messageKey": "MiniCompRight",
        "label": "Right",
        "defaultValue": 3,
        "options": miniCompOptions
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
