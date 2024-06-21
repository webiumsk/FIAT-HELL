// custom access point pages GUI
static const char PAGE_GUI[] PROGMEM = R"(
{
  "uri": "/gui",
  "title": "GUI",
  "menu": false,
  "element": [
    {
      "name": "fundingsource",
      "type": "ACRadio",
      "value": [
        "Blink",
        "LNbits"
      ],
      "label": "Funding source 'blink' or 'lnbits'",
      "arrange": "horizontal",
      "checked": 1
    },
    {
      "name": "enableswitch",
      "type": "ACRadio",
      "value": [
        "No",
        "Yes"
      ],
      "label": "Enable funding source switcher",
      "arrange": "horizontal",
      "checked": 2
    },
    {
      "name": "animated",
      "type": "ACRadio",
      "value": [
        "No",
        "Yes"
      ],
      "label": "Enable animated main title",
      "arrange": "horizontal",
      "checked": 1
    },  
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/gui"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/savegui"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char GUI_SAVE[] PROGMEM = R"(
{
  "uri": "/savegui",
  "title": "GUI",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "GUI have been saved to %s",
      "style": "font-family:Arial;font-size:18px;font-weight:400;color:#191970"
    },
    {
      "name": "validated",
      "type": "ACText",
      "style": "color:red"
    },
    {
      "name": "echo",
      "type": "ACText",
      "style": "font-family:monospace;font-size:small;white-space:pre;"
    },
    {
      "name": "ok",
      "type": "ACSubmit",
      "value": "OK",
      "uri": "/gui"
    }
  ]
}
)";