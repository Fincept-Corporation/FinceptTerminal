import yfinance as yf

da=yf.Ticker("YOYOW-KRW").info
print(da)