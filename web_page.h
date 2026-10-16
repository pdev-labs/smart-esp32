// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 pdev-labs — part of smart-esp32, see LICENSE.
#pragma once
#include <pgmspace.h>

// Lightweight UI for old phones (Oppo A37fw, Android 5). ES5 JS, XHR (no fetch).
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP Smart Switch</title>
<style>
body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:12px;text-align:center}
h2{margin:8px 0} .sub{color:#aaa;font-size:13px;margin-bottom:10px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:420px;margin:0 auto}
.card{background:#222;border-radius:10px;padding:14px}
.card h3{margin:0 0 8px;text-transform:capitalize}
.st{font-weight:bold;margin-bottom:8px}.on{color:#4caf50}.off{color:#f44336}
button{font-size:17px;padding:12px 16px;border:0;border-radius:8px;margin:2px}
.tgl{background:#2196f3;color:#fff;width:100%}
.mic{background:#ff9800;color:#000;width:100%;max-width:420px;margin-top:12px}
.send{background:#4caf50;color:#fff}
input{font-size:17px;padding:10px;width:92%;max-width:380px;border-radius:8px;border:1px solid #555}
#heard{background:#222;border-radius:8px;padding:10px;margin:10px auto;max-width:420px;min-height:20px;font-size:15px}
.note{color:#aaa;font-size:12px;margin-top:10px}
</style>
</head>
<body>
<h2>ESP Smart Switch</h2>
<div class="sub">Say or tap: "esp turn on fan"</div>
<div class="grid" id="grid"></div>
<button class="mic" onclick="startListen()">TAP &amp; SPEAK</button>
<div id="heard">Tap mic, or use buttons below. Mic needs internet.</div>
<p><input id="cmd" placeholder="type: esp turn on fan"><br><br>
<button class="send" onclick="sendText()">Send</button>
<button onclick="refresh()">Refresh</button></p>
<div class="note">1) Connect WiFi to ESP-SMART &nbsp; 2) open http://192.168.4.1/ &nbsp; 3) Tanner: LEDs first, AC only via electrician.</div>
<script>
var NAMES=["fan","light","tv","pump"];
var STATES={};
function xhrGet(url,cb){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4&&x.status==200)cb(x.responseText)};x.open("GET",url,true);x.send();}
function draw(){
  var g=document.getElementById("grid");g.innerHTML="";
  for(var i=0;i<NAMES.length;i++){
    (function(n){
      var on=!!STATES[n];
      var d=document.createElement("div");d.className="card";
      d.innerHTML="<h3>"+n+"</h3><div class='st "+(on?"on":"off")+"'>"+(on?"ON":"OFF")+"</div>";
      var b=document.createElement("button");b.className="tgl";b.innerHTML=on?"Turn OFF":"Turn ON";
      b.onclick=function(){xhrGet("/api/set?dev="+n+"&on="+(on?"0":"1"),function(){refresh();});};
      d.appendChild(b);g.appendChild(d);
    })(NAMES[i]);
  }
}
function refresh(){xhrGet("/api/state",function(t){try{STATES=JSON.parse(t);}catch(e){}draw();});}
function speakBack(msg){
  try{
    if("speechSynthesis" in window){var u=new SpeechSynthesisUtterance(msg);u.lang="en-IN";speechSynthesis.cancel();speechSynthesis.speak(u);}
  }catch(e){}
  document.getElementById("heard").innerHTML=msg;
}
function sendCmd(text){
  document.getElementById("heard").innerHTML="Heard: "+text+" ...";
  xhrGet("/api/cmd?text="+encodeURIComponent(text),function(t){
    var r={};try{r=JSON.parse(t);}catch(e){r={reply:t};}
    speakBack(r.reply||"Done.");refresh();
  });
}
function sendText(){var v=document.getElementById("cmd").value;if(v)sendCmd(v);}
function startListen(){
  var SR=window.SpeechRecognition||window.webkitSpeechRecognition;
  if(!SR){speakBack("Voice not supported here. Type command below instead.");return;}
  try{
    var rec=new SR();rec.lang="en-IN";rec.interimResults=false;rec.maxAlternatives=1;
    document.getElementById("heard").innerHTML="Listening... say: esp turn on fan";
    rec.onresult=function(e){
      var txt=e.results[0][0].transcript;
      sendCmd(txt);
    };
    rec.onerror=function(e){speakBack("Mic error ("+e.error+"). Type command instead.");};
    rec.onend=function(){};
    rec.start();
  }catch(e){speakBack("Mic failed. Type command instead.");}
}
refresh();setInterval(refresh,4000);
</script>
</body>
</html>
)HTML";
