var miniCompOptions = [
  {"label": "None", "value": 0},
  {"label": "Date", "value": 1},
  {"label": "Step Count", "value": 2},
  {"label": "Battery Life", "value": 3},
  {"label": "Year", "value": 4},
  {"label": "Sunset", "value": 5},
  {"label": "Sunrise", "value": 6},
  {"label": "Month", "value": 7},
  {"label": "UV Index", "value": 8},
  {"label": "Week Number", "value": 9}
];

var bottomCompOptions = [
  {"label": "None", "value": 0},
  {"label": "High / Low", "value": 1},
  {"label": "Weather", "value": 2},
  {"label": "Sunset", "value": 3},
  {"label": "Sunrise", "value": 4},
  {"label": "Step Count", "value": 5},
  {"label": "Week Number", "value": 6},
  {"label": "UV Index", "value": 7}
];

var bottomCompPrimaryOptions = [
  {"label": "High / Low", "value": 1},
  {"label": "Weather", "value": 2},
  {"label": "Sunset", "value": 3},
  {"label": "Sunrise", "value": 4},
  {"label": "Step Count", "value": 5},
  {"label": "Week Number", "value": 6},
  {"label": "UV Index", "value": 7}
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
        "label": "Color",
        "defaultValue": "0x000000"
      },
      {
        "type": "select",
        "messageKey": "Canvas",
        "label": "Canvas",
        "defaultValue": 0,
        "options": [
          {"label": "Paper", "value": 0},
          {"label": "Ink", "value": 1}
        ]
      },
      {
        "type": "select",
        "messageKey": "TempUnit",
        "label": "Temperature Unit",
        "defaultValue": 0,
        "options": [
          {"label": "Fahrenheit (°F)", "value": 0},
          {"label": "Celsius (°C)", "value": 1}
        ]
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
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Bottom Complications"
      },
      {
        "type": "select",
        "messageKey": "BottomCompLeft",
        "label": "Left",
        "defaultValue": 1,
        "options": bottomCompOptions
      },
      {
        "type": "select",
        "messageKey": "BottomCompPrimary",
        "label": "Primary",
        "defaultValue": 2,
        "options": bottomCompPrimaryOptions
      },
      {
        "type": "select",
        "messageKey": "BottomCompRight",
        "label": "Right",
        "defaultValue": 3,
        "options": bottomCompOptions
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
