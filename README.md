# Velo AdMob SDK for V & Velo

Module tích hợp quảng cáo **Google AdMob** đa nền tảng dành cho game và ứng dụng viết bằng ngôn ngữ [V](https://vlang.io) và engine [Velo](https://github.com/velo-engine).

Hỗ trợ native:
- **Android**: Kết nối trực tiếp qua JNI (`admob_android.h`) và Java bridge (`AdBridge.java`) sử dụng Google Play Services Ads SDK. Tương thích hoàn toàn với toolchain `vab` / `velo build android`.
- **iOS**: Cầu nối native Objective-C với ARC (`admob_ios.h`) tương tác với Google Mobile Ads SDK (`GADMobileAds`, `GADRewardedAd`, `GADInterstitialAd`), hỗ trợ cả thiết bị thật (arm64) và iOS Simulator.
- **Desktop (macOS, Windows, Linux) / Web / Testing**: Chế độ Mock Ads thông minh tự động kích hoạt giúp kiểm thử gameplay, luồng trả thưởng và tự động hóa CI mà không bị lỗi.

---

## 1. Cài đặt

### Cách 1: Thêm vào `v.mod` (Khuyến nghị)

Thêm vào dependencies trong file `v.mod` của dự án:

```v
Module {
	name: 'my_game'
	version: '1.0.0'
	dependencies: ['admob']
}
```

### Cách 2: Phát triển cục bộ bằng `v link`

Nếu bạn phát triển hoặc chỉnh sửa source code của module:

```bash
git clone https://github.com/cho-game/velo-admob
cd velo-admob
v link
```

Lệnh `v link` sẽ tạo symlink `admob` trong `~/.vmodules/`, cho phép bất kỳ dự án V/Velo nào trên máy có thể gọi `import admob`.

---

## 2. Thiết lập nền tảng

### 2.1 Android

AdMob trên Android cần Google Play Services Ads và lớp cầu nối Java. Module cung cấp script `tools/fetch_deps.py` để tự động hóa:

1. Chạy tải thư viện và cài đặt bridge vào game:
   ```bash
   python3 tools/fetch_deps.py android
   ```
2. File cấu hình `android/deps.json` trong game:
   ```json
   {
     "dependencies": ["com.google.android.gms:play-services-ads:25.5.0"],
     "admob_app_id": "ca-app-pub-xxxxxxxxxxxxxxxx~yyyyyyyyyy"
   }
   ```
3. Script sẽ thiết lập toàn bộ file cần thiết trong `android/deps/`:
   - `jars/`: Thư viện Play Services Ads
   - `res/`: Tài nguyên của AAR
   - `src/io/velo/admob/AdBridge.java`: Lớp điều khiển Java
   - `manifest_permissions.xml` & `manifest_application.xml`: Tự động điền `com.google.android.gms.ads.APPLICATION_ID`.

### 2.2 iOS

1. Chạy lệnh tải Google Mobile Ads xcframework:
   ```bash
   python3 tools/fetch_deps.py ios
   ```
2. Thêm `GADApplicationIdentifier` và `SKAdNetworkItems` vào `Info.plist` (hoặc cấu hình `velo.toml`):
   ```xml
   <key>GADApplicationIdentifier</key>
   <string>ca-app-pub-xxxxxxxxxxxxxxxx~yyyyyyyyyy</string>
   <key>SKAdNetworkItems</key>
   <array>
       <dict>
           <key>SKAdNetworkIdentifier</key>
           <string>cstr6suwn9.skadnetwork</string>
       </dict>
   </array>
   ```

*Lưu ý:* Khi chạy trên iOS Simulator hoặc môi trường chưa nhúng GoogleMobileAds framework, bridge tự động nhận diện an toàn và chuyển sang mock mode mà không gây crash ứng dụng.

---

## 3. Cách sử dụng

### 3.1 Khởi tạo

Khởi tạo SDK khi game bắt đầu (ví dụ trong màn hình loading hoặc menu chính):

```v
import admob

// Khởi tạo đầy đủ cấu hình cho Android và iOS:
admob.init(admob.Config{
    app_id_android:            'ca-app-pub-2847117040992400~6864359966'
    app_id_ios:                'ca-app-pub-2847117040992400~6864359966'
    rewarded_unit_android:     'ca-app-pub-2847117040992400/4166435045'
    rewarded_unit_ios:         'ca-app-pub-2847117040992400/4166435045'
    interstitial_unit_android: 'ca-app-pub-2847117040992400/1234567890'
    interstitial_unit_ios:     'ca-app-pub-2847117040992400/1234567890'
})

// Hoặc khởi tạo nhanh nếu chỉ dùng Rewarded Video:
admob.init_rewarded('ca-app-pub-2847117040992400/4166435045')
```

### 3.2 Quảng cáo trả thưởng (Rewarded Video)

Quảng cáo chạy bất đồng bộ để không chặn vòng lặp 60 FPS của game. Hãy gọi `show_rewarded()` khi người chơi xem và kiểm tra kết quả qua `poll_rewarded()`:

```v
import admob

// 1. Kiểm tra sẵn sàng và hiển thị:
pub fn on_click_watch_ad() {
    if !admob.is_rewarded_ready() {
        println('Quảng cáo đang tải, vui lòng thử lại sau...')
        admob.retry_rewarded()
        return
    }

    if admob.show_rewarded() {
        println('Bắt đầu phát video...')
    }
}

// 2. Poll kết quả trong hàm update() mỗi frame:
pub fn (mut g MyGame) update(dt f32) {
    match admob.poll_rewarded() {
        .rewarded {
            println('Người chơi đã xem hết: trả thưởng vàng/năng lượng!')
            g.give_reward()
        }
        .closed {
            println('Người chơi đóng video sớm: không nhận được thưởng')
        }
        .failed {
            println('Hiển thị video thất bại')
        }
        .none {
            // Không có sự kiện kết thúc
        }
    }
}
```

### 3.3 Quảng cáo chuyển cảnh (Interstitial Ad)

```v
// Hiển thị quảng cáo giữa các màn chơi:
if admob.is_interstitial_ready() {
    admob.show_interstitial()
}

// Kiểm tra trạng thái đóng:
match admob.poll_interstitial() {
    .closed {
        println('Đã đóng quảng cáo interstitial, tiếp tục màn tiếp theo')
    }
    .failed {
        println('Lỗi hiển thị interstitial')
    }
    .none {}
}
```

---

## 4. API Reference

| Hàm | Kiểu trả về | Mô tả |
|---|---|---|
| `admob.init(cfg Config)` | `void` | Khởi tạo SDK với thông tin ID cho Android và iOS |
| `admob.init_rewarded(unit string)` | `void` | Khởi tạo nhanh với rewarded ad unit |
| `admob.config()` | `Config` | Lấy thông tin cấu hình hiện tại |
| `admob.is_initialized()` | `bool` | Kiểm tra SDK đã được khởi tạo hay chưa |
| `admob.is_mock()` | `bool` | Trả về `true` nếu đang chạy môi trường mock (Desktop/Web) |
| `admob.is_rewarded_ready()` | `bool` | Kiểm tra video trả thưởng đã tải xong chưa |
| `admob.show_rewarded()` | `bool` | Bắt đầu phát video trả thưởng |
| `admob.retry_rewarded()` | `void` | Yêu cầu tải lại video trả thưởng nếu trước đó bị lỗi |
| `admob.poll_rewarded()` | `AdResult` | Lấy kết quả video trả thưởng (`.none`, `.rewarded`, `.closed`, `.failed`) |
| `admob.is_interstitial_ready()` | `bool` | Kiểm tra interstitial ad đã sẵn sàng chưa |
| `admob.show_interstitial()` | `bool` | Bắt đầu hiển thị interstitial ad |
| `admob.retry_interstitial()` | `void` | Yêu cầu tải lại interstitial ad |
| `admob.poll_interstitial()` | `AdResult` | Lấy kết quả interstitial ad (`.none`, `.closed`, `.failed`) |

---

## 5. Kiểm thử (Unit Tests)

Chạy bộ test của module:

```bash
v test .
```

---

## Giấy phép

MIT License.
