import yfinance as yf

d=yf.Ticker("RITES.NS").history(period='1mo')
print(d)