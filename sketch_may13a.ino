#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

const char* host = "ANCCD";
const char* ssid = "labor";
const char* password = "98668800";

const String currentVersion = "0.0.3";
const String versionURL = "https://raw.githubusercontent.com/585468654/esp32-update-test/main/version.json";

WebServer server(80);

// Login Page
const char* loginIndex = R"rawliteral(
<form name='loginForm'>
<table width='20%' bgcolor='A09F9F' align='center'>
<tr><td colspan=2><center><font size=4><b>ESP32 Login Page</b></font></center><br></td></tr>
<tr><td>Username:</td><td><input type='text' size=25 name='userid'><br></td></tr>
<tr><td>Password:</td><td><input type='Password' size=25 name='pwd'><br></td></tr>
<tr><td><input type='submit' onclick='check(this.form)' value='Login'></td></tr>
</table>
</form>
<script>
function check(form) {
  if(form.userid.value=='admin' && form.pwd.value=='admin') {
    window.open('/serverIndex')
  } else {
    alert('Error Password or Username')
  }
}
</script>
)rawliteral";

// Upload Page
const char* serverIndex = R"rawliteral(
<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>
<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>
  <input type='file' name='update'>
  <input type='submit' value='Update'>
</form>
<div id='prg'>progress: 0%</div>
<script>
$('form').submit(function(e){
  e.preventDefault();
  var form = $('#upload_form')[0];
  var data = new FormData(form);
  $.ajax({
    url: '/update',
    type: 'POST',
    data: data,
    contentType: false,
    processData:false,
    xhr: function() {
      var xhr = new window.XMLHttpRequest();
      xhr.upload.addEventListener('progress', function(evt) {
        if (evt.lengthComputable) {
          var per = evt.loaded / evt.total;
          $('#prg').html('progress: ' + Math.round(per*100) + '%');
        }
      }, false);
      return xhr;
    },
    success:function(d, s) { console.log('success!') },
    error: function (a, b, c) {}
  });
});
</script>
)rawliteral;

// ===== OTA From GitHub =====
void checkUpdate() {
  Serial.println("檢查更新中...");

  HTTPClient http;
  http.begin(versionURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      String latestVersion = doc["version"];
      String binURL = doc["url"];

      if (latestVersion != currentVersion) {
        Serial.println("發現新版本: " + latestVersion);
        Serial.println("下載更新中...");

        performOTA(binURL);
      } else {
        Serial.println("已是最新版本。");
      }
    } else {
      Serial.println("JSON 解析失敗。");
    }
  } else {
    Serial.printf("下載 version.json 失敗，HTTP 狀態碼：%d\n", httpCode);
  }
  http.end();
}

void performOTA(String binURL) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, binURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      WiFiClient* stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);

      if (written == contentLength) {
        Serial.println("韌體寫入成功！");
      } else {
        Serial.println("寫入不完整！");
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("更新完成，將重新啟動...");
          ESP.restart();
        } else {
          Serial.println("更新未完成！");
        }
      } else {
        Serial.printf("更新失敗：%s\n", Update.errorString());
      }
    } else {
      Serial.println("無法開始更新");
    }
  } else {
    Serial.printf("下載韌體失敗，HTTP 狀態碼：%d\n", httpCode);
  }

  http.end();
}

// ===== Setup =====
void setup(void) {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已連接，IP: " + WiFi.localIP().toString());

  if (!MDNS.begin(host)) {
    Serial.println("MDNS 啟動失敗");
    while (1) delay(1000);
  }

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", loginIndex);
  });

  server.on("/serverIndex", HTTP_GET, []() {
    server.send(200, "text/html", serverIndex);
  });

  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("手動 OTA 成功，重啟中...\n");
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("HTTP Server 啟動");

  // 啟動時先檢查是否需要更新
  checkUpdate();
}

void loop(void) {
  server.handleClient();
  delay(1);
}
