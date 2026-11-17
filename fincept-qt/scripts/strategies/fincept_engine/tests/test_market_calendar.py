from datetime import date, datetime, time

import pytest

from fincept_engine import market_calendar as cal


def test_holidays_2023():
    assert sorted(cal.nyse_holidays(2023)) == [
        date(2023, 1, 2), date(2023, 1, 16), date(2023, 2, 20), date(2023, 4, 7), date(2023, 5, 29),
        date(2023, 6, 19), date(2023, 7, 4), date(2023, 9, 4), date(2023, 11, 23), date(2023, 12, 25)]


def test_holidays_2022_new_year_on_saturday_not_observed_and_juneteenth_observed_monday():
    hs = cal.nyse_holidays(2022)
    assert date(2021, 12, 31) not in cal.nyse_holidays(2021)
    assert cal.is_trading_day(date(2021, 12, 31))
    assert sorted(hs) == [
        date(2022, 1, 17), date(2022, 2, 21), date(2022, 4, 15), date(2022, 5, 30), date(2022, 6, 20),
        date(2022, 7, 4), date(2022, 9, 5), date(2022, 11, 24), date(2022, 12, 26)]


def test_holidays_2021_no_juneteenth_and_weekend_observance():
    hs = cal.nyse_holidays(2021)
    assert date(2021, 6, 18) not in hs and date(2021, 6, 21) not in hs
    assert date(2021, 7, 5) in hs          # July 4 on Sunday
    assert date(2021, 12, 24) in hs        # Christmas on Saturday
    assert date(2021, 4, 2) in hs          # Good Friday


def test_saturday_holidays_move_to_friday():
    assert cal.is_holiday(date(2020, 7, 3))    # July 4 2020 was a Saturday
    assert cal.is_holiday(date(2027, 6, 18))   # Juneteenth 2027 is a Saturday
    assert cal.is_holiday(date(2028, 6, 19))   # Juneteenth 2028 Monday


def test_special_closures():
    for d in (date(2001, 9, 11), date(2012, 10, 29), date(2012, 10, 30), date(2018, 12, 5), date(2025, 1, 9)):
        assert not cal.is_trading_day(d)


def test_good_friday_and_easter():
    assert cal.easter_sunday(2019) == date(2019, 4, 21)
    assert cal.good_friday(2022) == date(2022, 4, 15)
    assert cal.good_friday(2024) == date(2024, 3, 29)
    assert cal.good_friday(2025) == date(2025, 4, 18)
    assert cal.good_friday(2008) == date(2008, 3, 21)


def test_third_friday_and_nth_weekday():
    assert cal.third_friday(2022, 4) == date(2022, 4, 15)
    assert cal.third_friday(2023, 1) == date(2023, 1, 20)
    assert cal.third_friday(2015, 12) == date(2015, 12, 18)
    assert cal.nth_weekday(2023, 5, cal.MONDAY, -1) == date(2023, 5, 29)
    assert cal.nth_weekday(2023, 11, cal.THURSDAY, 4) == date(2023, 11, 23)
    with pytest.raises(ValueError):
        cal.nth_weekday(2023, 2, cal.MONDAY, 5)


def test_previous_next_trading_day_types_and_values():
    assert cal.previous_trading_day(date(2022, 4, 18)) == date(2022, 4, 14)       # over Good Friday + weekend
    assert cal.next_trading_day(date(2022, 4, 14)) == date(2022, 4, 18)
    assert cal.previous_trading_day(datetime(2022, 4, 15, 12), inclusive=True) == datetime(2022, 4, 14)
    assert cal.next_trading_day(date(2023, 7, 3), inclusive=True) == date(2023, 7, 3)
    assert cal.add_trading_days(date(2023, 12, 22), 1) == date(2023, 12, 26)
    assert cal.add_trading_days(date(2023, 12, 26), -1) == date(2023, 12, 22)


def test_business_days_of_month():
    assert cal.last_business_day(2023, 12) == date(2023, 12, 29)
    assert cal.last_business_day(2024, 3) == date(2024, 3, 28)        # March 29 2024 was Good Friday
    assert cal.nth_last_business_day(2023, 11, 4) == date(2023, 11, 27)
    assert cal.nth_business_day(2023, 1, 1) == date(2023, 1, 3)
    assert len(cal.trading_days(date(2023, 1, 1), date(2023, 12, 31))) == 250


def test_early_closes():
    assert cal.early_close(date(2023, 7, 3)) == time(13, 0)
    assert cal.early_close(date(2023, 11, 24)) == time(13, 0)
    assert cal.early_close(date(2024, 12, 24)) == time(13, 0)
    assert cal.early_close(date(2023, 7, 5)) is None
    assert cal.market_close_time(date(2023, 7, 4)) is None
    assert cal.market_close_time(date(2023, 7, 5)) == time(16, 0)
