package io.velo.admob;

import android.app.Activity;
import android.util.Log;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.OnUserEarnedRewardListener;
import com.google.android.gms.ads.initialization.InitializationStatus;
import com.google.android.gms.ads.initialization.OnInitializationCompleteListener;
import com.google.android.gms.ads.interstitial.InterstitialAd;
import com.google.android.gms.ads.interstitial.InterstitialAdLoadCallback;
import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;

/**
 * AdBridge — Google AdMob integration for V and Velo on Android.
 * Communicates with native V code through JNI (admob_android.h).
 * Native game loop never waits: shows hop to the Android UI thread,
 * and poll checks thread-safe volatile flags.
 */
public final class AdBridge {
	private static final String TAG = "VeloAdMob";

	public static final int RESULT_NONE = 0;
	public static final int RESULT_REWARDED = 1;
	public static final int RESULT_CLOSED = 2;
	public static final int RESULT_FAILED = 3;

	// Rewarded Ad state
	private static volatile boolean rewardedReady;
	private static volatile boolean rewardedLoading;
	private static volatile int rewardedResult = RESULT_NONE;
	private static RewardedAd rewardedAd;
	private static String rewardedUnitId = "";
	private static boolean rewardedEarned;

	// Interstitial Ad state
	private static volatile boolean interstitialReady;
	private static volatile boolean interstitialLoading;
	private static volatile int interstitialResult = RESULT_NONE;
	private static InterstitialAd interstitialAd;
	private static String interstitialUnitId = "";

	private static boolean initialized;

	private AdBridge() {}

	public static void init(final Activity activity, final String rewardedId) {
		init(activity, rewardedId, "");
	}

	public static void init(final Activity activity, final String rewardedId, final String interstitialId) {
		rewardedUnitId = (rewardedId != null) ? rewardedId.trim() : "";
		interstitialUnitId = (interstitialId != null) ? interstitialId.trim() : "";

		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (initialized) {
					if (!rewardedUnitId.isEmpty() && !rewardedReady && !rewardedLoading) {
						loadRewarded(activity);
					}
					if (!interstitialUnitId.isEmpty() && !interstitialReady && !interstitialLoading) {
						loadInterstitial(activity);
					}
					return;
				}
				initialized = true;
				MobileAds.initialize(activity, new OnInitializationCompleteListener() {
					@Override
					public void onInitializationComplete(InitializationStatus status) {
						Log.i(TAG, "AdMob initialized on Android");
						if (!rewardedUnitId.isEmpty()) {
							loadRewarded(activity);
						}
						if (!interstitialUnitId.isEmpty()) {
							loadInterstitial(activity);
						}
					}
				});
			}
		});
	}

	// ---------- Rewarded Ads ----------

	private static void loadRewarded(final Activity activity) {
		if (rewardedLoading || rewardedReady || rewardedUnitId.isEmpty()) {
			return;
		}
		rewardedLoading = true;
		RewardedAd.load(activity, rewardedUnitId, new AdRequest.Builder().build(), new RewardedAdLoadCallback() {
			@Override
			public void onAdLoaded(RewardedAd loaded) {
				Log.i(TAG, "Rewarded ad loaded");
				rewardedAd = loaded;
				rewardedLoading = false;
				rewardedReady = true;
			}

			@Override
			public void onAdFailedToLoad(LoadAdError error) {
				Log.w(TAG, "Rewarded ad failed to load: " + error.getMessage());
				rewardedAd = null;
				rewardedLoading = false;
				rewardedReady = false;
			}
		});
	}

	public static boolean readyRewarded() {
		return rewardedReady;
	}

	public static boolean ready() {
		return readyRewarded();
	}

	public static void retryRewarded(final Activity activity) {
		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (initialized) {
					loadRewarded(activity);
				}
			}
		});
	}

	public static void retry(final Activity activity) {
		retryRewarded(activity);
	}

	public static void showRewarded(final Activity activity) {
		rewardedResult = RESULT_NONE;
		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				final RewardedAd shown = rewardedAd;
				if (shown == null) {
					rewardedResult = RESULT_FAILED;
					loadRewarded(activity);
					return;
				}
				rewardedAd = null;
				rewardedReady = false;
				rewardedEarned = false;

				shown.setFullScreenContentCallback(new FullScreenContentCallback() {
					@Override
					public void onAdDismissedFullScreenContent() {
						rewardedResult = rewardedEarned ? RESULT_REWARDED : RESULT_CLOSED;
						loadRewarded(activity);
					}

					@Override
					public void onAdFailedToShowFullScreenContent(AdError error) {
						Log.w(TAG, "Rewarded ad failed to show: " + error.getMessage());
						rewardedResult = RESULT_FAILED;
						loadRewarded(activity);
					}
				});

				shown.show(activity, new OnUserEarnedRewardListener() {
					@Override
					public void onUserEarnedReward(RewardItem item) {
						rewardedEarned = true;
					}
				});
			}
		});
	}

	public static void show(final Activity activity) {
		showRewarded(activity);
	}

	public static int pollRewardedResult() {
		int r = rewardedResult;
		if (r != RESULT_NONE) {
			rewardedResult = RESULT_NONE;
		}
		return r;
	}

	public static int pollResult() {
		return pollRewardedResult();
	}

	// ---------- Interstitial Ads ----------

	private static void loadInterstitial(final Activity activity) {
		if (interstitialLoading || interstitialReady || interstitialUnitId.isEmpty()) {
			return;
		}
		interstitialLoading = true;
		InterstitialAd.load(activity, interstitialUnitId, new AdRequest.Builder().build(), new InterstitialAdLoadCallback() {
			@Override
			public void onAdLoaded(InterstitialAd loaded) {
				Log.i(TAG, "Interstitial ad loaded");
				interstitialAd = loaded;
				interstitialLoading = false;
				interstitialReady = true;
			}

			@Override
			public void onAdFailedToLoad(LoadAdError error) {
				Log.w(TAG, "Interstitial ad failed to load: " + error.getMessage());
				interstitialAd = null;
				interstitialLoading = false;
				interstitialReady = false;
			}
		});
	}

	public static boolean readyInterstitial() {
		return interstitialReady;
	}

	public static void retryInterstitial(final Activity activity) {
		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				if (initialized) {
					loadInterstitial(activity);
				}
			}
		});
	}

	public static void showInterstitial(final Activity activity) {
		interstitialResult = RESULT_NONE;
		activity.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				final InterstitialAd shown = interstitialAd;
				if (shown == null) {
					interstitialResult = RESULT_FAILED;
					loadInterstitial(activity);
					return;
				}
				interstitialAd = null;
				interstitialReady = false;

				shown.setFullScreenContentCallback(new FullScreenContentCallback() {
					@Override
					public void onAdDismissedFullScreenContent() {
						interstitialResult = RESULT_CLOSED;
						loadInterstitial(activity);
					}

					@Override
					public void onAdFailedToShowFullScreenContent(AdError error) {
						Log.w(TAG, "Interstitial ad failed to show: " + error.getMessage());
						interstitialResult = RESULT_FAILED;
						loadInterstitial(activity);
					}
				});

				shown.show(activity);
			}
		});
	}

	public static int pollInterstitialResult() {
		int r = interstitialResult;
		if (r != RESULT_NONE) {
			interstitialResult = RESULT_NONE;
		}
		return r;
	}
}
