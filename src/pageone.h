// custom access point pages
static const char PAGE_ELEMENTS[] PROGMEM = R"(
{
  "uri": "/config",
  "title": "ATM Config",
  "menu": true,
  "element": [
    {
      "name": "text",
      "type": "ACText",
      "value": "AP options",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },    
    {
      "name": "password",
      "type": "ACInput",
      "label": "Password",
      "value": "changeme"
    },        
    {
      "name": "textwifi",
      "type": "ACText",
      "value": "WiFi",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;padding-bottom:8px;"
    },
    {
      "name": "localssid",
      "type": "ACInput",
      "label": "SSID",
      "value": ""
    },
    {
      "name": "localpass",
      "type": "ACInput",
      "label": "Passwd",
      "value": ""
    },
    {
      "name": "newline",
      "type": "ACElement",
      "value": "<hr>"
    },
    {
      "name": "texttext",
      "type": "ACText",
      "value": "Texts",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;padding-bottom:8px;width:100%;"
    },
    {
      "name": "atmdesc",
      "type": "ACInput",
      "label": "Description",
      "value": ""
    },
    {
      "name": "atmsubtitle",
      "type": "ACInput",
      "label": "Subtitle",
      "value": ""
    },
    {
      "name": "atmtitle",
      "type": "ACInput",
      "label": "Main Title",
      "value": ""
    },
    {
      "name": "newline1",
      "type": "ACElement",
      "value": "<hr>"
    },
    {
      "name": "pincode",
      "type": "ACInput",
      "apply": "number",
      "label": "PIN: 4 digits",      
      "value": "1111"
    },  
    {
      "name": "load",
      "type": "ACSubmit",
      "value": "Load",
      "uri": "/config"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/save"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char PAGE_SAVE[] PROGMEM = R"(
{
  "uri": "/save",
  "title": "Elements",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "Elements have been saved to %s",
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
      "uri": "/config"
    }
  ]
}
)";