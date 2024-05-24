// custom access point page Second Currency
static const char PAGE_SECOND[] PROGMEM = R"(
{
  "uri": "/second",
  "title": "Second currency",
  "menu": true,
  "element": [    
    {
      "name": "currencyTwo",
      "type": "ACInput",      
      "label": "Second currency",
      "value": ""
    },
    {
      "name": "lnurl2",
      "type": "ACInput",
      "label": "LNURLDevice",
      "value": ""
    },  
    {
      "name": "billmech2",
      "type": "ACInput",      
      "label": "Note values 5,10,20,50,100",
      "value": ""
    },
    {
      "name": "maxamount2",
      "type": "ACInput",
      "apply": "number",
      "label": "Max withdrawable in fiat",
      "value": ""
    },
    {
      "name": "charge2",
      "type": "ACInput",
      "apply": "number",
      "label": "Percentage charge for service",
      "value": ""
    },   
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/second"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/savesecond"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char SECOND_SAVE[] PROGMEM = R"(
{
  "uri": "/savesecond",
  "title": "Second currency",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "Second currency have been saved to %s",
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
      "uri": "/second"
    }
  ]
}
)";