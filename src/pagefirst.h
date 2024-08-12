// custom access point pages
static const char PAGE_FIRST[] PROGMEM = R"(
{
  "uri": "/first",
  "title": "First currency",
  "menu": true,
  "element": [    
    {
      "name": "textonlineblink",
      "type": "ACText",
      "value": "Blink settings",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;padding-bottom:8px;"
    },
    {
      "name": "blinkapikey",
      "type": "ACInput",
      "label": "API key",
      "value": ""
    },
    {
      "name": "blinkwalletid",
      "type": "ACInput",
      "label": "Wallet ID",
      "value": ""
    },
    {
      "name": "newline",
      "type": "ACElement",
      "value": "<hr>"
    },
    {
      "name": "textonline",
      "type": "ACText",
      "value": "LNbits settings",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;padding-bottom:8px;"
    },
    {
      "name": "lnurl",
      "type": "ACInput",
      "label": "LNURLDevice",
      "value": ""
    },
    {
      "name": "adminkey",
      "type": "ACInput",
      "label": "Admin key",
      "value": ""
    },
    {
      "name": "readkey",
      "type": "ACInput",
      "label": "Invoice/read ",
      "value": ""
    },
    {
      "name": "newline2",
      "type": "ACElement",
      "value": "<hr>"
    },
    {
      "name": "currencyOne",
      "type": "ACInput",
      "label": "Main Currency",
      "value": ""
    },   
    {
      "name": "billmech",
      "type": "ACInput",      
      "label": "Note values 5,10,20,50,100,200",
      "value": ""
    },
    {
      "name": "maxamount",
      "type": "ACInput",
      "apply": "number",
      "label": "Max withdrawable in fiat",
      "value": ""
    },
    {
      "name": "charge1",
      "type": "ACInput",
      "apply": "number",
      "label": "Percentage charge for service",
      "value": ""
    },
    {
      "name": "newline1",
      "type": "ACElement",
      "value": "<hr>"
    },    
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/first"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/savefirst"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char FIRST_SAVE[] PROGMEM = R"(
{
  "uri": "/savefirst",
  "title": "First currency",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "First currency have been saved to %s",
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
      "uri": "/first"
    }
  ]
}
)";