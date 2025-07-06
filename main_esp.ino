#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* ssid = "Acer";
const char* password = "Sarthak@017";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

struct Item {
  String name;
  int price;
  int qty;
};

Item cart[20];
int itemCount = 0;
int grandTotal = 0;
String lastScanned = "Waiting...";
String lastName = "";
int lastPrice = 0;

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Last Scanned:");
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(lastName);
  display.setTextSize(2);
  display.setCursor(0, 50);
  display.print("Rs. ");
  display.println(lastPrice);
  display.display();
}

void addToCart(String name, int price) {
  for (int i = 0; i < itemCount; i++) {
    if (cart[i].name == name) {
      cart[i].qty++;
      grandTotal += price;
      return;
    }
  }
  cart[itemCount].name = name;
  cart[itemCount].price = price;
  cart[itemCount].qty = 1;
  itemCount++;
  grandTotal += price;
}

// void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
//   if (type == WStype_TEXT) {
//     String data = (char*)payload;
//     Serial.println("Received: " + data);

//     if (data.startsWith("NAME:")) {
//       lastName = data.substring(5);
//       lastScanned = lastName;
//     }
//     else if (data.startsWith("PRICE:")) {
//       lastPrice = data.substring(6).toInt();
//       addToCart(lastName, lastPrice);
//       updateOLED();
//     }
//   }
// }

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String data = (char*)payload;
    Serial.println("Received: " + data);

    if (data.startsWith("NAME:")) {
      lastName = data.substring(5);
      lastScanned = lastName;
    }

    else if (data.startsWith("PRICE:")) {
      lastPrice = data.substring(6).toInt();
      addToCart(lastName, lastPrice);
      updateOLED();
    }

    else if (data.startsWith("REMOVED:")) {
      String removeName = data.substring(8);
      for (int i = 0; i < itemCount; i++) {
        if (cart[i].name == removeName) {
          grandTotal -= cart[i].price * cart[i].qty;
          for (int j = i; j < itemCount - 1; j++) {
            cart[j] = cart[j + 1];
          }
          itemCount--;
          Serial.println("Item removed: " + removeName);
          updateOLED();
          break;
        }
      }
    }

    else if (data.startsWith("QTY:")) {
      int newQty = data.substring(4).toInt();
      for (int i = 0; i < itemCount; i++) {
        if (cart[i].name == lastName) {
          grandTotal -= cart[i].qty * cart[i].price;
          cart[i].qty = newQty;
          grandTotal += newQty * cart[i].price;
          Serial.println("Quantity updated: " + cart[i].name + " = " + String(newQty));
          updateOLED();
          break;
        }
      }
    }
  }
}


void handleCartData() {
  String table = "<table><tr><th>Product</th><th>Qty</th><th>Price</th><th>Total</th></tr>";
  for (int i = 0; i < itemCount; i++) {
    table += "<tr><td>" + cart[i].name + "</td><td>" + String(cart[i].qty) + "</td><td>₹" + String(cart[i].price) + "</td><td>₹" + String(cart[i].price * cart[i].qty) + "</td></tr>";
  }
  table += "</table><p class='total'>Grand Total: ₹" + String(grandTotal) + "</p>";
  table += "<p>Last Scanned: " + lastScanned + "</p>";
  server.send(200, "text/html", table);
}

void handleRoot() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Smart Cart</title>
    <style>
      body { font-family: Arial; padding: 20px; background: #f4f4f4; }
      h1 { color: #2c3e50; }
      table { width: 100%; border-collapse: collapse; margin-top: 20px; }
      th, td { padding: 12px; border: 1px solid #ccc; text-align: center; }
      th { background-color: #3498db; color: white; }
      tr:nth-child(even) { background-color: #f9f9f9; }
      .total { font-weight: bold; font-size: 18px; color: green; margin-top: 20px; }
      .pay-btn { padding: 12px 20px; background: #27ae60; color: white; border: none; cursor: pointer; margin-top: 20px; font-size: 16px; border-radius: 5px; }
    </style>
    <script>
      function updateCart() {
        fetch('/cart')
          .then(res => res.text())
          .then(data => {
            document.getElementById('cart').innerHTML = data;
          });
      }

      function payNow() {
        fetch("/generateQR")
          .then(res => res.text())
          .then(data => {
            document.getElementById('qr').innerHTML = data;
          });
      }

      function confirmPayment() {
        fetch("/pay")
          .then(res => res.text())
          .then(msg => {
            alert(msg);
            location.reload();
          });
      }

      setInterval(updateCart, 1000);
    </script>
  </head>
  <body>
    <h1>🛒 Smart Cart - Live Bill</h1>
    <div id="cart"><p>Loading cart...</p></div>
    <button class="pay-btn" onclick="payNow()">Pay Now</button>
    <div id="qr"></div>
    <button class="pay-btn" onclick="confirmPayment()">I Have Paid</button>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", page);
}
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      code0 = ((c >> 4) & 0xf) + '0';
      if (((c >> 4) & 0xf) > 9) code0 = ((c >> 4) & 0xf) - 10 + 'A';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}


void handleGenerateQR() {
  String upiURL = "upi://pay?pa=7205788849@jupiteraxis&pn=sarthak&am=" + String(grandTotal) + "&cu=INR";
   String encodedUpiURL = urlEncode(upiURL);

  String qrURL = "https://api.dub.co/qr?url=" + encodedUpiURL;
  String html = "<h3>Scan this QR to Pay ₹" + String(grandTotal) + "</h3>";
  html += "<img src='" + qrURL + "' width='250' height='250'>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected: " + WiFi.localIP().toString());

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED not found. Check wiring!");
    while (true);
  }
  Serial.println("✅ OLED ready!");
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("SmartCart");
  display.display();

  server.on("/", handleRoot);
  server.on("/cart", handleCartData);
  server.on("/generateQR", handleGenerateQR);
  server.on("/pay", []() {
    grandTotal = 0;
    itemCount = 0;
    lastScanned = "Waiting...";
    lastName = "";
    lastPrice = 0;
    server.send(200, "text/plain", " Payment Confirmed. Thank you!");
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  Serial.println("Web server and WebSocket started.");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}