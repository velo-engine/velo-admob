module admob

$if android {
	#flag -I @DIR
	#include "@DIR/admob_android.h"

	fn C.sapp_android_get_native_activity() voidptr

	fn C.velo_admob_init(activity voidptr, rewarded_unit &char, interstitial_unit &char)
	fn C.velo_admob_is_rewarded_ready(activity voidptr) int
	fn C.velo_admob_retry_rewarded(activity voidptr)
	fn C.velo_admob_show_rewarded(activity voidptr)
	fn C.velo_admob_poll_rewarded(activity voidptr) int
	fn C.velo_admob_is_interstitial_ready(activity voidptr) int
	fn C.velo_admob_retry_interstitial(activity voidptr)
	fn C.velo_admob_show_interstitial(activity voidptr)
	fn C.velo_admob_poll_interstitial(activity voidptr) int

	fn init_android(cfg Config) {
		act := C.sapp_android_get_native_activity()
		if act == unsafe { nil } {
			return
		}
		C.velo_admob_init(act, &char(cfg.rewarded_unit_android.str), &char(cfg.interstitial_unit_android.str))
	}

	fn is_rewarded_ready_android() bool {
		act := C.sapp_android_get_native_activity()
		return C.velo_admob_is_rewarded_ready(act) == 1
	}

	fn retry_rewarded_android() {
		act := C.sapp_android_get_native_activity()
		C.velo_admob_retry_rewarded(act)
	}

	fn show_rewarded_android() bool {
		act := C.sapp_android_get_native_activity()
		if C.velo_admob_is_rewarded_ready(act) == 0 {
			C.velo_admob_retry_rewarded(act)
			return false
		}
		C.velo_admob_show_rewarded(act)
		return true
	}

	fn poll_rewarded_android() AdResult {
		act := C.sapp_android_get_native_activity()
		return match C.velo_admob_poll_rewarded(act) {
			1 { AdResult.rewarded }
			2 { AdResult.closed }
			3 { AdResult.failed }
			else { AdResult.none }
		}
	}

	fn is_interstitial_ready_android() bool {
		act := C.sapp_android_get_native_activity()
		return C.velo_admob_is_interstitial_ready(act) == 1
	}

	fn retry_interstitial_android() {
		act := C.sapp_android_get_native_activity()
		C.velo_admob_retry_interstitial(act)
	}

	fn show_interstitial_android() bool {
		act := C.sapp_android_get_native_activity()
		if C.velo_admob_is_interstitial_ready(act) == 0 {
			C.velo_admob_retry_interstitial(act)
			return false
		}
		C.velo_admob_show_interstitial(act)
		return true
	}

	fn poll_interstitial_android() AdResult {
		act := C.sapp_android_get_native_activity()
		return match C.velo_admob_poll_interstitial(act) {
			1 { AdResult.closed }
			2 { AdResult.closed }
			3 { AdResult.failed }
			else { AdResult.none }
		}
	}
} $else {
	fn init_android(cfg Config) {}
	fn is_rewarded_ready_android() bool { return false }
	fn retry_rewarded_android() {}
	fn show_rewarded_android() bool { return false }
	fn poll_rewarded_android() AdResult { return .none }
	fn is_interstitial_ready_android() bool { return false }
	fn retry_interstitial_android() {}
	fn show_interstitial_android() bool { return false }
	fn poll_interstitial_android() AdResult { return .none }
}
