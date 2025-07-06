import cv2
import winsound
from pyzbar.pyzbar import decode
import numpy as np
import websocket
import firebase_admin
from firebase_admin import credentials, db

# 🔹 ESP32 WebSocket
ESP32_IP = "ws://10.189.137.120:81"

# 🔹 Firebase Setup
cred = credentials.Certificate("smart-cart-ec13b-firebase-adminsdk-fbsvc-0673aee615.json")
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://smart-cart-ec13b-default-rtdb.firebaseio.com/"
})

def send_to_esp32(message):
    try:
        ws = websocket.create_connection(ESP32_IP)
        ws.send(message)
        ws.close()
        print(f"[OK] Sent to ESP32: {message}")
    except Exception as e:
        print(f"[ERROR] ESP32 Error: {e}")

def get_product_info(barcode):
    """ Fetch product name & price from Firebase """
    try:
        ref = db.reference("/")
        products = ref.get()

        if products and barcode in products:
            name = products[barcode].get("name", "Unknown")
            price = products[barcode].get("price", "Unknown")
            return name, price
        else:
            return None, None  # Explicitly return None if not found
    except Exception as e:
        print(f"[ERROR] Firebase Error: {e}")
        return None, None

def adjust_brightness_contrast(image):
    alpha, beta = 1.5, 30
    return cv2.convertScaleAbs(image, alpha=alpha, beta=beta)

def preprocess_image(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    enhanced = adjust_brightness_contrast(gray)
    blurred = cv2.GaussianBlur(enhanced, (3, 3), 0)
    return blurred

def detect_barcode_from_camera():
    cap = cv2.VideoCapture(1, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
    detected_barcodes = set()

    while True:
        ret, frame = cap.read()
        if not ret:
            print("[ERROR] Camera error")
            continue

        processed_frame = preprocess_image(frame)
        barcodes = decode(processed_frame)

        for barcode in barcodes:
            try:
                barcode_data = barcode.data.decode("utf-8")
            except Exception as e:
                print("[ERROR] Failed to decode barcode:", e)
                continue

            x, y, w, h = barcode.rect
            cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(frame, barcode_data, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

            if barcode_data not in detected_barcodes:
                winsound.Beep(1000, 200)
                detected_barcodes.add(barcode_data)

                # 🔍 Get product from Firebase
                name, price = get_product_info(barcode_data)

                if name and price:  # Only send if valid product found
                    send_to_esp32(f"NAME:{name}")
                    send_to_esp32(f"PRICE:{price}")
                else:
                    print(f"[INFO] Product not found in database for barcode: {barcode_data}")

            print(f"[OK] Scanned: {barcode_data}")

        cv2.imshow("Smart Cart Barcode Scanner", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

# 🚀 Start everything
detect_barcode_from_camera()