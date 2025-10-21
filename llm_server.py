from flask import Flask, request, send_from_directory, make_response, jsonify
from gtts import gTTS
from pydub import AudioSegment
import wave, os, uuid, requests
import pandas as pd
from datetime import datetime
import json
import io

app = Flask(__name__)
UPLOAD_FOLDER = "audios"
LOG_FILE = "access_logs.csv"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Server Configuration
SERVER_IP = "192.168.137.44"
SERVER_PORT = 5000

# API Configuration
GROQ_API_KEY = "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"
WHISPER_MODEL = "whisper-large-v3"
CHAT_MODEL = "llama3-8b-8192"

# Aktif kayıtları tutmak için sözlük
active_recordings = {}

def get_log_context():
    try:
        df = pd.read_csv(LOG_FILE)
        # Son 10 kaydı al ve string formatına çevir
        recent_logs = df.tail(10).to_string()
        return f"""Bu bir giriş-çıkış kayıt sistemidir. Aşağıdaki kayıtlar son giriş-çıkışları göstermektedir:\n\n{recent_logs}\n\nLütfen yukarıdaki kayıtları kullanarak soruyu yanıtla. Yanıtını Türkçe ve doğal bir dille ver."""
    except Exception as e:
        print(f"Log dosyası okuma hatası: {str(e)}")
        return "Kayıt dosyası okunamadı."

def get_user_info_by_id(user_id):
    try:
        # Log dosyasından son aktiviteyi bul
        if os.path.exists(LOG_FILE):
            logs_df = pd.read_csv(LOG_FILE)
            user_logs = logs_df[logs_df['ID'] == int(user_id)].sort_values('Timestamp', ascending=False)
            
            if not user_logs.empty:
                name = user_logs.iloc[0]['Name']
                last_action = user_logs.iloc[0]['Action']
                last_time = user_logs.iloc[0]['Timestamp']
                return f"{name} (ID: {user_id}). Son işlemi: {last_time} tarihinde {last_action}"
            
            return f"ID {user_id} için kayıt bulunamadı."
            
    except Exception as e:
        print(f"Kullanıcı bilgisi alma hatası: {str(e)}")
        return f"ID {user_id} için bilgi alınırken hata oluştu."

def is_log_related_query(text):
    # Log ile ilgili anahtar kelime kombinasyonlarını kontrol et
    text_lower = text.lower()
    
    # ID sorgusu kontrolü
    id_patterns = [
        r"id'?si? *\d+ *olan",
        r"id *\d+ *kim",
        r"\d+ *numaralı *kişi",
        r"\d+ *id'?li",
        r"numara *\d+ *kim"
    ]
    
    import re
    for pattern in id_patterns:
        if re.search(pattern, text_lower):
            return True, re.search(r"\d+", text_lower).group()  # ID numarasını da döndür
    
    # Anahtar kelime grupları
    primary_keywords = ["giriş", "çıkış", "kayıt", "parmak", "id"]
    secondary_keywords = ["kim", "ne zaman", "saat", "kişi", "kaç", "son"]
    
    # En az bir primary keyword ve bir secondary keyword olmalı
    has_primary = any(keyword in text_lower for keyword in primary_keywords)
    has_secondary = any(keyword in text_lower for keyword in secondary_keywords)
    
    # Özel durumlar için tam kalıplar
    exact_patterns = [
        "kim geldi",
        "kim gitti",
        "gelen kim",
        "giden kim",
        "son giriş",
        "son kayıt"
    ]
    
    has_exact_match = any(pattern in text_lower for pattern in exact_patterns)
    
    return (has_primary and has_secondary) or has_exact_match, None

@app.route("/upload", methods=["POST"])
def upload():
    try:
        print("\n=== New Upload Request ===")
        print(f"📥 Ses verisi alınıyor...")
        print(f"Content-Length: {request.content_length} bytes")
        print(f"Headers: {dict(request.headers)}")
        
        # İstek başlıklarını kontrol et
        is_first_chunk = request.headers.get('X-First-Chunk', 'false').lower() == 'true'
        is_last_chunk = request.headers.get('X-Last-Chunk', 'false').lower() == 'true'
        is_wake_check = request.headers.get('X-Wake-Check', 'false').lower() == 'true'
        session_id = request.headers.get('X-Session-ID', str(uuid.uuid4()))
        
        print(f"Session ID: {session_id}")
        print(f"First Chunk: {is_first_chunk}")
        print(f"Last Chunk: {is_last_chunk}")
        print(f"Wake Check: {is_wake_check}")
        
        # Eğer JSON olarak sadece text geldiyse, TTS-only mod
        if request.is_json:
            data = request.get_json()
            text = data.get("text", "").strip()
            lang = data.get("lang", "tr")  # Varsayılan olarak Türkçe
            if text:
                file_id = str(uuid.uuid4())
                mp3_path = os.path.join(UPLOAD_FOLDER, f"{file_id}.mp3")
                wav_reply_path = os.path.join(UPLOAD_FOLDER, f"{file_id}_reply.wav")
                try:
                    gTTS(text, lang=lang).save(mp3_path)
                    audio = AudioSegment.from_mp3(mp3_path)
                    audio = audio.set_frame_rate(16000).set_channels(1).set_sample_width(2)
                    raw_pcm = audio.raw_data
                    with wave.open(wav_reply_path, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(16000)
                        wf.writeframes(raw_pcm)
                    host = request.host_url.rstrip('/')
                    url = f"{host}/audios/{os.path.basename(wav_reply_path)}"
                    resp = make_response(url, 200)
                    resp.headers["Content-Type"] = "text/plain"
                    return resp
                except Exception as e:
                    return jsonify({"error": str(e)}), 500
        
        # Yeni kayıt oturumu başlat
        if is_first_chunk:
            print(f"🆕 Yeni kayıt oturumu başlatıldı: {session_id}")
            active_recordings[session_id] = io.BytesIO()
        
        # Veriyi geçici bellekte biriktir
        if session_id in active_recordings:
            active_recordings[session_id].write(request.data)
            current_size = active_recordings[session_id].tell()
            print(f"📝 Chunk alındı. Toplam boyut: {current_size} bytes")
            
            # Son chunk ise ses işlemeyi başlat
            if is_last_chunk:
                print("🔄 Son chunk alındı, ses işleme başlıyor...")
                wav_data = active_recordings[session_id].getvalue()
                
                # Geçici dosyaya kaydet
                file_id = str(uuid.uuid4())
                wav_path = os.path.join(UPLOAD_FOLDER, f"{file_id}.wav")
                
                print(f"💾 WAV dosyası kaydediliyor: {wav_path}")
                with open(wav_path, "wb") as f:
                    f.write(wav_data)
                
                print(f"WAV dosyası kaydedildi: {os.path.getsize(wav_path)} bytes")
                
                # Whisper API'ye gönder
                print("🎯 Whisper API'ye gönderiliyor...")
                headers = {"Authorization": f"Bearer {GROQ_API_KEY}"}
                
                with open(wav_path, "rb") as af:
                    try:
                        print("Whisper API isteği yapılıyor...")
                        asr_res = requests.post(
                            "https://api.groq.com/openai/v1/audio/transcriptions",
                            headers=headers,
                            files={"file": (os.path.basename(wav_path), af, "audio/wav")},
                            data={
                                "model": WHISPER_MODEL,
                                "language": "tr",
                                "response_format": "json"
                            }
                        )
                        print(f"Whisper API yanıt kodu: {asr_res.status_code}")
                        asr_res.raise_for_status()
                        text = asr_res.json().get("text", "").strip()
                        print(f"🗣️ Algılanan metin: {text}")
                        
                        # Wake word kontrolü ise sadece metni döndür
                        if is_wake_check:
                            print("Wake word kontrolü yapılıyor...")
                            resp = make_response(text, 200)
                            resp.headers["Content-Type"] = "text/plain"
                            return resp
                            
                        # Normal asistan işlemine devam et
                        print("📝 Sorgu tipi belirleniyor...")
                        is_log_query, id_match = is_log_related_query(text)
                        
                        if is_log_query:
                            print("📊 Log ile ilgili soru tespit edildi")
                            
                            # ID sorgusu ise direkt bilgiyi al
                            if id_match:
                                print(f"🔍 ID sorgusu tespit edildi: {id_match}")
                                user_info = get_user_info_by_id(id_match)
                                reply = user_info
                                print(f"💡 Kullanıcı bilgisi: {reply}")
                            else:
                                context = get_log_context()
                                system_prompt = """Sen bir akıllı asistansın. Sana verilen giriş-çıkış kayıtlarını kullanarak soruları yanıtlayacaksın.
                                Yanıtlarını her zaman Türkçe ve doğal bir dille ver. Teknik detaylardan kaçın, sade ve anlaşılır ol.
                                Eğer soruyu anlamadıysan veya kayıtlarda yeterli bilgi yoksa, bunu nazikçe belirt."""
                                messages = [
                                    {"role": "system", "content": system_prompt},
                                    {"role": "user", "content": context},
                                    {"role": "user", "content": text}
                                ]
                                
                                print("🤖 LLM API'ye gönderiliyor...")
                                try:
                                    chat_res = requests.post(
                                        "https://api.groq.com/openai/v1/chat/completions",
                                        headers=headers,
                                        json={
                                            "model": CHAT_MODEL,
                                            "messages": messages,
                                            "temperature": 0.7
                                        }
                                    )
                                    print(f"LLM API yanıt kodu: {chat_res.status_code}")
                                    chat_res.raise_for_status()
                                    reply = chat_res.json()["choices"][0]["message"]["content"].strip()
                                    print(f"💡 LLM yanıtı: {reply}")
                                except Exception as e:
                                    print(f"❌ LLM API hatası: {str(e)}")
                                    print(f"Yanıt: {chat_res.text if 'chat_res' in locals() else 'Yanıt yok'}")
                                    raise
                        else:
                            print("💭 Genel bir soru tespit edildi")
                            system_prompt = """Sen yardımcı bir asistansın. Kullanıcıların her türlü sorusuna yardımcı olabilirsin.
                            Yanıtlarını her zaman Türkçe ve doğal bir dille ver. Bilmediğin konularda dürüst ol.
                            Güncel olaylar, hava durumu, genel bilgi ve benzeri her konuda yardımcı olmaya çalış."""
                            messages = [
                                {"role": "system", "content": system_prompt},
                                {"role": "user", "content": text}
                            ]
                            
                            print("🤖 LLM API'ye gönderiliyor...")
                            try:
                                chat_res = requests.post(
                                    "https://api.groq.com/openai/v1/chat/completions",
                                    headers=headers,
                                    json={
                                        "model": CHAT_MODEL,
                                        "messages": messages,
                                        "temperature": 0.7
                                    }
                                )
                                print(f"LLM API yanıt kodu: {chat_res.status_code}")
                                chat_res.raise_for_status()
                                reply = chat_res.json()["choices"][0]["message"]["content"].strip()
                                print(f"💡 LLM yanıtı: {reply}")
                            except Exception as e:
                                print(f"❌ LLM API hatası: {str(e)}")
                                print(f"Yanıt: {chat_res.text if 'chat_res' in locals() else 'Yanıt yok'}")
                                raise

                        print(f"🗣️ Kullanıcı sorusu: {text}")
                    except Exception as e:
                        print(f"❌ Whisper API hatası: {str(e)}")
                        print(f"Yanıt: {asr_res.text if 'asr_res' in locals() else 'Yanıt yok'}")
                        raise

                # gTTS → MP3 → kesin PCM WAV
                print("🔊 Ses sentezleniyor...")
                mp3_path = os.path.join(UPLOAD_FOLDER, f"{file_id}.mp3")
                wav_reply_path = os.path.join(UPLOAD_FOLDER, f"{file_id}_reply.wav")
                
                try:
                    print("gTTS ile MP3 oluşturuluyor...")
                    gTTS(reply, lang="tr").save(mp3_path)
                    print(f"MP3 kaydedildi: {mp3_path}")

                    # pydub ile MP3'ü decode edip raw PCM veriye dönüştür
                    print("MP3'ten WAV'a dönüştürülüyor...")
                    audio = AudioSegment.from_mp3(mp3_path)
                    audio = audio.set_frame_rate(16000).set_channels(1).set_sample_width(2)
                    raw_pcm = audio.raw_data

                    # wave modülü ile baştan oluşturulan header + PCM
                    with wave.open(wav_reply_path, "wb") as wf:
                        wf.setnchannels(1)         # mono
                        wf.setsampwidth(2)         # 16 bit = 2 byte
                        wf.setframerate(16000)     # 16 kHz
                        wf.writeframes(raw_pcm)

                    print(f"✅ Yanıt WAV kaydedildi: {wav_reply_path}")
                    print(f"WAV boyutu: {os.path.getsize(wav_reply_path)} bytes")
                except Exception as e:
                    print(f"❌ Ses sentezleme hatası: {str(e)}")
                    raise

                # Oturum verilerini temizle
                del active_recordings[session_id]
                
                # URL'i dön
                host = request.host_url.rstrip('/')
                url = f"{host}/audios/{os.path.basename(wav_reply_path)}"
                print(f"🔗 Dönülen URL: {url}")
                resp = make_response(url, 200)
                resp.headers["Content-Type"] = "text/plain"
                return resp
            
            # Ara chunk'lar için OK yanıtı
            return make_response("OK", 200)
            
        else:
            error_msg = "Geçersiz oturum ID'si veya aktif kayıt bulunamadı"
            print(f"❌ Hata: {error_msg}")
            return jsonify({
                "error": error_msg,
                "details": "Bu oturum ID'si için aktif kayıt bulunamadı"
            }), 400

    except Exception as e:
        print(f"❌ Genel hata: {str(e)}")
        import traceback
        print(f"Stack trace:\n{traceback.format_exc()}")
        
        # Hata durumunda oturum verilerini temizle
        if 'session_id' in locals() and session_id in active_recordings:
            del active_recordings[session_id]
            
        return jsonify({
            "error": "İşlem sırasında bir hata oluştu",
            "details": str(e)
        }), 500

@app.route("/audios/<filename>")
def serve_audio(filename):
    return send_from_directory(UPLOAD_FOLDER, filename)



@app.route("/register_user", methods=["POST"])
def register_user():
    try:
        data = request.get_json()
        print(f"[DEBUG] Gelen kayıt isteği: {data}")  # 🔍 debug log

        name = data.get("name", "").strip()
        password = data.get("password", "").strip()

        if not name or not password:
            return jsonify({"status": "error", "message": "İsim ve şifre zorunlu."}), 400

        users_file = "users.csv"

        # CSV dosyası yoksa oluştur
        if not os.path.exists(users_file):
            df = pd.DataFrame(columns=["Name", "Password"])
            df.to_csv(users_file, index=False)

        # CSV'yi yükle
        df = pd.read_csv(users_file)

        # Kullanıcı zaten varsa → güncelle
        if not df[df["Name"] == name].empty:
            print(f"[INFO] {name} adlı kullanıcı zaten kayıtlı, şifresi güncelleniyor.")
            df.loc[df["Name"] == name, "Password"] = password
            message = "Mevcut kullanıcı güncellendi."
        else:
            # Yeni kullanıcı olarak ekle
            df = pd.concat([df, pd.DataFrame([{"Name": name, "Password": password}])], ignore_index=True)
            message = "Yeni kullanıcı kaydedildi."

        # Güncellenmiş CSV'yi kaydet
        df.to_csv(users_file, index=False)

        return jsonify({"status": "success", "message": message, "name": name}), 200

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500


@app.route("/check_user", methods=["POST"])
def check_user():
    try:
        data = request.get_json()
        name = data.get("name", "").strip()
        users_file = "users.csv"
        if not name or not os.path.exists(users_file):
            return "NOT_FOUND", 200
        df = pd.read_csv(users_file)
        if not df[df["Name"] == name].empty:
            return "OK", 200
        else:
            return "NOT_FOUND", 200
    except Exception as e:
        return "NOT_FOUND", 200

@app.route("/verify_user", methods=["POST"])
def verify_user():
    try:
        data = request.get_json()
        name = data.get("name", "").strip()
        password = data.get("password", "").strip()
        if not name or not password:
            return jsonify({"status": "error", "message": "İsim ve şifre zorunlu."}), 400
        users_file = "users.csv"
        if not os.path.exists(users_file):
            return jsonify({"status": "error", "message": "Kullanıcı bulunamadı."}), 400
        df = pd.read_csv(users_file)
        df["Password"] = df["Password"].astype(str).str.strip()
        match = df[(df["Name"] == name) & (df["Password"] == password)]
        if not match.empty:
            # Log kaydı
            log_file = "access_logs.csv"
            now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            log_df = pd.DataFrame([{ "Name": name, "Timestamp": now }])
            if not os.path.exists(log_file):
                log_df.to_csv(log_file, index=False)
            else:
                log_df.to_csv(log_file, mode='a', header=False, index=False)
            return jsonify({"status": "success", "message": "Giriş başarılı.", "name": name}), 200
        else:
            return jsonify({"status": "error", "message": "Şifre yanlış veya kullanıcı yok."}), 401
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route("/last_login", methods=["GET"])
def last_login():
    log_file = "access_logs.csv"
    if not os.path.exists(log_file):
        return jsonify({"message": "Kayıt yok"}), 200
    try:
        df = pd.read_csv(log_file)
        if df.empty:
            return jsonify({"message": "Kayıt yok"}), 200

        last_name = df.iloc[-1]["Name"]
        tts_text = f"Son giriş yapan kişi: {last_name}"

        # Sesli yanıt üret
        file_id = str(uuid.uuid4())
        mp3_path = os.path.join(UPLOAD_FOLDER, f"{file_id}.mp3")
        wav_path = os.path.join(UPLOAD_FOLDER, f"{file_id}_reply.wav")
        try:
            gTTS(tts_text, lang="tr").save(mp3_path)
            audio = AudioSegment.from_mp3(mp3_path)
            audio = audio.set_frame_rate(16000).set_channels(1).set_sample_width(2)
            with wave.open(wav_path, "wb") as wf:
                wf.setnchannels(1)
                wf.setsampwidth(2)
                wf.setframerate(16000)
                wf.writeframes(audio.raw_data)
            host = request.host_url.rstrip('/')
            url = f"{host}/audios/{os.path.basename(wav_path)}"

            return jsonify({
                "name": last_name,
                "url": url
            }), 200
        except Exception as e:
            print(f"Ses sentezleme hatası: {str(e)}")
            return jsonify({"name": last_name}), 200
    except Exception as e:
        return jsonify({"message": f"Hata: {str(e)}"}), 500

    
if __name__ == "__main__":
    print(f"Server starting on http://{SERVER_IP}:{SERVER_PORT}")
    app.run(host="0.0.0.0", port=SERVER_PORT, debug=True)