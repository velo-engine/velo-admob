module admob

fn test_admob_init_and_config() {
	cfg := Config{
		app_id_android:            'ca-app-pub-test~android'
		app_id_ios:                'ca-app-pub-test~ios'
		rewarded_unit_android:     'ca-app-pub-test/rewarded-android'
		rewarded_unit_ios:         'ca-app-pub-test/rewarded-ios'
		interstitial_unit_android: 'ca-app-pub-test/interstitial-android'
		interstitial_unit_ios:     'ca-app-pub-test/interstitial-ios'
	}

	init(cfg)

	assert is_initialized()
	c := config()
	assert c.app_id_android == 'ca-app-pub-test~android'
	assert c.app_id_ios == 'ca-app-pub-test~ios'
	assert c.rewarded_unit_android == 'ca-app-pub-test/rewarded-android'
	assert c.rewarded_unit_ios == 'ca-app-pub-test/rewarded-ios'
	assert c.interstitial_unit_android == 'ca-app-pub-test/interstitial-android'
	assert c.interstitial_unit_ios == 'ca-app-pub-test/interstitial-ios'
}

fn test_mock_rewarded_ad_flow() {
	init_rewarded('test-rewarded-unit')
	assert is_initialized()

	// In desktop/test environment, ads are mocked
	assert is_mock()
	assert is_rewarded_ready()

	// Initially no poll result
	assert poll_rewarded() == .none

	// Show ad
	shown := show_rewarded()
	assert shown

	// Poll should return rewarded once
	res := poll_rewarded()
	assert res == .rewarded

	// Next poll should return none
	assert poll_rewarded() == .none
}

fn test_mock_interstitial_ad_flow() {
	init(Config{
		interstitial_unit_android: 'test-int-unit'
		interstitial_unit_ios:     'test-int-unit'
	})

	assert is_interstitial_ready()
	assert poll_interstitial() == .none

	shown := show_interstitial()
	assert shown

	res := poll_interstitial()
	assert res == .closed

	assert poll_interstitial() == .none
}
