// custom access point pages Third currency
static const char PAGE_THIRD[] PROGMEM = R"(
{
  "uri": "/third",
  "title": "Third currency",
  "menu": true,
  "element": [
    {
      "name": "currencyThree",
      "type": "ACInput",
      "label": "Third currency",
      "value": ""
    },  
    {
      "name": "lnurl3",
      "type": "ACInput",
      "label": "LNURLDevice",
      "value": ""
    },
    {
      "name": "billmech3",
      "type": "ACInput",      
      "label": "Note values 5,10,20,50,100",
      "value": ""
    },
    {
      "name": "maxamount3",
      "type": "ACInput",
      "apply": "number",
      "label": "Max withdrawable in fiat",
      "value": ""
    },
    {
      "name": "charge3",
      "type": "ACInput",
      "apply": "number",
      "label": "Percentage charge for service",
      "value": ""
    },   
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/third"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/savethird"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char THIRD_SAVE[] PROGMEM = R"(
{
  "uri": "/savethird",
  "title": "Third currency",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "Third currency have been saved to %s",
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
      "uri": "/third"
    }
  ]
}
)";