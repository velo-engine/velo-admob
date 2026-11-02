module admob

$if ios {
	#flag -I @DIR
	#include "@DIR/admob_ios.h"

	fn C.velo_admob_ios_init(&char, &char)
	fn C.velo_admob_ios_is_rewarded_ready() int
	fn C.velo_admob_ios_retry_rewarded()
	fn C.velo_admob_ios_show_rewarded() int
	fn C.velo_admob_ios_poll_rewarded() int
	fn C.velo_admob_ios_is_interstitial_ready() int
	fn C.velo_admob_ios_retry_interstitial()
	fn C.velo_admob_ios_show_interstitial() int
	fn C.velo_admob_ios_poll_interstitial() int

	fn init_ios(cfg Config) {
		C.velo_admob_ios_init(&char(cfg.rewarded_unit_ios.str), &char(cfg.interstitial_unit_ios.str))
	}

	fn is_rewarded_ready_ios() bool {
		return C.velo_admob_ios_is_rewarded_ready() == 1
	}

	fn retry_rewarded_ios() {
		C.velo_admob_ios_retry_rewarded()
	}

	fn show_rewarded_ios() bool {
		return C.velo_admob_ios_show_rewarded() == 1
	}

	fn poll_rewarded_ios() AdResult {
		return match C.velo_admob_ios_poll_rewarded() {
			1 { AdResult.rewarded }
			2 { AdResult.closed }
			3 { AdResult.failed }
			else { AdResult.none }
		}
	}

	fn is_interstitial_ready_ios() bool {
		return C.velo_admob_ios_is_interstitial_ready() == 1
	}

	fn retry_interstitial_ios() {
		C.velo_admob_ios_retry_interstitial()
	}

	fn show_interstitial_ios() bool {
		return C.velo_admob_ios_show_interstitial() == 1
	}

	fn poll_interstitial_ios() AdResult {
		return match C.velo_admob_ios_poll_interstitial() {
			1 { AdResult.closed }
			2 { AdResult.closed }
			3 { AdResult.failed }
			else { AdResult.none }
		}
	}
} $else {
	fn init_ios(cfg Config) {}
	fn is_rewarded_ready_ios() bool {
		return false
	}
	fn retry_rewarded_ios() {}
	fn show_rewarded_ios() bool {
		return false
	}
	fn poll_rewarded_ios() AdResult {
		return .none
	}
	fn is_interstitial_ready_ios() bool {
		return false
	}
	fn retry_interstitial_ios() {}
	fn show_interstitial_ios() bool {
		return false
	}
	fn poll_interstitial_ios() AdResult {
		return .none
	}
}
