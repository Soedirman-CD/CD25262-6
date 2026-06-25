/*
 * SISTEM SMART FEEDER IKAN LELE BERBASIS IOT
 * LOKASI: TAMBAK TARKO LELE
 */

// =======================================================
// 1. KONFIGURASI BLYNK & WIFI
// =======================================================
#define BLYNK_TEMPLATE_ID "TMPL6Tn21RCDb"
#define BLYNK_TEMPLATE_NAME "Smart Feeder Ikan Lele Berbasis Internet of Things"
#define BLYNK_AUTH_TOKEN "igvHGdBBrEH706kuFg6am8Su1k64F8Zz"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "time.h"
#include "HX711.h"
#include <ESP32Servo.h>

char ssid[] = "Redmi 14C";
char pass[] = "123456789";

// =======================================================
// 2. DEKLARASI PIN (PIN AMAN / BUKAN STRAPPING PIN)
// =======================================================
const int LOADCELL_DOUT_PIN = 16;  // Modul HX711 (Pin DT)
const int LOADCELL_SCK_PIN = 4;    // Modul HX711 (Pin SCK)
const int SERVO_PIN = 18;          // Motor Servo (Sinyal PWM)
const int RELAY_PIN = 22;          // Modul Relay (Pelontar 12V) - Sangat aman di pin 19

HX711 scale;
Servo katupServo;

// =======================================================
// 3. VARIABEL KONTROL & DINAMIS (DARI BLYNK)
// =======================================================
float faktor_kalibrasi = (-342.512);  // Nilai kalibrasi Load Cell
bool sistemAktif = true;              // Flag Mode Auto/Manual (V3)

// Variabel Target Berat (Default 500 gram)
int targetBeratPakan = 500;

// Variabel Jadwal Waktu (Default)
int jamMakan1 = 8, menitMakan1 = 0;
int jamMakan2 = 14, menitMakan2 = 0;
int jamMakan3 = 20, menitMakan3 = 0;

// Flag Mencegah Pemberian Pakan Ganda di Menit yang Sama
bool sudahMakan1 = false;
bool sudahMakan2 = false;
bool sudahMakan3 = false;

// Konfigurasi NTP Server untuk Sinkronisasi RTC Internal (WIB / UTC+7)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;
const int daylightOffset_sec = 0;

// =======================================================
// 4. FUNGSI PENERIMA DATA DARI BLYNK
// =======================================================

// V2: Tombol Manual Start
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    Serial.println("\n[MANUAL] Perintah manual diterima dari Blynk!");
    prosesBeriMakan();
  }
}

// V3: Switch Mode Auto/Off
BLYNK_WRITE(V3) {
  sistemAktif = param.asInt();
  Serial.print("[SISTEM] Mode Auto Jadwal NTP: ");
  Serial.println(sistemAktif ? "AKTIF" : "MATI");
}

// V4: Input Target Berat (Slider/Numeric Input)
BLYNK_WRITE(V4) {
  targetBeratPakan = param.asInt();
  Serial.printf("[UPDATE] Target berat pakan: %d gram\n", targetBeratPakan);
}

// V5: Input Jadwal Pagi (Time Input)
BLYNK_WRITE(V5) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    jamMakan1 = t.getStartHour();
    menitMakan1 = t.getStartMinute();
    Serial.printf("[UPDATE] Jadwal 1: %02d:%02d WIB\n", jamMakan1, menitMakan1);
  }
}

// V6: Input Jadwal Siang (Time Input)
BLYNK_WRITE(V6) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    jamMakan2 = t.getStartHour();
    menitMakan2 = t.getStartMinute();
    Serial.printf("[UPDATE] Jadwal 2: %02d:%02d WIB\n", jamMakan2, menitMakan2);
  }
}

// V7: Input Jadwal Malam (Time Input)
BLYNK_WRITE(V7) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    jamMakan3 = t.getStartHour();
    menitMakan3 = t.getStartMinute();
    Serial.printf("[UPDATE] Jadwal 3: %02d:%02d WIB\n", jamMakan3, menitMakan3);
  }
}

// =======================================================
// 5. FUNGSI UTAMA PEMBERIAN PAKAN (DENGAN PROTEKSI GANDA)
// =======================================================
void prosesBeriMakan() {
  Serial.println("\n>>> MEMBUKA KATUP PENAKAR <<<");
  Blynk.virtualWrite(V8, 1); // Menyalakan LED Status Servo (V8)
  katupServo.write(90);      // Buka katup pakan

  float beratSekarang = 0;
  int toleransi = 10;        // LAPIS 1: Toleransi pakan melayang
  
  // LAPIS 2: Menyiapkan Stopwatch Timeout (Batas 30 detik)
  unsigned long waktuMulai = millis(); 
  const unsigned long BATAS_WAKTU = 12000; 
  
  // Looping penimbangan: Berhenti jika tercapai ATAU timeout
  while (beratSekarang < (targetBeratPakan - toleransi)) {
    beratSekarang = scale.get_units(3); 
    if (beratSekarang < 0) beratSekarang = 0; 

    Serial.print("Menimbang: ");
    Serial.print(beratSekarang);
    Serial.println(" g");
    
    Blynk.virtualWrite(V0, beratSekarang); 
    Blynk.run(); 
    
    // Pengecekan Timeout (Alarm Pakan Habis/Macet)
    if (millis() - waktuMulai > BATAS_WAKTU) {
      Serial.println("[ALARM] Timeout 30 detik! Tandon pakan kosong atau macet.");
      break; 
    }

    delay(500); 
  }

  // Menutup Katup
  katupServo.write(0);       
  Blynk.virtualWrite(V8, 0); 
  Serial.println(">>> PENAKARAN SELESAI. KATUP DITUTUP <<<");
  
  // Memberi waktu agar sisa pakan yang melayang jatuh ke timbangan
  delay(2000); 
  
  // Timbang ulang hasil final
  float beratFinal = scale.get_units(5);
  if (beratFinal < 0) beratFinal = 0;
  Blynk.virtualWrite(V0, beratFinal); 
  Serial.printf("Berat Akhir yang Dilontarkan: %.1f g\n", beratFinal);
  
  // Mengeksekusi Motor Pelontar
  Serial.println(">>> MOTOR PELONTAR AKTIF (5 DETIK) <<<");
  Blynk.virtualWrite(V1, 1); 
  digitalWrite(RELAY_PIN, LOW); // Relay menyala
  delay(5000); 
  
  // Mematikan Motor Pelontar
  digitalWrite(RELAY_PIN, HIGH); // Relay mati
  Blynk.virtualWrite(V1, 0); 
  Serial.println(">>> SIKLUS PEMBERIAN PAKAN SELESAI <<<");

  scale.tare(); // Kembalikan timbangan ke 0 untuk jadwal berikutnya
  Blynk.virtualWrite(V0, 0); 
}

// =======================================================
// 6. FUNGSI PENGECEKAN JADWAL REALTIME
// =======================================================
void cekJadwalNTP() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;  

  int jam = timeinfo.tm_hour;
  int menit = timeinfo.tm_min;
  int detik = timeinfo.tm_sec;

  

  // Reset semua flag pada jam 00:00 (Tengah malam)
  if (jam == 0 && menit == 0) {
    sudahMakan1 = false;
    sudahMakan2 = false;
    sudahMakan3 = false;
  }

  // Debugging Waktu (Tampil tiap 10 detik di Serial Monitor)
  if (detik % 10 == 0) {
    Serial.printf("Waktu Saat Ini: %02d:%02d:%02d WIB | Mode Auto: %s | Target: %d g\n",
                  jam, menit, detik, sistemAktif ? "ON" : "OFF", targetBeratPakan);
  }

  // Pengecekan Eksekusi Jadwal Otomatis (HANYA MENYALA JIKA JADWAL SESUAI)
  if (sistemAktif) {
    if (jam == jamMakan1 && menit == menitMakan1 && !sudahMakan1) {
      Serial.println("\n[AUTO] Waktunya Jadwal 1 Pagi!");
      prosesBeriMakan();
      sudahMakan1 = true;
    } else if (jam == jamMakan2 && menit == menitMakan2 && !sudahMakan2) {
      Serial.println("\n[AUTO] Waktunya Jadwal 2 Siang!");
      prosesBeriMakan();
      sudahMakan2 = true;
    } else if (jam == jamMakan3 && menit == menitMakan3 && !sudahMakan3) {
      Serial.println("\n[AUTO] Waktunya Jadwal 3 Malam!");
      prosesBeriMakan();
      sudahMakan3 = true;
    }
  }
}

// =======================================================
// 7. SETUP & MAIN LOOP
// =======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- MEMULAI SISTEM SMART FEEDER ---");

  // 1. Inisialisasi Hardware (TRIK KUNCI SEBELUM BUKA UNTUK RELAY ACTIVE-LOW)
  // Ini mencegah motor langsung menyala saat dicolokkan ke listrik
   
  pinMode(RELAY_PIN, OUTPUT);    
digitalWrite(RELAY_PIN, HIGH); 
  katupServo.setPeriodHertz(50);
  katupServo.attach(SERVO_PIN, 500, 2400);
  katupServo.write(0);

  // 2. Inisialisasi Timbangan Load Cell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(faktor_kalibrasi);
  scale.tare();

  // 3. Inisialisasi Konektivitas (WiFi & Blynk)
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 4. Sinkronisasi Awal Aplikasi Blynk (MEMATIKAN SEMUA INDIKATOR LED)
  Blynk.virtualWrite(V1, 0); 
  Blynk.virtualWrite(V8, 0); 
  Blynk.virtualWrite(V0, 0); 
  
  Serial.println("Sistem Siap. Waktu dan Blynk berhasil tersinkronisasi.");
}

void loop() {
  Blynk.run();
  cekJadwalNTP();
  delay(1000);  // Efisiensi CPU
}