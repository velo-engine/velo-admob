module admob

// AdResult represents the result of showing an ad.
pub enum AdResult {
	none     // no ad running, or currently playing on screen
	rewarded // watched to the end: reward earned
	closed   // closed or dismissed before earning reward
	failed   // ad could not be loaded or presented
}

// AdType specifies the category of ad format.
pub enum AdType {
	rewarded
	interstitial
}

// Config holds the Google AdMob identifiers for Android and iOS.
pub struct Config {
pub:
	app_id_android            string
	app_id_ios                string
	rewarded_unit_android     string
	rewarded_unit_ios         string
	interstitial_unit_android string
	interstitial_unit_ios     string
}

// AdMobManager manages the AdMob lifecycle and state.
pub struct AdMobManager {
pub mut:
	initialized            bool
	cfg                    Config
	mock_rewarded_pending   bool
	mock_interstitial_done  bool
}

const default_admob = &AdMobManager{}

// get returns the shared AdMobManager singleton.
pub fn get() &AdMobManager {
	return default_admob
}

// init initializes the AdMob SDK with platform-specific IDs.
pub fn init(cfg Config) {
	mut s := get()
	s.cfg = cfg
	s.initialized = true

	$if android {
		init_android(cfg)
	} $else $if ios {
		init_ios(cfg)
	} $else {
		// Desktop / WebGL / bot mock mode
		s.mock_rewarded_pending = false
		s.mock_interstitial_done = false
	}
}

// init_rewarded is a convenience initializer when only a rewarded ad unit is needed.
pub fn init_rewarded(rewarded_unit string) {
	init(Config{
		rewarded_unit_android: rewarded_unit
		rewarded_unit_ios:     rewarded_unit
	})
}

// config returns the current AdMob configuration.
pub fn config() Config {
	return get().cfg
}

// is_initialized returns true if init has been called.
pub fn is_initialized() bool {
	return get().initialized
}

// is_mock reports whether ads are mocked on the current platform (e.g. desktop).
pub fn is_mock() bool {
	$if android || ios {
		return false
	}
	return true
}

// is_rewarded_ready checks if a rewarded ad is loaded and ready to be displayed.
pub fn is_rewarded_ready() bool {
	$if android {
		return is_rewarded_ready_android()
	} $else $if ios {
		return is_rewarded_ready_ios()
	} $else {
		return true
	}
}

// is_interstitial_ready checks if an interstitial ad is loaded and ready.
pub fn is_interstitial_ready() bool {
	$if android {
		return is_interstitial_ready_android()
	} $else $if ios {
		return is_interstitial_ready_ios()
	} $else {
		return true
	}
}

// retry_rewarded requests loading a rewarded ad if previous load failed or wasn't loaded.
pub fn retry_rewarded() {
	$if android {
		retry_rewarded_android()
	} $else $if ios {
		retry_rewarded_ios()
	}
}

// retry_interstitial requests loading an interstitial ad.
pub fn retry_interstitial() {
	$if android {
		retry_interstitial_android()
	} $else $if ios {
		retry_interstitial_ios()
	}
}

// show_rewarded starts showing a rewarded video ad.
// Returns false if no ad is ready to show.
pub fn show_rewarded() bool {
	if !is_rewarded_ready() {
		retry_rewarded()
		return false
	}

	$if android {
		return show_rewarded_android()
	} $else $if ios {
		return show_rewarded_ios()
	} $else {
		mut s := get()
		s.mock_rewarded_pending = true
		return true
	}
}

// show_interstitial starts showing an interstitial ad.
// Returns false if no ad is ready to show.
pub fn show_interstitial() bool {
	if !is_interstitial_ready() {
		retry_interstitial()
		return false
	}

	$if android {
		return show_interstitial_android()
	} $else $if ios {
		return show_interstitial_ios()
	} $else {
		mut s := get()
		s.mock_interstitial_done = true
		return true
	}
}

// poll_rewarded checks the outcome of the most recently displayed rewarded ad.
// Returns .none while playing or idle, .rewarded if finished, .closed if closed early, .failed on error.
pub fn poll_rewarded() AdResult {
	$if android {
		return poll_rewarded_android()
	} $else $if ios {
		return poll_rewarded_ios()
	} $else {
		mut s := get()
		if s.mock_rewarded_pending {
			s.mock_rewarded_pending = false
			return .rewarded
		}
		return .none
	}
}

// poll_interstitial checks the outcome of the most recently displayed interstitial ad.
// Returns .none while playing or idle, .closed when dismissed, .failed on error.
pub fn poll_interstitial() AdResult {
	$if android {
		return poll_interstitial_android()
	} $else $if ios {
		return poll_interstitial_ios()
	} $else {
		mut s := get()
		if s.mock_interstitial_done {
			s.mock_interstitial_done = false
			return .closed
		}
		return .none
	}
}
